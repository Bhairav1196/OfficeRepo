/**
 * @file route_manager.c
 * @brief Direct, in-process EtherNet/IP scanner ownership for one EK9500 coupler.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "gateway/route_manager.h"

#include "ead/config.h"
#include "ead/eip_transport.h"
#include "ead/layout.h"
#include "ead/logger.h"
#include "ead/process_image.h"
#include "ead/terminal_manager.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** @brief Private state for one EK9500 route. */
typedef struct
{
    S_GW_ROUTE_CONFIG st_RouteConfig;
    ead_system_config_t st_EadConfig;
    ead_layout_t st_Layout;
    ead_process_image_t st_Image;
    ead_terminal_manager_t st_TerminalManager;
    ead_eip_transport_t *pst_Transport;
    pthread_mutex_t st_Mutex;
    bool b_MutexInitialized;
    pthread_t st_Thread;
    bool b_ThreadStarted;
    atomic_bool b_StopRequested;
    struct S_GW_ROUTE_MANAGER *pst_Owner;

    uint8_t aui8_DiValid[GW_MAX_DIGITAL_CHANNELS];
    uint8_t aui8_DoValid[GW_MAX_DIGITAL_CHANNELS];
    uint8_t aui8_AiValid[GW_MAX_ANALOG_CHANNELS];
    uint8_t aui8_AoValid[GW_MAX_ANALOG_CHANNELS];
    size_t sz_DiCount;
    size_t sz_DoCount;
    size_t sz_AiCount;
    size_t sz_AoCount;

    E_GW_ROUTE_STATE e_State;
    bool b_IoOpen;
    bool b_CyclicInputSeen;
    uint64_t ui64_AppliedOutputRevision;
    uint32_t ui32_CommunicationErrors;
    uint32_t ui32_ReconnectCount;
    uint32_t ui32_WriteErrors;
    uint32_t ui32_LastWriterIpv4;
    uint64_t ui64_LastWriteMs;
} S_GW_ROUTE;

/** @brief Private manager definition. */
struct S_GW_ROUTE_MANAGER
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE ast_Routes[GW_COUPLER_COUNT];
    pthread_mutex_t st_DiagnosticsMutex;
    bool b_DiagnosticsMutexInitialized;
    uint32_t ui32_ActiveClients;
    uint64_t ui64_Connections;
    uint64_t ui64_RejectedConnections;
    uint64_t ui64_Requests;
    uint64_t ui64_Exceptions;
};

static uint64_t fn_MonotonicMs(void)
{
    struct timespec st_Now;
    if (clock_gettime(CLOCK_MONOTONIC, &st_Now) != 0)
    {
        return 0U;
    }
    return ((uint64_t)st_Now.tv_sec * UINT64_C(1000)) +
           ((uint64_t)st_Now.tv_nsec / UINT64_C(1000000));
}

static void fn_SleepMs(uint32_t ui32_Milliseconds)
{
    struct timespec st_Delay;
    st_Delay.tv_sec = (time_t)(ui32_Milliseconds / 1000U);
    st_Delay.tv_nsec = (long)((ui32_Milliseconds % 1000U) * 1000000U);
    while ((nanosleep(&st_Delay, &st_Delay) != 0) && (errno == EINTR))
    {
        /* Retry the remaining interval. */
    }
}

static E_GW_RESULT fn_MapEad(ead_result_t e_Result)
{
    switch (e_Result)
    {
        case EAD_OK: return GW_OK;
        case EAD_ERR_ARGUMENT: return GW_ERR_ARGUMENT;
        case EAD_ERR_CONFIG: return GW_ERR_CONFIG;
        case EAD_ERR_RANGE: return GW_ERR_RANGE;
        case EAD_ERR_IO: return GW_ERR_IO;
        case EAD_ERR_PROTOCOL: return GW_ERR_PROTOCOL;
        case EAD_ERR_TIMEOUT: return GW_ERR_TIMEOUT;
        case EAD_ERR_STATE: return GW_ERR_STATE;
        case EAD_ERR_NOT_FOUND: return GW_ERR_NOT_FOUND;
        case EAD_ERR_REMOTE: return GW_ERR_REMOTE;
        default: return GW_ERR_REMOTE;
    }
}

static void fn_SetError(char *pc_Error,
                        size_t sz_ErrorSize,
                        const char *pc_Route,
                        const char *pc_Detail)
{
    if ((pc_Error != NULL) && (sz_ErrorSize > 0U))
    {
        (void)snprintf(pc_Error, sz_ErrorSize, "%s: %s",
                       (pc_Route != NULL) ? pc_Route : "gateway",
                       (pc_Detail != NULL) ? pc_Detail : "error");
    }
}

static S_GW_ROUTE *fn_FindRoute(S_GW_ROUTE_MANAGER *pst_Manager,
                                uint8_t ui8_UnitId)
{
    size_t sz_Index;
    if (pst_Manager == NULL)
    {
        return NULL;
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        S_GW_ROUTE *pst_Route = &pst_Manager->ast_Routes[sz_Index];
        if (pst_Route->st_RouteConfig.b_Enabled &&
            (pst_Route->st_RouteConfig.ui8_UnitId == ui8_UnitId))
        {
            return pst_Route;
        }
    }
    return NULL;
}

static E_GW_RESULT fn_BuildLogicalMap(S_GW_ROUTE *pst_Route)
{
    size_t sz_TerminalIndex;
    if (pst_Route == NULL)
    {
        return GW_ERR_ARGUMENT;
    }
    for (sz_TerminalIndex = 0U;
         sz_TerminalIndex < pst_Route->st_Layout.terminal_count;
         ++sz_TerminalIndex)
    {
        const ead_terminal_address_t *pst_Terminal =
            &pst_Route->st_Layout.terminals[sz_TerminalIndex];
        uint8_t *pui8_Valid = NULL;
        size_t *psz_Count = NULL;
        size_t sz_Capacity = 0U;
        size_t sz_Base;
        size_t sz_End;
        size_t sz_Channel;

        if ((!pst_Terminal->config.enabled) ||
            (pst_Terminal->descriptor.domain == EAD_DOMAIN_RAW))
        {
            continue;
        }
        if (pst_Terminal->config.logical_base == 0U)
        {
            return GW_ERR_CONFIG;
        }
        if (pst_Terminal->descriptor.domain == EAD_DOMAIN_DI)
        {
            pui8_Valid = pst_Route->aui8_DiValid;
            psz_Count = &pst_Route->sz_DiCount;
            sz_Capacity = GW_MAX_DIGITAL_CHANNELS;
        }
        else if (pst_Terminal->descriptor.domain == EAD_DOMAIN_DO)
        {
            pui8_Valid = pst_Route->aui8_DoValid;
            psz_Count = &pst_Route->sz_DoCount;
            sz_Capacity = GW_MAX_DIGITAL_CHANNELS;
        }
        else if (pst_Terminal->descriptor.domain == EAD_DOMAIN_AI)
        {
            pui8_Valid = pst_Route->aui8_AiValid;
            psz_Count = &pst_Route->sz_AiCount;
            sz_Capacity = GW_MAX_ANALOG_CHANNELS;
        }
        else if (pst_Terminal->descriptor.domain == EAD_DOMAIN_AO)
        {
            pui8_Valid = pst_Route->aui8_AoValid;
            psz_Count = &pst_Route->sz_AoCount;
            sz_Capacity = GW_MAX_ANALOG_CHANNELS;
        }
        else
        {
            continue;
        }
        sz_Base = pst_Terminal->config.logical_base - 1U;
        if ((sz_Base > sz_Capacity) ||
            (pst_Terminal->descriptor.channels > (sz_Capacity - sz_Base)))
        {
            return GW_ERR_RANGE;
        }
        sz_End = sz_Base + pst_Terminal->descriptor.channels;
        for (sz_Channel = sz_Base; sz_Channel < sz_End; ++sz_Channel)
        {
            pui8_Valid[sz_Channel] = 1U;
        }
        if (sz_End > *psz_Count)
        {
            *psz_Count = sz_End;
        }
    }
    return GW_OK;
}

static void fn_InputCallback(const uint8_t *pui8_Data,
                             size_t sz_Size,
                             uint32_t ui32_Sequence,
                             void *pv_Context)
{
    S_GW_ROUTE *pst_Route = (S_GW_ROUTE *)pv_Context;
    (void)ui32_Sequence;
    if ((pst_Route == NULL) || (pui8_Data == NULL))
    {
        return;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (fn_EadProcessImageReplaceInput(&pst_Route->st_Image,
                                       pui8_Data, sz_Size) == EAD_OK)
    {
        pst_Route->e_State = GW_ROUTE_ONLINE;
        pst_Route->b_CyclicInputSeen = true;
    }
    else
    {
        ++pst_Route->ui32_CommunicationErrors;
        pst_Route->e_State = GW_ROUTE_DEGRADED;
    }
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
}

static E_GW_RESULT fn_CheckAssemblySize(S_GW_ROUTE *pst_Route,
                                        uint16_t ui16_Assembly,
                                        size_t sz_Expected,
                                        const char *pc_Direction,
                                        uint8_t *pui8_Data,
                                        size_t *psz_Size,
                                        char *pc_Error,
                                        size_t sz_ErrorSize)
{
    ead_result_t e_Result = fn_EadEipReadAssembly(
        pst_Route->pst_Transport, ui16_Assembly, pui8_Data,
        EAD_MAX_ASSEMBLY_SIZE, psz_Size, pc_Error, sz_ErrorSize);
    if (e_Result != EAD_OK)
    {
        return fn_MapEad(e_Result);
    }
    if (*psz_Size != sz_Expected)
    {
        (void)snprintf(pc_Error, sz_ErrorSize,
                       "%s Assembly size mismatch: device=%zu configured=%zu",
                       pc_Direction, *psz_Size, sz_Expected);
        return GW_ERR_CONFIG;
    }
    return GW_OK;
}

static E_GW_RESULT fn_ConnectRoute(S_GW_ROUTE *pst_Route,
                                   char *pc_Error,
                                   size_t sz_ErrorSize)
{
    uint8_t aui8_Input[EAD_MAX_ASSEMBLY_SIZE];
    uint8_t aui8_Output[EAD_MAX_ASSEMBLY_SIZE];
    size_t sz_Input = 0U;
    size_t sz_Output = 0U;
    ead_result_t e_EadResult;
    E_GW_RESULT e_Result;

    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    pst_Route->e_State = GW_ROUTE_CONNECTING;
    pst_Route->b_IoOpen = false;
    pst_Route->b_CyclicInputSeen = false;
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);

    e_EadResult = fn_EadEipConnect(pst_Route->pst_Transport,
                                   &pst_Route->st_EadConfig.scanner,
                                   pst_Route->st_Layout.input_size,
                                   pst_Route->st_Layout.output_size,
                                   fn_InputCallback, pst_Route,
                                   pc_Error, sz_ErrorSize);
    if (e_EadResult != EAD_OK)
    {
        return fn_MapEad(e_EadResult);
    }

    e_Result = fn_CheckAssemblySize(
        pst_Route, pst_Route->st_EadConfig.scanner.input_assembly,
        pst_Route->st_Layout.input_size, "Input",
        aui8_Input, &sz_Input, pc_Error, sz_ErrorSize);
    if (e_Result != GW_OK)
    {
        fn_EadEipDisconnect(pst_Route->pst_Transport);
        return e_Result;
    }
    e_Result = fn_CheckAssemblySize(
        pst_Route, pst_Route->st_EadConfig.scanner.output_assembly,
        pst_Route->st_Layout.output_size, "Output",
        aui8_Output, &sz_Output, pc_Error, sz_ErrorSize);
    if (e_Result != GW_OK)
    {
        fn_EadEipDisconnect(pst_Route->pst_Transport);
        return e_Result;
    }

    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    e_EadResult = fn_EadProcessImageReplaceInput(&pst_Route->st_Image,
                                                  aui8_Input, sz_Input);
    if (e_EadResult == EAD_OK)
    {
        e_EadResult = fn_EadProcessImageReplaceOutput(&pst_Route->st_Image,
                                                       aui8_Output, sz_Output);
    }
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    if (e_EadResult != EAD_OK)
    {
        fn_EadEipDisconnect(pst_Route->pst_Transport);
        return fn_MapEad(e_EadResult);
    }

    e_EadResult = fn_EadEipOpenIo(pst_Route->pst_Transport,
                                  aui8_Output, sz_Output,
                                  pc_Error, sz_ErrorSize);
    if (e_EadResult != EAD_OK)
    {
        fn_EadEipDisconnect(pst_Route->pst_Transport);
        return fn_MapEad(e_EadResult);
    }

    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    pst_Route->b_IoOpen = true;
    pst_Route->e_State = GW_ROUTE_CONNECTING;
    pst_Route->ui64_AppliedOutputRevision =
        fn_EadProcessImageOutputRevision(&pst_Route->st_Image);
    ++pst_Route->ui32_ReconnectCount;
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);

    EAD_LOGI("%s: Class 1 I/O open to %s; assemblies %zu/%zu",
             pst_Route->st_RouteConfig.ac_Name,
             pst_Route->st_EadConfig.scanner.adapter_ip,
             pst_Route->st_Layout.input_size,
             pst_Route->st_Layout.output_size);
    return GW_OK;
}

static void fn_DisconnectRoute(S_GW_ROUTE *pst_Route)
{
    fn_EadEipDisconnect(pst_Route->pst_Transport);
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    pst_Route->b_IoOpen = false;
    pst_Route->b_CyclicInputSeen = false;
    pst_Route->e_State = GW_ROUTE_FAULT;
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
}

static void *fn_RouteWorker(void *pv_Argument)
{
    S_GW_ROUTE *pst_Route = (S_GW_ROUTE *)pv_Argument;
    char ac_Error[EAD_ERROR_LENGTH];
    while (!atomic_load_explicit(&pst_Route->b_StopRequested,
                                 memory_order_relaxed))
    {
        bool b_Open;
        (void)pthread_mutex_lock(&pst_Route->st_Mutex);
        b_Open = pst_Route->b_IoOpen;
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);

        if (!b_Open)
        {
            E_GW_RESULT e_Result;
            (void)memset(ac_Error, 0, sizeof(ac_Error));
            e_Result = fn_ConnectRoute(pst_Route, ac_Error, sizeof(ac_Error));
            if (e_Result != GW_OK)
            {
                (void)pthread_mutex_lock(&pst_Route->st_Mutex);
                ++pst_Route->ui32_CommunicationErrors;
                pst_Route->e_State = GW_ROUTE_FAULT;
                (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
                EAD_LOGW("%s: connection failed: %s (%s)",
                         pst_Route->st_RouteConfig.ac_Name,
                         ac_Error, fn_GW_ResultText(e_Result));
                fn_EadEipDisconnect(pst_Route->pst_Transport);
                fn_SleepMs(pst_Route->st_EadConfig.scanner.reconnect_delay_ms);
                continue;
            }
        }

        {
            uint64_t ui64_Revision;
            uint8_t aui8_Output[EAD_MAX_ASSEMBLY_SIZE];
            size_t sz_Output = 0U;
            bool b_Update = false;
            ead_result_t e_EadResult;

            (void)pthread_mutex_lock(&pst_Route->st_Mutex);
            ui64_Revision = fn_EadProcessImageOutputRevision(&pst_Route->st_Image);
            if (ui64_Revision != pst_Route->ui64_AppliedOutputRevision)
            {
                e_EadResult = fn_EadProcessImageSnapshotOutput(
                    &pst_Route->st_Image, aui8_Output,
                    sizeof(aui8_Output), &sz_Output);
                b_Update = (e_EadResult == EAD_OK);
            }
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);

            if (b_Update)
            {
                e_EadResult = fn_EadEipSetOutput(pst_Route->pst_Transport,
                                                 aui8_Output, sz_Output);
                (void)pthread_mutex_lock(&pst_Route->st_Mutex);
                if (e_EadResult == EAD_OK)
                {
                    pst_Route->ui64_AppliedOutputRevision = ui64_Revision;
                }
                else
                {
                    ++pst_Route->ui32_WriteErrors;
                }
                (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            }
        }

        (void)memset(ac_Error, 0, sizeof(ac_Error));
        if (fn_EadEipPoll(pst_Route->pst_Transport,
                          pst_Route->st_RouteConfig.ui32_WorkerPeriodMs,
                          ac_Error, sizeof(ac_Error)) != EAD_OK)
        {
            EAD_LOGW("%s: cyclic I/O lost: %s",
                     pst_Route->st_RouteConfig.ac_Name, ac_Error);
            (void)pthread_mutex_lock(&pst_Route->st_Mutex);
            ++pst_Route->ui32_CommunicationErrors;
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            fn_DisconnectRoute(pst_Route);
        }
    }

    fn_EadEipDisconnect(pst_Route->pst_Transport);
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    pst_Route->b_IoOpen = false;
    pst_Route->b_CyclicInputSeen = false;
    pst_Route->e_State = GW_ROUTE_STOPPED;
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return NULL;
}

static E_GW_RESULT fn_InitializeRoute(S_GW_ROUTE_MANAGER *pst_Manager,
                                      S_GW_ROUTE *pst_Route,
                                      const S_GW_ROUTE_CONFIG *pst_Config,
                                      char *pc_Error,
                                      size_t sz_ErrorSize)
{
    char ac_EadError[EAD_ERROR_LENGTH];
    ead_result_t e_EadResult;
    E_GW_RESULT e_Result;

    (void)memset(pst_Route, 0, sizeof(*pst_Route));
    pst_Route->st_RouteConfig = *pst_Config;
    pst_Route->pst_Owner = pst_Manager;
    atomic_init(&pst_Route->b_StopRequested, false);
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) return GW_ERR_IO;
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
    {
        (void)pthread_mutexattr_destroy(&attr);
        return GW_ERR_IO;
    }
    const int mutex_result = pthread_mutex_init(&pst_Route->st_Mutex, &attr);
    (void)pthread_mutexattr_destroy(&attr);
    if (mutex_result != 0)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name,
                    "mutex initialization failed");
        return GW_ERR_IO;
    }
    pst_Route->b_MutexInitialized = true;

    (void)memset(ac_EadError, 0, sizeof(ac_EadError));
    e_EadResult = fn_EadConfigLoad(pst_Config->ac_CouplerConfigPath,
                                   &pst_Route->st_EadConfig,
                                   ac_EadError, sizeof(ac_EadError));
    if (e_EadResult != EAD_OK)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name, ac_EadError);
        return fn_MapEad(e_EadResult);
    }
    e_EadResult = fn_EadBuildLayout(&pst_Route->st_EadConfig,
                                    &pst_Route->st_Layout,
                                    ac_EadError, sizeof(ac_EadError));
    if (e_EadResult != EAD_OK)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name, ac_EadError);
        return fn_MapEad(e_EadResult);
    }
    e_EadResult = fn_EadProcessImageInit(&pst_Route->st_Image,
                                         pst_Route->st_Layout.input_size,
                                         pst_Route->st_Layout.output_size);
    if (e_EadResult != EAD_OK)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name,
                    "process-image initialization failed");
        return fn_MapEad(e_EadResult);
    }
    fn_EadTerminalManagerInit(&pst_Route->st_TerminalManager,
                              &pst_Route->st_Layout,
                              &pst_Route->st_Image);
    e_Result = fn_BuildLogicalMap(pst_Route);
    if (e_Result != GW_OK)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name,
                    "logical channel map is invalid");
        return e_Result;
    }
    pst_Route->pst_Transport = fn_EadEipCreate();
    if (pst_Route->pst_Transport == NULL)
    {
        fn_SetError(pc_Error, sz_ErrorSize, pst_Config->ac_Name,
                    "EtherNet/IP transport allocation failed");
        return GW_ERR_IO;
    }
    pst_Route->e_State = GW_ROUTE_STOPPED;
    return GW_OK;
}

E_GW_RESULT fn_GW_RouteManagerCreate(const S_GW_CONFIG *pst_Config,
                                      S_GW_ROUTE_MANAGER **ppst_Manager,
                                      char *pc_Error,
                                      size_t sz_ErrorSize)
{
    S_GW_ROUTE_MANAGER *pst_Manager;
    size_t sz_Index;
    if ((pst_Config == NULL) || (ppst_Manager == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    *ppst_Manager = NULL;
    pst_Manager = calloc(1U, sizeof(*pst_Manager));
    if (pst_Manager == NULL)
    {
        return GW_ERR_IO;
    }
    pst_Manager->st_Config = *pst_Config;
    if (pthread_mutex_init(&pst_Manager->st_DiagnosticsMutex, NULL) != 0)
    {
        free(pst_Manager);
        return GW_ERR_IO;
    }
    pst_Manager->b_DiagnosticsMutexInitialized = true;
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        E_GW_RESULT e_Result;
        if (!pst_Config->ast_Routes[sz_Index].b_Enabled)
        {
            continue;
        }
        e_Result = fn_InitializeRoute(pst_Manager,
                                      &pst_Manager->ast_Routes[sz_Index],
                                      &pst_Config->ast_Routes[sz_Index],
                                      pc_Error, sz_ErrorSize);
        if (e_Result != GW_OK)
        {
            fn_GW_RouteManagerDestroy(&pst_Manager);
            return e_Result;
        }
    }
    *ppst_Manager = pst_Manager;
    return GW_OK;
}

E_GW_RESULT fn_GW_RouteManagerStart(S_GW_ROUTE_MANAGER *pst_Manager)
{
    size_t sz_Index;
    if (pst_Manager == NULL)
    {
        return GW_ERR_ARGUMENT;
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        S_GW_ROUTE *pst_Route = &pst_Manager->ast_Routes[sz_Index];
        if (!pst_Route->st_RouteConfig.b_Enabled)
        {
            continue;
        }
        atomic_store_explicit(&pst_Route->b_StopRequested, false,
                              memory_order_relaxed);
        if (pthread_create(&pst_Route->st_Thread, NULL,
                           fn_RouteWorker, pst_Route) != 0)
        {
            fn_GW_RouteManagerStop(pst_Manager);
            return GW_ERR_IO;
        }
        pst_Route->b_ThreadStarted = true;
    }
    return GW_OK;
}

void fn_GW_RouteManagerStop(S_GW_ROUTE_MANAGER *pst_Manager)
{
    size_t sz_Index;
    if (pst_Manager == NULL)
    {
        return;
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        if (pst_Manager->ast_Routes[sz_Index].b_ThreadStarted)
        {
            atomic_store_explicit(
                &pst_Manager->ast_Routes[sz_Index].b_StopRequested,
                true, memory_order_relaxed);
        }
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        S_GW_ROUTE *pst_Route = &pst_Manager->ast_Routes[sz_Index];
        if (pst_Route->b_ThreadStarted)
        {
            (void)pthread_join(pst_Route->st_Thread, NULL);
            pst_Route->b_ThreadStarted = false;
        }
    }
}

void fn_GW_RouteManagerDestroy(S_GW_ROUTE_MANAGER **ppst_Manager)
{
    S_GW_ROUTE_MANAGER *pst_Manager;
    size_t sz_Index;
    if ((ppst_Manager == NULL) || (*ppst_Manager == NULL))
    {
        return;
    }
    pst_Manager = *ppst_Manager;
    fn_GW_RouteManagerStop(pst_Manager);
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        S_GW_ROUTE *pst_Route = &pst_Manager->ast_Routes[sz_Index];
        if (pst_Route->pst_Transport != NULL)
        {
            fn_EadEipDestroy(pst_Route->pst_Transport);
            pst_Route->pst_Transport = NULL;
        }
        fn_EadProcessImageDestroy(&pst_Route->st_Image);
        if (pst_Route->b_MutexInitialized)
        {
            (void)pthread_mutex_destroy(&pst_Route->st_Mutex);
            pst_Route->b_MutexInitialized = false;
        }
    }
    if (pst_Manager->b_DiagnosticsMutexInitialized)
    {
        (void)pthread_mutex_destroy(&pst_Manager->st_DiagnosticsMutex);
    }
    free(pst_Manager);
    *ppst_Manager = NULL;
}

E_GW_RESULT fn_GW_RouteGetInfo(S_GW_ROUTE_MANAGER *pst_Manager,
                               uint8_t ui8_UnitId,
                               S_GW_ROUTE_INFO *pst_Info)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    if ((pst_Route == NULL) || (pst_Info == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    (void)memset(pst_Info, 0, sizeof(*pst_Info));
    (void)snprintf(pst_Info->ac_Name, sizeof(pst_Info->ac_Name), "%s",
                   pst_Route->st_RouteConfig.ac_Name);
    pst_Info->ui8_UnitId = pst_Route->st_RouteConfig.ui8_UnitId;
    pst_Info->sz_DigitalInputCount = pst_Route->sz_DiCount;
    pst_Info->sz_DigitalOutputCount = pst_Route->sz_DoCount;
    pst_Info->sz_AnalogInputCount = pst_Route->sz_AiCount;
    pst_Info->sz_AnalogOutputCount = pst_Route->sz_AoCount;
    pst_Info->e_State = pst_Route->e_State;
    pst_Info->b_IoOpen = pst_Route->b_IoOpen;
    pst_Info->b_InputValid = pst_Route->b_CyclicInputSeen &&
        fn_EadProcessImageHasInput(&pst_Route->st_Image);
    pst_Info->ui64_InputSequence =
        fn_EadProcessImageInputSequence(&pst_Route->st_Image);
    pst_Info->ui64_InputAgeMs =
        fn_EadProcessImageInputAgeMs(&pst_Route->st_Image);
    pst_Info->ui64_OutputRevision =
        fn_EadProcessImageOutputRevision(&pst_Route->st_Image);
    pst_Info->ui32_CommunicationErrors = pst_Route->ui32_CommunicationErrors;
    pst_Info->ui32_ReconnectCount = pst_Route->ui32_ReconnectCount;
    pst_Info->ui32_WriteErrors = pst_Route->ui32_WriteErrors;
    pst_Info->ui32_LastWriterIpv4 = pst_Route->ui32_LastWriterIpv4;
    pst_Info->ui64_LastWriteMs = pst_Route->ui64_LastWriteMs;
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return GW_OK;
}

static bool fn_IsBipolarAnalogMode(ead_analog_mode_t e_Mode)
{
    return (e_Mode == EAD_ANALOG_BIPOLAR_10V) ||
           (e_Mode == EAD_ANALOG_BIPOLAR_20MA);
}

static bool fn_AnalogOutputValueWithinConfiguredRange(
    const S_GW_ROUTE *pst_Route,
    size_t sz_ZeroBasedChannel,
    double d_Engineering)
{
    const ead_terminal_address_t *pst_Terminal = NULL;
    size_t sz_LocalChannel = 0U;
    double d_Minimum;
    double d_Maximum;
    const double d_Extension = 0.071;

    if ((pst_Route == NULL) || !isfinite(d_Engineering))
    {
        return false;
    }
    if (fn_EadResolveLogical(&pst_Route->st_Layout, EAD_DOMAIN_AO,
                             sz_ZeroBasedChannel + 1U, &pst_Terminal,
                             &sz_LocalChannel) != EAD_OK)
    {
        return false;
    }
    (void)sz_LocalChannel;

    if (fn_IsBipolarAnalogMode(pst_Terminal->config.analog_mode))
    {
        const double d_Midpoint =
            (pst_Terminal->config.engineering_min +
             pst_Terminal->config.engineering_max) / 2.0;
        const double d_HalfSpan =
            (pst_Terminal->config.engineering_max -
             pst_Terminal->config.engineering_min) / 2.0;
        const double d_Factor = pst_Terminal->config.allow_extended_range ?
                                (1.0 + d_Extension) : 1.0;
        d_Minimum = d_Midpoint - (d_HalfSpan * d_Factor);
        d_Maximum = d_Midpoint + (d_HalfSpan * d_Factor);
    }
    else
    {
        const double d_Span = pst_Terminal->config.engineering_max -
                              pst_Terminal->config.engineering_min;
        d_Minimum = pst_Terminal->config.engineering_min;
        d_Maximum = pst_Terminal->config.engineering_max +
                    (pst_Terminal->config.allow_extended_range ?
                     (d_Span * d_Extension) : 0.0);
    }

    return (d_Engineering >= d_Minimum) && (d_Engineering <= d_Maximum);
}

static E_GW_RESULT fn_CheckWriteAllowed(S_GW_ROUTE *pst_Route)
{
    if ((!pst_Route->b_IoOpen) || (!pst_Route->b_CyclicInputSeen) ||
        (pst_Route->e_State == GW_ROUTE_STOPPED) ||
        (pst_Route->e_State == GW_ROUTE_FAULT))
    {
        return GW_ERR_STATE;
    }
    if (pst_Route->pst_Owner->st_Config.b_RejectWritesWhenStale &&
        ((!fn_EadProcessImageHasInput(&pst_Route->st_Image)) ||
         (fn_EadProcessImageInputAgeMs(&pst_Route->st_Image) >
          pst_Route->pst_Owner->st_Config.ui32_InputStaleTimeoutMs)))
    {
        return GW_ERR_BUSY;
    }
    return GW_OK;
}

E_GW_RESULT fn_GW_RouteReadDigitalInput(S_GW_ROUTE_MANAGER *pst_Manager,
                                        uint8_t ui8_UnitId,
                                        uint16_t ui16_Channel,
                                        bool *pb_Value)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    ead_result_t e_Result;
    if ((pst_Route == NULL) || (pb_Value == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (((size_t)ui16_Channel >= pst_Route->sz_DiCount) ||
        (pst_Route->aui8_DiValid[ui16_Channel] == 0U))
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    e_Result = fn_EadGetDigitalLogical(&pst_Route->st_TerminalManager,
                                       EAD_DOMAIN_DI,
                                       (size_t)ui16_Channel + 1U,
                                       pb_Value);
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return fn_MapEad(e_Result);
}

E_GW_RESULT fn_GW_RouteReadDigitalOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Channel,
                                         bool *pb_Value)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    ead_result_t e_Result;
    if ((pst_Route == NULL) || (pb_Value == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (((size_t)ui16_Channel >= pst_Route->sz_DoCount) ||
        (pst_Route->aui8_DoValid[ui16_Channel] == 0U))
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    e_Result = fn_EadGetDigitalLogical(&pst_Route->st_TerminalManager,
                                       EAD_DOMAIN_DO,
                                       (size_t)ui16_Channel + 1U,
                                       pb_Value);
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return fn_MapEad(e_Result);
}

E_GW_RESULT fn_GW_RouteWriteDigitalOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Channel,
                                          bool b_Value,
                                          uint32_t ui32_WriterIpv4)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    E_GW_RESULT e_Result;
    ead_result_t e_EadResult;
    if (pst_Route == NULL)
    {
        return GW_ERR_NOT_FOUND;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    e_Result = fn_CheckWriteAllowed(pst_Route);
    if (e_Result == GW_OK)
    {
        if (((size_t)ui16_Channel >= pst_Route->sz_DoCount) ||
            (pst_Route->aui8_DoValid[ui16_Channel] == 0U))
        {
            e_Result = GW_ERR_RANGE;
        }
        else
        {
            e_EadResult = fn_EadSetDigitalLogical(
                &pst_Route->st_TerminalManager, EAD_DOMAIN_DO,
                (size_t)ui16_Channel + 1U, b_Value);
            e_Result = fn_MapEad(e_EadResult);
            if (e_Result == GW_OK)
            {
                pst_Route->ui32_LastWriterIpv4 = ui32_WriterIpv4;
                pst_Route->ui64_LastWriteMs = fn_MonotonicMs();
            }
            else
            {
                ++pst_Route->ui32_WriteErrors;
            }
        }
    }
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return e_Result;
}

E_GW_RESULT fn_GW_RouteReadAnalogInput(S_GW_ROUTE_MANAGER *pst_Manager,
                                       uint8_t ui8_UnitId,
                                       uint16_t ui16_Channel,
                                       ead_analog_sample_t *pst_Sample)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    ead_result_t e_Result;
    if ((pst_Route == NULL) || (pst_Sample == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (((size_t)ui16_Channel >= pst_Route->sz_AiCount) ||
        (pst_Route->aui8_AiValid[ui16_Channel] == 0U))
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    e_Result = fn_EadGetAnalogLogical(&pst_Route->st_TerminalManager,
                                      EAD_DOMAIN_AI,
                                      (size_t)ui16_Channel + 1U,
                                      pst_Sample);
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return fn_MapEad(e_Result);
}

E_GW_RESULT fn_GW_RouteReadAnalogOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                        uint8_t ui8_UnitId,
                                        uint16_t ui16_Channel,
                                        ead_analog_sample_t *pst_Sample)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    ead_result_t e_Result;
    if ((pst_Route == NULL) || (pst_Sample == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (((size_t)ui16_Channel >= pst_Route->sz_AoCount) ||
        (pst_Route->aui8_AoValid[ui16_Channel] == 0U))
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    e_Result = fn_EadGetAnalogLogical(&pst_Route->st_TerminalManager,
                                      EAD_DOMAIN_AO,
                                      (size_t)ui16_Channel + 1U,
                                      pst_Sample);
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return fn_MapEad(e_Result);
}


static E_GW_RESULT fn_GetAnalogMode(S_GW_ROUTE_MANAGER *pst_Manager,
                                    uint8_t ui8_UnitId,
                                    uint16_t ui16_Channel,
                                    ead_address_domain_t e_Domain,
                                    ead_analog_mode_t *pe_Mode)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    const ead_terminal_address_t *pst_Terminal = NULL;
    size_t sz_LocalChannel = 0U;
    ead_result_t e_Result;

    if ((pst_Route == NULL) || (pe_Mode == NULL))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }

    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    if (e_Domain == EAD_DOMAIN_AI)
    {
        if (((size_t)ui16_Channel >= pst_Route->sz_AiCount) ||
            (pst_Route->aui8_AiValid[ui16_Channel] == 0U))
        {
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return GW_ERR_RANGE;
        }
    }
    else if (e_Domain == EAD_DOMAIN_AO)
    {
        if (((size_t)ui16_Channel >= pst_Route->sz_AoCount) ||
            (pst_Route->aui8_AoValid[ui16_Channel] == 0U))
        {
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return GW_ERR_RANGE;
        }
    }
    else
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_ARGUMENT;
    }

    e_Result = fn_EadResolveLogical(&pst_Route->st_Layout,
                                    e_Domain,
                                    (size_t)ui16_Channel + 1U,
                                    &pst_Terminal,
                                    &sz_LocalChannel);
    (void)sz_LocalChannel;
    if ((e_Result == EAD_OK) && (pst_Terminal != NULL))
    {
        *pe_Mode = pst_Terminal->config.analog_mode;
    }
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return fn_MapEad(e_Result);
}

E_GW_RESULT fn_GW_RouteGetAnalogInputMode(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Channel,
                                          ead_analog_mode_t *pe_Mode)
{
    return fn_GetAnalogMode(pst_Manager,
                            ui8_UnitId,
                            ui16_Channel,
                            EAD_DOMAIN_AI,
                            pe_Mode);
}

E_GW_RESULT fn_GW_RouteGetAnalogOutputMode(S_GW_ROUTE_MANAGER *pst_Manager,
                                           uint8_t ui8_UnitId,
                                           uint16_t ui16_Channel,
                                           ead_analog_mode_t *pe_Mode)
{
    return fn_GetAnalogMode(pst_Manager,
                            ui8_UnitId,
                            ui16_Channel,
                            EAD_DOMAIN_AO,
                            pe_Mode);
}

E_GW_RESULT fn_GW_RouteWriteAnalogOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Channel,
                                         double d_Engineering,
                                         uint32_t ui32_WriterIpv4)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    E_GW_RESULT e_Result;
    ead_result_t e_EadResult;
    if (pst_Route == NULL)
    {
        return GW_ERR_NOT_FOUND;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    e_Result = fn_CheckWriteAllowed(pst_Route);
    if (e_Result == GW_OK)
    {
        if (((size_t)ui16_Channel >= pst_Route->sz_AoCount) ||
            (pst_Route->aui8_AoValid[ui16_Channel] == 0U) ||
            !fn_AnalogOutputValueWithinConfiguredRange(
                pst_Route, (size_t)ui16_Channel, d_Engineering))
        {
            e_Result = GW_ERR_RANGE;
        }
        else
        {
            e_EadResult = fn_EadSetAnalogLogical(
                &pst_Route->st_TerminalManager, EAD_DOMAIN_AO,
                (size_t)ui16_Channel + 1U, d_Engineering);
            e_Result = fn_MapEad(e_EadResult);
            if (e_Result == GW_OK)
            {
                pst_Route->ui32_LastWriterIpv4 = ui32_WriterIpv4;
                pst_Route->ui64_LastWriteMs = fn_MonotonicMs();
            }
            else
            {
                ++pst_Route->ui32_WriteErrors;
            }
        }
    }
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return e_Result;
}


E_GW_RESULT fn_GW_RouteWriteDigitalOutputs(S_GW_ROUTE_MANAGER *pst_Manager,
                                           uint8_t ui8_UnitId,
                                           uint16_t ui16_Start,
                                           uint16_t ui16_Quantity,
                                           const uint8_t *pui8_Packed,
                                           uint32_t ui32_WriterIpv4)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    uint8_t aui8_Previous[EAD_MAX_ASSEMBLY_SIZE];
    size_t sz_Previous = 0U;
    uint16_t ui16_Index;
    E_GW_RESULT e_Result;
    ead_result_t e_EadResult;
    if ((pst_Route == NULL) || (pui8_Packed == NULL) || (ui16_Quantity == 0U))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    e_Result = fn_CheckWriteAllowed(pst_Route);
    if (e_Result != GW_OK)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return e_Result;
    }
    if (((size_t)ui16_Start + (size_t)ui16_Quantity) > pst_Route->sz_DoCount)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        if (pst_Route->aui8_DoValid[(size_t)ui16_Start + ui16_Index] == 0U)
        {
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return GW_ERR_RANGE;
        }
    }
    e_EadResult = fn_EadProcessImageSnapshotOutput(
        &pst_Route->st_Image, aui8_Previous, sizeof(aui8_Previous), &sz_Previous);
    if (e_EadResult != EAD_OK)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return fn_MapEad(e_EadResult);
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        const bool b_Value =
            (pui8_Packed[ui16_Index / 8U] &
             (uint8_t)(1U << (ui16_Index % 8U))) != 0U;
        e_EadResult = fn_EadSetDigitalLogical(
            &pst_Route->st_TerminalManager, EAD_DOMAIN_DO,
            (size_t)ui16_Start + (size_t)ui16_Index + 1U, b_Value);
        if (e_EadResult != EAD_OK)
        {
            (void)fn_EadProcessImageReplaceOutput(
                &pst_Route->st_Image, aui8_Previous, sz_Previous);
            ++pst_Route->ui32_WriteErrors;
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return fn_MapEad(e_EadResult);
        }
    }
    pst_Route->ui32_LastWriterIpv4 = ui32_WriterIpv4;
    pst_Route->ui64_LastWriteMs = fn_MonotonicMs();
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return GW_OK;
}

E_GW_RESULT fn_GW_RouteWriteAnalogOutputs(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Start,
                                          uint16_t ui16_Quantity,
                                          const double *pd_Engineering,
                                          uint32_t ui32_WriterIpv4)
{
    S_GW_ROUTE *pst_Route = fn_FindRoute(pst_Manager, ui8_UnitId);
    uint8_t aui8_Previous[EAD_MAX_ASSEMBLY_SIZE];
    size_t sz_Previous = 0U;
    uint16_t ui16_Index;
    E_GW_RESULT e_Result;
    ead_result_t e_EadResult;
    if ((pst_Route == NULL) || (pd_Engineering == NULL) || (ui16_Quantity == 0U))
    {
        return (pst_Route == NULL) ? GW_ERR_NOT_FOUND : GW_ERR_ARGUMENT;
    }
    (void)pthread_mutex_lock(&pst_Route->st_Mutex);
    e_Result = fn_CheckWriteAllowed(pst_Route);
    if (e_Result != GW_OK)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return e_Result;
    }
    if (((size_t)ui16_Start + (size_t)ui16_Quantity) > pst_Route->sz_AoCount)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return GW_ERR_RANGE;
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        const size_t sz_Channel = (size_t)ui16_Start + ui16_Index;
        if ((pst_Route->aui8_AoValid[sz_Channel] == 0U) ||
            !fn_AnalogOutputValueWithinConfiguredRange(
                pst_Route, sz_Channel, pd_Engineering[ui16_Index]))
        {
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return GW_ERR_RANGE;
        }
    }
    e_EadResult = fn_EadProcessImageSnapshotOutput(
        &pst_Route->st_Image, aui8_Previous, sizeof(aui8_Previous), &sz_Previous);
    if (e_EadResult != EAD_OK)
    {
        (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
        return fn_MapEad(e_EadResult);
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        e_EadResult = fn_EadSetAnalogLogical(
            &pst_Route->st_TerminalManager, EAD_DOMAIN_AO,
            (size_t)ui16_Start + (size_t)ui16_Index + 1U,
            pd_Engineering[ui16_Index]);
        if (e_EadResult != EAD_OK)
        {
            (void)fn_EadProcessImageReplaceOutput(
                &pst_Route->st_Image, aui8_Previous, sz_Previous);
            ++pst_Route->ui32_WriteErrors;
            (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
            return fn_MapEad(e_EadResult);
        }
    }
    pst_Route->ui32_LastWriterIpv4 = ui32_WriterIpv4;
    pst_Route->ui64_LastWriteMs = fn_MonotonicMs();
    (void)pthread_mutex_unlock(&pst_Route->st_Mutex);
    return GW_OK;
}

void fn_GW_RecordConnection(S_GW_ROUTE_MANAGER *pst_Manager, bool b_Accepted)
{
    if (pst_Manager == NULL)
    {
        return;
    }
    (void)pthread_mutex_lock(&pst_Manager->st_DiagnosticsMutex);
    if (b_Accepted)
    {
        ++pst_Manager->ui64_Connections;
    }
    else
    {
        ++pst_Manager->ui64_RejectedConnections;
    }
    (void)pthread_mutex_unlock(&pst_Manager->st_DiagnosticsMutex);
}

void fn_GW_SetActiveClients(S_GW_ROUTE_MANAGER *pst_Manager, uint32_t ui32_Count)
{
    if (pst_Manager == NULL)
    {
        return;
    }
    (void)pthread_mutex_lock(&pst_Manager->st_DiagnosticsMutex);
    pst_Manager->ui32_ActiveClients = ui32_Count;
    (void)pthread_mutex_unlock(&pst_Manager->st_DiagnosticsMutex);
}

void fn_GW_RecordRequest(S_GW_ROUTE_MANAGER *pst_Manager, bool b_Exception)
{
    if (pst_Manager == NULL)
    {
        return;
    }
    (void)pthread_mutex_lock(&pst_Manager->st_DiagnosticsMutex);
    ++pst_Manager->ui64_Requests;
    if (b_Exception)
    {
        ++pst_Manager->ui64_Exceptions;
    }
    (void)pthread_mutex_unlock(&pst_Manager->st_DiagnosticsMutex);
}

void fn_GW_GetServerDiagnostics(S_GW_ROUTE_MANAGER *pst_Manager,
                                uint32_t *pui32_ActiveClients,
                                uint64_t *pui64_Connections,
                                uint64_t *pui64_RejectedConnections,
                                uint64_t *pui64_Requests,
                                uint64_t *pui64_Exceptions)
{
    if (pst_Manager == NULL)
    {
        return;
    }
    (void)pthread_mutex_lock(&pst_Manager->st_DiagnosticsMutex);
    if (pui32_ActiveClients != NULL)
    {
        *pui32_ActiveClients = pst_Manager->ui32_ActiveClients;
    }
    if (pui64_Connections != NULL)
    {
        *pui64_Connections = pst_Manager->ui64_Connections;
    }
    if (pui64_RejectedConnections != NULL)
    {
        *pui64_RejectedConnections = pst_Manager->ui64_RejectedConnections;
    }
    if (pui64_Requests != NULL)
    {
        *pui64_Requests = pst_Manager->ui64_Requests;
    }
    if (pui64_Exceptions != NULL)
    {
        *pui64_Exceptions = pst_Manager->ui64_Exceptions;
    }
    (void)pthread_mutex_unlock(&pst_Manager->st_DiagnosticsMutex);
}

/** @brief Hold one route image across an entire Modbus transaction. */
E_GW_RESULT fn_GW_RouteTransactionBegin(S_GW_ROUTE_MANAGER *manager, uint8_t unit)
{
    S_GW_ROUTE *route = fn_FindRoute(manager, unit);
    if (route == NULL) return GW_ERR_NOT_FOUND;
    return pthread_mutex_lock(&route->st_Mutex) == 0 ? GW_OK : GW_ERR_IO;
}
void fn_GW_RouteTransactionEnd(S_GW_ROUTE_MANAGER *manager, uint8_t unit)
{
    S_GW_ROUTE *route = fn_FindRoute(manager, unit);
    if (route != NULL) (void)pthread_mutex_unlock(&route->st_Mutex);
}
