/**
 * @file test_modbus_multi_client.c
 * @brief Loopback stress test for simultaneous Modbus TCP masters.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "gateway/gateway_config.h"
#include "gateway/route_manager.h"
#include "modbus_tcp/modbus_tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEST_CLIENT_COUNT (8U)
#define TEST_ITERATIONS    (100U)

#define TEST_CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

typedef struct
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_Manager;
    atomic_bool *pb_StopRequested;
    atomic_bool *pb_ReloadMastersRequested;
    E_GW_RESULT e_Result;
} S_TEST_SERVER_ARGUMENTS;

typedef struct
{
    uint16_t ui16_Port;
    uint8_t ui8_UnitId;
    atomic_uint *pui_Failures;
} S_TEST_CLIENT_ARGUMENTS;

static void fn_SleepMs(uint32_t ui32_Milliseconds)
{
    struct timespec st_Delay;
    st_Delay.tv_sec = (time_t)(ui32_Milliseconds / 1000U);
    st_Delay.tv_nsec = (long)((ui32_Milliseconds % 1000U) * 1000000U);
    (void)nanosleep(&st_Delay, NULL);
}

static bool fn_SendAll(int i32_Socket, const uint8_t *pui8_Data, size_t sz_Length)
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

static bool fn_ReceiveAll(int i32_Socket, uint8_t *pui8_Data, size_t sz_Length)
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

static int fn_Connect(uint16_t ui16_Port)
{
    uint32_t ui32_Attempt;
    for (ui32_Attempt = 0U; ui32_Attempt < 100U; ++ui32_Attempt)
    {
        struct sockaddr_in st_Address;
        const int i32_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (i32_Socket < 0)
        {
            return -1;
        }
        (void)memset(&st_Address, 0, sizeof(st_Address));
        st_Address.sin_family = AF_INET;
        st_Address.sin_port = htons(ui16_Port);
        st_Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(i32_Socket, (const struct sockaddr *)&st_Address,
                    (socklen_t)sizeof(st_Address)) == 0)
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
        pst_Arguments->pb_StopRequested,
        pst_Arguments->pb_ReloadMastersRequested);
    return NULL;
}

static void *fn_ClientThread(void *pv_Argument)
{
    S_TEST_CLIENT_ARGUMENTS *pst_Arguments =
        (S_TEST_CLIENT_ARGUMENTS *)pv_Argument;
    uint8_t aui8_Request[12];
    uint8_t aui8_Response[9];
    uint32_t ui32_Iteration;
    const int i32_Socket = fn_Connect(pst_Arguments->ui16_Port);
    if (i32_Socket < 0)
    {
        (void)atomic_fetch_add_explicit(pst_Arguments->pui_Failures, 1U,
                                        memory_order_relaxed);
        return NULL;
    }
    for (ui32_Iteration = 0U; ui32_Iteration < TEST_ITERATIONS; ++ui32_Iteration)
    {
        const uint16_t ui16_Transaction = (uint16_t)(ui32_Iteration + 1U);
        aui8_Request[0] = (uint8_t)(ui16_Transaction >> 8U);
        aui8_Request[1] = (uint8_t)(ui16_Transaction & UINT16_C(0x00FF));
        aui8_Request[2] = 0U;
        aui8_Request[3] = 0U;
        aui8_Request[4] = 0U;
        aui8_Request[5] = 6U;
        aui8_Request[6] = pst_Arguments->ui8_UnitId;
        aui8_Request[7] = 2U;
        aui8_Request[8] = 0U;
        aui8_Request[9] = 0U;
        aui8_Request[10] = 0U;
        aui8_Request[11] = 16U;
        if ((!fn_SendAll(i32_Socket, aui8_Request, sizeof(aui8_Request))) ||
            (!fn_ReceiveAll(i32_Socket, aui8_Response, sizeof(aui8_Response))) ||
            (aui8_Response[0] != aui8_Request[0]) ||
            (aui8_Response[1] != aui8_Request[1]) ||
            (aui8_Response[6] != pst_Arguments->ui8_UnitId) ||
            (aui8_Response[7] != 0x82U) || (aui8_Response[8] != 6U))
        {
            (void)atomic_fetch_add_explicit(pst_Arguments->pui_Failures, 1U,
                                            memory_order_relaxed);
            break;
        }
    }
    (void)close(i32_Socket);
    return NULL;
}

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
    S_TEST_CLIENT_ARGUMENTS ast_ClientArguments[TEST_CLIENT_COUNT];
    pthread_t st_ServerThread;
    pthread_t ast_ClientThreads[TEST_CLIENT_COUNT];
    atomic_bool b_StopRequested = ATOMIC_VAR_INIT(false);
    atomic_bool b_ReloadMastersRequested = ATOMIC_VAR_INIT(false);
    atomic_uint ui_Failures = ATOMIC_VAR_INIT(0U);
    char ac_Error[512];
    const uint16_t ui16_Port = fn_FindFreePort();
    uint8_t ui8_Index;

    TEST_CHECK(ui16_Port != 0U);
    TEST_CHECK(fn_GW_ConfigLoad(TEST_GATEWAY_CONFIG, &st_Config,
                                ac_Error, sizeof(ac_Error)) == GW_OK);
    (void)snprintf(st_Config.ac_BindIp, sizeof(st_Config.ac_BindIp), "%s",
                   "127.0.0.1");
    st_Config.ui16_Port = ui16_Port;
    st_Config.ui16_SecondaryPort = 0U;
    st_Config.ui8_MaxClients = TEST_CLIENT_COUNT;
    st_Config.ui32_MaxRequestsPerSecondPerClient = 1000U;
    TEST_CHECK(fn_GW_RouteManagerCreate(&st_Config, &pst_Manager,
                                        ac_Error, sizeof(ac_Error)) == GW_OK);

    st_ServerArguments.st_Config = st_Config;
    st_ServerArguments.pst_Manager = pst_Manager;
    st_ServerArguments.pb_StopRequested = &b_StopRequested;
    st_ServerArguments.pb_ReloadMastersRequested =
        &b_ReloadMastersRequested;
    st_ServerArguments.e_Result = GW_ERR_STATE;
    TEST_CHECK(pthread_create(&st_ServerThread, NULL, fn_ServerThread,
                              &st_ServerArguments) == 0);

    for (ui8_Index = 0U; ui8_Index < TEST_CLIENT_COUNT; ++ui8_Index)
    {
        ast_ClientArguments[ui8_Index].ui16_Port = ui16_Port;
        ast_ClientArguments[ui8_Index].ui8_UnitId = 1U;
        ast_ClientArguments[ui8_Index].pui_Failures = &ui_Failures;
        TEST_CHECK(pthread_create(&ast_ClientThreads[ui8_Index], NULL,
                                  fn_ClientThread,
                                  &ast_ClientArguments[ui8_Index]) == 0);
    }
    for (ui8_Index = 0U; ui8_Index < TEST_CLIENT_COUNT; ++ui8_Index)
    {
        (void)pthread_join(ast_ClientThreads[ui8_Index], NULL);
    }
    atomic_store_explicit(&b_StopRequested, true, memory_order_relaxed);
    (void)pthread_join(st_ServerThread, NULL);

    TEST_CHECK(st_ServerArguments.e_Result == GW_OK);
    TEST_CHECK(atomic_load_explicit(&ui_Failures,
                                    memory_order_relaxed) == 0U);
    fn_GW_RouteManagerDestroy(&pst_Manager);
    (void)puts("Multi-client test passed: 8 masters x 100 polls on Unit ID 1");
    return 0;
}
