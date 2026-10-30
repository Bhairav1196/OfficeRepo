/**
 * @file test_master_rate_limit.c
 * @brief Verify a custom ACL request-rate ceiling is enforced per master.
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

#define TEST_CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

typedef struct
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_Manager;
    atomic_bool *pb_StopRequested;
    atomic_bool *pb_ReloadRequested;
    E_GW_RESULT e_Result;
} S_TEST_SERVER_ARGUMENTS;

static void fn_SleepMs(const uint32_t ui32_Milliseconds)
{
    struct timespec st_Delay;
    st_Delay.tv_sec = (time_t)(ui32_Milliseconds / 1000U);
    st_Delay.tv_nsec = (long)((ui32_Milliseconds % 1000U) * 1000000U);
    (void)nanosleep(&st_Delay, NULL);
}

static bool fn_WriteText(const char *pc_Path, const char *pc_Text)
{
    const int i32_File = open(pc_Path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    const size_t sz_Length = strlen(pc_Text);
    ssize_t sz_Written;
    if (i32_File < 0) return false;
    sz_Written = write(i32_File, pc_Text, sz_Length);
    if (close(i32_File) != 0) return false;
    return sz_Written == (ssize_t)sz_Length;
}

static bool fn_SendAll(const int i32_Socket, const uint8_t *pui8_Data,
                       const size_t sz_Length)
{
    size_t sz_Offset = 0U;
    while (sz_Offset < sz_Length)
    {
        const ssize_t sz_Sent = send(i32_Socket, &pui8_Data[sz_Offset],
                                     sz_Length - sz_Offset, 0);
        if (sz_Sent > 0) sz_Offset += (size_t)sz_Sent;
        else if ((sz_Sent < 0) && (errno == EINTR)) continue;
        else return false;
    }
    return true;
}

static bool fn_ReceiveAll(const int i32_Socket, uint8_t *pui8_Data,
                          const size_t sz_Length)
{
    size_t sz_Offset = 0U;
    while (sz_Offset < sz_Length)
    {
        const ssize_t sz_Received = recv(i32_Socket, &pui8_Data[sz_Offset],
                                         sz_Length - sz_Offset, 0);
        if (sz_Received > 0) sz_Offset += (size_t)sz_Received;
        else if ((sz_Received < 0) && (errno == EINTR)) continue;
        else return false;
    }
    return true;
}

static bool fn_Poll(const int i32_Socket, const uint16_t ui16_Transaction)
{
    uint8_t aui8_Request[12] =
    {
        (uint8_t)(ui16_Transaction >> 8U),
        (uint8_t)(ui16_Transaction & UINT16_C(0x00FF)),
        0U, 0U, 0U, 6U, 1U, 2U, 0U, 0U, 0U, 16U
    };
    uint8_t aui8_Response[9];
    return fn_SendAll(i32_Socket, aui8_Request, sizeof(aui8_Request)) &&
           fn_ReceiveAll(i32_Socket, aui8_Response, sizeof(aui8_Response)) &&
           (aui8_Response[0] == aui8_Request[0]) &&
           (aui8_Response[1] == aui8_Request[1]) &&
           (aui8_Response[6] == 1U) && (aui8_Response[7] == 0x82U);
}

static uint16_t fn_FindFreePort(void)
{
    struct sockaddr_in st_Address;
    socklen_t sz_Length = (socklen_t)sizeof(st_Address);
    const int i32_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    uint16_t ui16_Port = 0U;
    if (i32_Socket < 0) return 0U;
    (void)memset(&st_Address, 0, sizeof(st_Address));
    st_Address.sin_family = AF_INET;
    st_Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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

static int fn_ConnectFrom(const uint16_t ui16_Port, const char *pc_SourceIp)
{
    uint32_t ui32_Attempt;
    for (ui32_Attempt = 0U; ui32_Attempt < 100U; ++ui32_Attempt)
    {
        struct sockaddr_in st_Local;
        struct sockaddr_in st_Server;
        struct timeval st_Timeout;
        const int i32_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (i32_Socket < 0) return -1;
        st_Timeout.tv_sec = 0;
        st_Timeout.tv_usec = 300000;
        (void)setsockopt(i32_Socket, SOL_SOCKET, SO_RCVTIMEO,
                         &st_Timeout, (socklen_t)sizeof(st_Timeout));
        (void)memset(&st_Local, 0, sizeof(st_Local));
        st_Local.sin_family = AF_INET;
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

static void *fn_ServerThread(void *pv_Argument)
{
    S_TEST_SERVER_ARGUMENTS *pst_Arguments =
        (S_TEST_SERVER_ARGUMENTS *)pv_Argument;
    pst_Arguments->e_Result = fn_MODB_ServerRun(
        &pst_Arguments->st_Config, pst_Arguments->pst_Manager,
        pst_Arguments->pb_StopRequested, pst_Arguments->pb_ReloadRequested);
    return NULL;
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
    char ac_AclPath[] = "/tmp/ek9500-rate-acl-XXXXXX";
    const uint16_t ui16_Port = fn_FindFreePort();
    int i32_AclFile;
    int i32_Client;

    TEST_CHECK(ui16_Port != 0U);
    i32_AclFile = mkstemp(ac_AclPath);
    TEST_CHECK(i32_AclFile >= 0);
    TEST_CHECK(close(i32_AclFile) == 0);
    TEST_CHECK(fn_WriteText(ac_AclPath, "STM32|127.0.0.5|1|3\n"));

    TEST_CHECK(fn_GW_ConfigLoad(TEST_GATEWAY_CONFIG, &st_Config,
                                ac_Error, sizeof(ac_Error)) == GW_OK);
    (void)snprintf(st_Config.ac_BindIp, sizeof(st_Config.ac_BindIp), "%s",
                   "127.0.0.1");
    (void)snprintf(st_Config.ac_MasterAclPath,
                   sizeof(st_Config.ac_MasterAclPath), "%s", ac_AclPath);
    st_Config.ui16_Port = ui16_Port;
    st_Config.ui16_SecondaryPort = 0U;
    st_Config.ui8_MaxClients = 2U;
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

    i32_Client = fn_ConnectFrom(ui16_Port, "127.0.0.5");
    TEST_CHECK(i32_Client >= 0);
    TEST_CHECK(fn_Poll(i32_Client, 1U));
    TEST_CHECK(fn_Poll(i32_Client, 2U));
    TEST_CHECK(fn_Poll(i32_Client, 3U));
    TEST_CHECK(fn_Poll(i32_Client, 4U)); /* busy response, persistent connection */

    (void)close(i32_Client);
    atomic_store_explicit(&b_StopRequested, true, memory_order_relaxed);
    (void)pthread_join(st_ServerThread, NULL);
    TEST_CHECK(st_ServerArguments.e_Result == GW_OK);
    fn_GW_RouteManagerDestroy(&pst_Manager);
    TEST_CHECK(unlink(ac_AclPath) == 0);
    (void)puts("Per-master request-rate limit test passed");
    return 0;
}
