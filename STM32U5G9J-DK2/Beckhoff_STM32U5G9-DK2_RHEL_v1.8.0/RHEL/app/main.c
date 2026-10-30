/**
 * @file main.c
 * @brief Single-process EK9500 EtherNet/IP to Modbus TCP gateway.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "gateway/gateway_config.h"
#include "gateway/route_manager.h"
#include "modbus_tcp/modbus_tcp_server.h"
#include "modbus_tcp/modbus_master_acl.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>

#ifndef GW_DEFAULT_CONFIG_PATH
#define GW_DEFAULT_CONFIG_PATH "config/gateway.conf"
#endif

#ifndef GW_VERSION
#define GW_VERSION "unknown"
#endif

/** @brief Arguments owned by the signal-wait thread. */
typedef struct
{
    sigset_t st_SignalSet;
    atomic_bool *pb_StopRequested;
    atomic_bool *pb_ReloadMastersRequested;
} S_SIGNAL_ARGUMENTS;

/**
 * @brief Wait synchronously for shutdown and master-ACL reload signals.
 *
 * @param[in] pv_Argument Pointer to S_SIGNAL_ARGUMENTS.
 * @return Always NULL.
 * @note SIGHUP requests a live allowlist reload. SIGINT/SIGTERM stop the app.
 */
static void *fn_SignalThread(void *pv_Argument)
{
    S_SIGNAL_ARGUMENTS *pst_Arguments = (S_SIGNAL_ARGUMENTS *)pv_Argument;

    if (pst_Arguments == NULL)
    {
        return NULL;
    }
    while (!atomic_load_explicit(pst_Arguments->pb_StopRequested,
                                 memory_order_relaxed))
    {
        int i32_Signal = 0;
        if (sigwait(&pst_Arguments->st_SignalSet, &i32_Signal) != 0)
        {
            atomic_store_explicit(pst_Arguments->pb_StopRequested, true,
                                  memory_order_relaxed);
            break;
        }
        if (i32_Signal == SIGHUP)
        {
            atomic_store_explicit(pst_Arguments->pb_ReloadMastersRequested,
                                  true, memory_order_relaxed);
        }
        else
        {
            atomic_store_explicit(pst_Arguments->pb_StopRequested, true,
                                  memory_order_relaxed);
        }
    }
    return NULL;
}

/** @brief Print accepted command-line forms. */
static void fn_PrintUsage(const char *pc_Program)
{
    (void)fprintf(stderr,
                  "Usage: %s [config/gateway.conf]\n"
                  "       %s --version\n"
                  "       %s --help\n"
                  "       --check-config [config/gateway.conf] validates files without opening sockets\n",
                  pc_Program, pc_Program, pc_Program);
}

/** @brief Print the compiled gateway release version. */
static void fn_PrintVersion(void)
{
    (void)fprintf(stdout, "ek9500_modbus_tcp_gateway %s\n", GW_VERSION);
}

/**
 * @brief Write the current process identifier for masterctl.sh.
 *
 * @param[in] pc_Path PID-file path.
 * @return true when the complete PID was written; otherwise false.
 */
/** @brief Keep the PID file locked for the complete gateway lifetime. */
static int fn_WritePidFile(const char *path)
{
    char pid[32];
    int length = snprintf(pid, sizeof(pid), "%ld\n", (long)getpid());
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0640);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0 || ftruncate(fd, 0) != 0 ||
        write(fd, pid, (size_t)length) != length)
    {
        int saved = errno;
        (void)close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

/** @brief Program entry point. */
int main(int argc, char **argv)
{
    const char *pc_ConfigPath = GW_DEFAULT_CONFIG_PATH;
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_RouteManager = NULL;
    char ac_Error[512];
    E_GW_RESULT e_Result;
    atomic_bool b_StopRequested = ATOMIC_VAR_INIT(false);
    atomic_bool b_ReloadMastersRequested = ATOMIC_VAR_INIT(false);
    S_SIGNAL_ARGUMENTS st_SignalArguments;
    pthread_t st_SignalThread;
    bool b_SignalThreadStarted = false;
    int pid_fd = -1;
    int i32_ExitCode = EXIT_FAILURE;
    size_t sz_Index;
    bool check_config = false;

    if ((argc < 1) || (argv == NULL) || (argc > 3))
    {
        fn_PrintUsage(((argv != NULL) && (argv[0] != NULL)) ?
                      argv[0] : "ek9500_modbus_tcp_gateway");
        return EXIT_FAILURE;
    }
    if (argc >= 2)
    {
        if (strcmp(argv[1], "--check-config") == 0)
        {
            check_config = true;
            pc_ConfigPath = (argc == 3) ? argv[2] : GW_DEFAULT_CONFIG_PATH;
        }
        else if (argc == 3)
        {
            fn_PrintUsage(argv[0]);
            return EXIT_FAILURE;
        }
        if ((strcmp(argv[1], "--help") == 0) ||
            (strcmp(argv[1], "-h") == 0))
        {
            fn_PrintUsage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[1], "--version") == 0)
        {
            fn_PrintVersion();
            return EXIT_SUCCESS;
        }
        if (!check_config) pc_ConfigPath = argv[1];
    }

    (void)memset(ac_Error, 0, sizeof(ac_Error));
    e_Result = fn_GW_ConfigLoad(pc_ConfigPath, &st_Config,
                                ac_Error, sizeof(ac_Error));
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "Configuration failed: %s (%s)\n",
                      ac_Error, fn_GW_ResultText(e_Result));
        return EXIT_FAILURE;
    }

    if (check_config)
    {
        S_MODB_MASTER_ACL acl;
        e_Result = fn_MODB_MasterAclLoad(st_Config.ac_MasterAclPath, &acl,
                                         ac_Error, sizeof(ac_Error));
        if (e_Result == GW_OK)
            e_Result = fn_GW_RouteManagerCreate(&st_Config, &pst_RouteManager,
                                                ac_Error, sizeof(ac_Error));
        fn_GW_RouteManagerDestroy(&pst_RouteManager);
        if (e_Result != GW_OK)
        {
            (void)fprintf(stderr, "Configuration failed: %s\n", ac_Error);
            return EXIT_FAILURE;
        }
        (void)puts("Configuration valid: gateway, ACL and both terminal layouts. No network I/O performed.");
        return EXIT_SUCCESS;
    }

    (void)sigemptyset(&st_SignalArguments.st_SignalSet);
    (void)sigaddset(&st_SignalArguments.st_SignalSet, SIGINT);
    (void)sigaddset(&st_SignalArguments.st_SignalSet, SIGTERM);
    (void)sigaddset(&st_SignalArguments.st_SignalSet, SIGHUP);
    if (pthread_sigmask(SIG_BLOCK, &st_SignalArguments.st_SignalSet, NULL) != 0)
    {
        (void)fprintf(stderr, "Unable to block gateway signals\n");
        return EXIT_FAILURE;
    }
    st_SignalArguments.pb_StopRequested = &b_StopRequested;
    st_SignalArguments.pb_ReloadMastersRequested =
        &b_ReloadMastersRequested;
    if (pthread_create(&st_SignalThread, NULL, fn_SignalThread,
                       &st_SignalArguments) != 0)
    {
        (void)fprintf(stderr, "Unable to create signal thread\n");
        return EXIT_FAILURE;
    }
    b_SignalThreadStarted = true;

    pid_fd = fn_WritePidFile(st_Config.ac_PidFilePath);
    if (pid_fd < 0)
    {
        (void)fprintf(stderr, "Cannot lock PID file %s: %s. Check directory permissions or another running gateway.\n",
                      st_Config.ac_PidFilePath, strerror(errno));
        goto cleanup;
    }

    e_Result = fn_GW_RouteManagerCreate(&st_Config, &pst_RouteManager,
                                        ac_Error, sizeof(ac_Error));
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "Route initialization failed: %s (%s)\n",
                      ac_Error, fn_GW_ResultText(e_Result));
        goto cleanup;
    }
    e_Result = fn_GW_RouteManagerStart(pst_RouteManager);
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "Unable to start EtherNet/IP workers: %s\n",
                      fn_GW_ResultText(e_Result));
        goto cleanup;
    }

    (void)fprintf(stdout, "EK9500 Modbus TCP gateway v%s\n", GW_VERSION);
    (void)fprintf(stdout, "  Master ACL: %s\n", st_Config.ac_MasterAclPath);
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        const S_GW_ROUTE_CONFIG *pst_Route = &st_Config.ast_Routes[sz_Index];
        if (pst_Route->b_Enabled)
        {
            (void)fprintf(stdout, "  Unit ID %u -> %s -> %s\n",
                          (unsigned int)pst_Route->ui8_UnitId,
                          pst_Route->ac_Name,
                          pst_Route->ac_CouplerConfigPath);
        }
    }
    (void)fflush(stdout);

    e_Result = fn_MODB_ServerRun(&st_Config, pst_RouteManager,
                                 &b_StopRequested,
                                 &b_ReloadMastersRequested);
    if (e_Result == GW_OK)
    {
        i32_ExitCode = EXIT_SUCCESS;
    }
    else
    {
        (void)fprintf(stderr, "Modbus TCP server stopped: %s\n",
                      fn_GW_ResultText(e_Result));
    }

cleanup:
    atomic_store_explicit(&b_StopRequested, true, memory_order_relaxed);
    fn_GW_RouteManagerDestroy(&pst_RouteManager);
    if (b_SignalThreadStarted)
    {
        (void)pthread_kill(st_SignalThread, SIGTERM);
        (void)pthread_join(st_SignalThread, NULL);
    }
    if (pid_fd >= 0) (void)close(pid_fd);
    return i32_ExitCode;
}
