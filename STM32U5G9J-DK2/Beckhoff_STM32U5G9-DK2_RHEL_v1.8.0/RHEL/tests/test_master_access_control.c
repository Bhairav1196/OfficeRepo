/**
 * @file test_master_access_control.c
 * @brief Verify runtime add/remove control without stalling another master.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "gateway/gateway_config.h"
#include "gateway/route_manager.h"
#include "modbus_tcp/modbus_tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TEST_POLL_COUNT (100U)

#define TEST_CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

/** @brief Server-thread arguments. */
typedef struct
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_Manager;
    atomic_bool *pb_StopRequested;
    atomic_bool *pb_ReloadRequested;
    E_GW_RESULT e_Result;
} S_TEST_SERVER_ARGUMENTS;

/** @brief Sleep for a bounded number of milliseconds. */
static void fn_SleepMs(const uint32_t ui32_Milliseconds)
{
    struct timespec st_Delay;
    st_Delay.tv_sec = (time_t)(ui32_Milliseconds / 1000U);
    st_Delay.tv_nsec = (long)((ui32_Milliseconds % 1000U) * 1000000U);
    (void)nanosleep(&st_Delay, NULL);
}

/** @brief Return monotonic milliseconds for a coarse anti-stall assertion. */
static uint64_t fn_MonotonicMs(void)
{
    struct timespec st_Time;
    if (clock_gettime(CLOCK_MONOTONIC, &st_Time) != 0)
    {
        return UINT64_C(0);
    }
    return ((uint64_t)st_Time.tv_sec * UINT64_C(1000)) +
           ((uint64_t)st_Time.tv_nsec / UINT64_C(1000000));
}

/** @brief Replace the ACL test file atomically enough for the test process. */
static bool fn_WriteText(const char *pc_Path, const char *pc_Text)
{
    const int i32_File = open(pc_Path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    const size_t sz_Length = strlen(pc_Text);
    ssize_t sz_Written;

    if (i32_File < 0)
    {
        return false;
    }
    sz_Written = write(i32_File, pc_Text, sz_Length);
    if (close(i32_File) != 0)
    {
        return false;
    }
    return sz_Written == (ssize_t)sz_Length;
}

/** @brief Send all requested bytes on one blocking test socket. */
static bool fn_SendAll(const int i32_Socket,
                       const uint8_t *pui8_Data,
                       const size_t sz_Length)
{
    size_t sz_Offset = 0U;
    while (sz_Offset < sz_Length)
    {
        const ssize_t sz_Sent = send(i32_Socket, &pui8_Data[sz_Offset],
                                     sz_Length - sz_Offset, 0);
        if (sz_Sent > 0)
        {
            sz_Offset += (size_t)sz_Sent;
        }
        else if ((sz_Sent < 0) && (errno == EINTR))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/** @brief Receive an exact response length with a socket timeout. */
static bool fn_ReceiveAll(const int i32_Socket,
                          uint8_t *pui8_Data,
                          const size_t sz_Length)
{
    size_t sz_Offset = 0U;
    while (sz_Offset < sz_Length)
    {
        const ssize_t sz_Received = recv(i32_Socket, &pui8_Data[sz_Offset],
                                         sz_Length - sz_Offset, 0);
        if (sz_Received > 0)
        {
            sz_Offset += (size_t)sz_Received;
        }
        else if ((sz_Received < 0) && (errno == EINTR))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/** @brief Connect to the test listener using a selected loopback source IP. */
static int fn_ConnectFrom(const uint16_t ui16_Port, const char *pc_SourceIp)
{
    uint32_t ui32_Attempt;
    for (ui32_Attempt = 0U; ui32_Attempt < 100U; ++ui32_Attempt)
    {
        struct sockaddr_in st_Local;
        struct sockaddr_in st_Server;
        struct timeval st_Timeout;
        const int i32_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (i32_Socket < 0)
        {
            return -1;
        }
        st_Timeout.tv_sec = 1;
        st_Timeout.tv_usec = 0;
        (void)setsockopt(i32_Socket, SOL_SOCKET, SO_RCVTIMEO,
                         &st_Timeout, (socklen_t)sizeof(st_Timeout));
        (void)memset(&st_Local, 0, sizeof(st_Local));
        st_Local.sin_family = AF_INET;
        st_Local.sin_port = 0U;
        if ((inet_pton(AF_INET, pc_SourceIp, &st_Local.sin_addr) != 1) ||
            (bind(i32_Socket, (const struct sockaddr *)&st_Local,
                  (socklen_t)sizeof(st_Local)) != 0))
        {
            (void)close(i32_Socket);
            return -1;
        }
        (void)memset(&st_Server, 0, sizeof(st_Server));
        st_Server.sin_family = AF_INET;
        st_Server.sin_port = htons(ui16_Port);
        st_Server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(i32_Socket, (const struct sockaddr *)&st_Server,
                    (socklen_t)sizeof(st_Server)) == 0)
        {
            return i32_Socket;
        }
        (void)close(i32_Socket);
        fn_SleepMs(10U);
    }
    return -1;
}

/** @brief Execute one FC02 poll and validate transaction and Unit-ID routing. */
static bool fn_PollDiscreteInputs(const int i32_Socket,
                                  const uint16_t ui16_Transaction,
                                  const uint8_t ui8_UnitId)
{
    uint8_t aui8_Request[12];
    uint8_t aui8_Response[9];

    aui8_Request[0] = (uint8_t)(ui16_Transaction >> 8U);
    aui8_Request[1] = (uint8_t)(ui16_Transaction & UINT16_C(0x00FF));
    aui8_Request[2] = 0U;
    aui8_Request[3] = 0U;
    aui8_Request[4] = 0U;
    aui8_Request[5] = 6U;
    aui8_Request[6] = ui8_UnitId;
    aui8_Request[7] = 2U;
    aui8_Request[8] = 0U;
    aui8_Request[9] = 0U;
    aui8_Request[10] = 0U;
    aui8_Request[11] = 16U;

    return fn_SendAll(i32_Socket, aui8_Request, sizeof(aui8_Request)) &&
           fn_ReceiveAll(i32_Socket, aui8_Response, sizeof(aui8_Response)) &&
           (aui8_Response[0] == aui8_Request[0]) &&
           (aui8_Response[1] == aui8_Request[1]) &&
           (aui8_Response[6] == ui8_UnitId) &&
           (aui8_Response[7] == 0x82U) &&
           (aui8_Response[8] == 6U);
}

/** @brief Start the production server in a test-owned thread. */
static void *fn_ServerThread(void *pv_Argument)
{
    S_TEST_SERVER_ARGUMENTS *pst_Arguments =
        (S_TEST_SERVER_ARGUMENTS *)pv_Argument;
    pst_Arguments->e_Result = fn_MODB_ServerRun(
        &pst_Arguments->st_Config, pst_Arguments->pst_Manager,
        pst_Arguments->pb_StopRequested,
        pst_Arguments->pb_ReloadRequested);
    return NULL;
}

/** @brief Find an unused loopback TCP port. */
static uint16_t fn_FindFreePort(void)
{
    struct sockaddr_in st_Address;
    socklen_t sz_Length = (socklen_t)sizeof(st_Address);
    const int i32_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    uint16_t ui16_Port = 0U;
    if (i32_Socket < 0)
    {
        return 0U;
    }
    (void)memset(&st_Address, 0, sizeof(st_Address));
    st_Address.sin_family = AF_INET;
    st_Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    st_Address.sin_port = 0U;
    if ((bind(i32_Socket, (const struct sockaddr *)&st_Address,
              (socklen_t)sizeof(st_Address)) == 0) &&
        (getsockname(i32_Socket, (struct sockaddr *)&st_Address,
                     &sz_Length) == 0))
    {
        ui16_Port = ntohs(st_Address.sin_port);
    }
    (void)close(i32_Socket);
    return ui16_Port;
}

int main(void)
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_Manager = NULL;
    S_TEST_SERVER_ARGUMENTS st_ServerArguments;
    pthread_t st_ServerThread;
    atomic_bool b_StopRequested = ATOMIC_VAR_INIT(false);
    atomic_bool b_ReloadRequested = ATOMIC_VAR_INIT(false);
    char ac_Error[512];
    char ac_AclPath[] = "/tmp/ek9500-access-acl-XXXXXX";
    const uint16_t ui16_Port = fn_FindFreePort();
    int i32_AclFile;
    int i32_ModScan = -1;
    int i32_SlowHmi = -1;
    int i32_Unauthorized = -1;
    int i32_NewMaster = -1;
    uint8_t aui8_PartialRequest[6] = {0U, 1U, 0U, 0U, 0U, 6U};
    uint32_t ui32_Index;
    uint64_t ui64_Start;
    uint64_t ui64_Elapsed;

    TEST_CHECK(ui16_Port != 0U);
    i32_AclFile = mkstemp(ac_AclPath);
    TEST_CHECK(i32_AclFile >= 0);
    TEST_CHECK(close(i32_AclFile) == 0);
    TEST_CHECK(fn_WriteText(ac_AclPath,
        "ModScan|127.0.0.2|2\n"
        "Beijer|127.0.0.3|1\n"));

    TEST_CHECK(fn_GW_ConfigLoad(TEST_GATEWAY_CONFIG, &st_Config,
                                ac_Error, sizeof(ac_Error)) == GW_OK);
    (void)snprintf(st_Config.ac_BindIp, sizeof(st_Config.ac_BindIp), "%s",
                   "127.0.0.1");
    (void)snprintf(st_Config.ac_MasterAclPath,
                   sizeof(st_Config.ac_MasterAclPath), "%s", ac_AclPath);
    st_Config.ui16_Port = ui16_Port;
    st_Config.ui16_SecondaryPort = 0U;
    st_Config.ui8_MaxClients = 4U;
    st_Config.ui8_MaxRequestsPerEventPerClient = 2U;
    st_Config.ui32_MaxRequestsPerSecondPerClient = 1000U;
    TEST_CHECK(fn_GW_ConfigValidate(&st_Config,
                                    ac_Error, sizeof(ac_Error)) == GW_OK);
    TEST_CHECK(fn_GW_RouteManagerCreate(&st_Config, &pst_Manager,
                                        ac_Error, sizeof(ac_Error)) == GW_OK);

    st_ServerArguments.st_Config = st_Config;
    st_ServerArguments.pst_Manager = pst_Manager;
    st_ServerArguments.pb_StopRequested = &b_StopRequested;
    st_ServerArguments.pb_ReloadRequested = &b_ReloadRequested;
    st_ServerArguments.e_Result = GW_ERR_STATE;
    TEST_CHECK(pthread_create(&st_ServerThread, NULL, fn_ServerThread,
                              &st_ServerArguments) == 0);

    i32_ModScan = fn_ConnectFrom(ui16_Port, "127.0.0.2");
    TEST_CHECK(i32_ModScan >= 0);
    TEST_CHECK(fn_PollDiscreteInputs(i32_ModScan, 1U, 1U));

    i32_SlowHmi = fn_ConnectFrom(ui16_Port, "127.0.0.3");
    TEST_CHECK(i32_SlowHmi >= 0);
    TEST_CHECK(fn_SendAll(i32_SlowHmi, aui8_PartialRequest,
                          sizeof(aui8_PartialRequest)));

    ui64_Start = fn_MonotonicMs();
    for (ui32_Index = 0U; ui32_Index < TEST_POLL_COUNT; ++ui32_Index)
    {
        TEST_CHECK(fn_PollDiscreteInputs(i32_ModScan,
                                         (uint16_t)(ui32_Index + 2U), 1U));
    }
    ui64_Elapsed = fn_MonotonicMs() - ui64_Start;
    TEST_CHECK(ui64_Elapsed < UINT64_C(1000));

    i32_Unauthorized = fn_ConnectFrom(ui16_Port, "127.0.0.4");
    TEST_CHECK(i32_Unauthorized >= 0);
    TEST_CHECK(!fn_PollDiscreteInputs(i32_Unauthorized, 1U, 1U));
    (void)close(i32_Unauthorized);
    i32_Unauthorized = -1;

    TEST_CHECK(fn_WriteText(ac_AclPath,
        "ModScan|127.0.0.2|2\n"
        "NewMaster|127.0.0.4|1\n"));
    atomic_store_explicit(&b_ReloadRequested, true, memory_order_relaxed);
    fn_SleepMs(400U);

    TEST_CHECK(fn_PollDiscreteInputs(i32_ModScan, 200U, 1U));
    TEST_CHECK(!fn_PollDiscreteInputs(i32_SlowHmi, 2U, 1U));

    i32_NewMaster = fn_ConnectFrom(ui16_Port, "127.0.0.4");
    TEST_CHECK(i32_NewMaster >= 0);
    TEST_CHECK(fn_PollDiscreteInputs(i32_NewMaster, 1U, 1U));

    (void)close(i32_NewMaster);
    (void)close(i32_SlowHmi);
    (void)close(i32_ModScan);
    atomic_store_explicit(&b_StopRequested, true, memory_order_relaxed);
    (void)pthread_join(st_ServerThread, NULL);

    TEST_CHECK(st_ServerArguments.e_Result == GW_OK);
    fn_GW_RouteManagerDestroy(&pst_Manager);
    TEST_CHECK(unlink(ac_AclPath) == 0);
    (void)puts("Master access-control test passed: add/remove reload preserved ModScan polling");
    return 0;
}
