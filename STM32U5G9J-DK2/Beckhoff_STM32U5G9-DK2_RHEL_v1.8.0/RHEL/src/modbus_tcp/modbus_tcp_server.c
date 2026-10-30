/**
 * @file modbus_tcp_server.c
 * @brief Allocation-free epoll-based Modbus TCP server for multiple masters.
 *
 * @details One event-loop thread owns the protocol context and all client
 * stream buffers. Each connection has independent receive/transmit state, so
 * fragmented requests, coalesced requests, partial sends, slow clients, and
 * simultaneous ModScan/HMI polling are handled without sharing stream state.
 *
 * @author Prathamesh Chipkar
 */
#define _GNU_SOURCE
#include "modbus_tcp/modbus_tcp_server.h"
#include "modbus_tcp/modbus_master_acl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MODB_SERVER_BACKLOG          (32)
#define MODB_SERVER_EVENT_COUNT      (GW_MAX_MODBUS_CLIENTS + 2U)
#define MODB_SERVER_SECONDARY_TAG    (GW_MAX_MODBUS_CLIENTS + 1U)
#define MODB_SERVER_EPOLL_WAIT_MS    (250)
#define MODB_SERVER_MAX_FRAME_ERRORS (8U)

/** @brief Get CLOCK_MONOTONIC time in milliseconds. */
static uint64_t fn_MODB_MonotonicMs(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0U;
    return ((uint64_t)now.tv_sec * UINT64_C(1000)) +
           ((uint64_t)now.tv_nsec / UINT64_C(1000000));
}

_Static_assert(GW_MODBUS_STREAM_CAPACITY >= GW_MODBUS_TCP_ADU_MAX,
               "receive buffer must retain one maximum Modbus TCP ADU");
_Static_assert(GW_MODBUS_TX_CAPACITY >= GW_MODBUS_TCP_ADU_MAX,
               "transmit buffer must retain one maximum Modbus TCP ADU");

/** @brief Per-master TCP stream and bounded response queue. */
typedef struct
{
    int i32_FileDescriptor;
    uint32_t ui32_PeerIpv4;
    uint8_t aui8_Receive[GW_MODBUS_STREAM_CAPACITY];
    size_t sz_ReceiveLength;
    uint8_t aui8_Transmit[GW_MODBUS_TX_CAPACITY];
    size_t sz_TransmitOffset;
    size_t sz_TransmitLength;
    uint64_t ui64_LastActivityMs;
    uint64_t ui64_RateWindowStartMs;
    uint32_t ui32_RateWindowRequests;
    uint32_t ui32_MaxRequestsPerSecond;
    uint32_t ui32_FrameErrors;
    bool b_PeerReadClosed;
    char ac_MasterName[GW_NAME_LENGTH];
    bool b_InUse;
} S_MODB_CLIENT;

/** @brief Enable non-blocking mode while preserving existing descriptor flags. */
static E_GW_RESULT fn_MODB_SetNonBlocking(const int i32_FileDescriptor)
{
    const int i32_Flags = fcntl(i32_FileDescriptor, F_GETFL, 0);
    if (i32_Flags < 0)
    {
        return GW_ERR_IO;
    }
    if (fcntl(i32_FileDescriptor, F_SETFL, i32_Flags | O_NONBLOCK) != 0)
    {
        return GW_ERR_IO;
    }
    return GW_OK;
}

/** @brief Remove, close, and clear one owned client connection. */
static void fn_MODB_CloseClient(const int i32_Epoll, S_MODB_CLIENT *pst_Client)
{
    if ((pst_Client != NULL) && pst_Client->b_InUse)
    {
        (void)epoll_ctl(i32_Epoll, EPOLL_CTL_DEL, pst_Client->i32_FileDescriptor, NULL);
        (void)close(pst_Client->i32_FileDescriptor);
        (void)memset(pst_Client, 0, sizeof(*pst_Client));
        pst_Client->i32_FileDescriptor = -1;
    }
}

/** @brief Count active client slots for diagnostics. */
static uint32_t fn_MODB_CountClients(const S_MODB_CLIENT *past_Clients,
                                     const uint8_t ui8_MaxClients)
{
    uint32_t ui32_Count = 0U;
    uint8_t ui8_Index;
    for (ui8_Index = 0U; ui8_Index < ui8_MaxClients; ++ui8_Index)
    {
        if (past_Clients[ui8_Index].b_InUse)
        {
            ++ui32_Count;
        }
    }
    return ui32_Count;
}

/** @brief Count active connections opened from one IPv4 peer. */
static uint32_t fn_MODB_CountPeerConnections(
    const S_MODB_CLIENT *past_Clients,
    const uint8_t ui8_MaxClients,
    const uint32_t ui32_PeerIpv4)
{
    uint32_t ui32_Count = 0U;
    uint8_t ui8_Index;

    for (ui8_Index = 0U; ui8_Index < ui8_MaxClients; ++ui8_Index)
    {
        if (past_Clients[ui8_Index].b_InUse &&
            (past_Clients[ui8_Index].ui32_PeerIpv4 == ui32_PeerIpv4))
        {
            ++ui32_Count;
        }
    }
    return ui32_Count;
}

/** @brief Decide whether an allowlisted master may open another connection. */
static bool fn_MODB_ClientAllowed(const S_MODB_CLIENT *past_Clients,
                                  const uint8_t ui8_MaxClients,
                                  const S_MODB_MASTER_ACL *pst_Acl,
                                  const uint32_t ui32_PeerIpv4,
                                  const S_MODB_MASTER_ACL_ENTRY **ppst_Entry)
{
    const S_MODB_MASTER_ACL_ENTRY *pst_Entry;
    const uint32_t ui32_Total = fn_MODB_CountClients(past_Clients,
                                                      ui8_MaxClients);
    const uint32_t ui32_PeerCount = fn_MODB_CountPeerConnections(
        past_Clients, ui8_MaxClients, ui32_PeerIpv4);

    if (ppst_Entry != NULL)
    {
        *ppst_Entry = NULL;
    }
    pst_Entry = fn_MODB_MasterAclFind(pst_Acl, ui32_PeerIpv4);
    if ((pst_Entry == NULL) || (ui32_Total >= ui8_MaxClients) ||
        (ui32_PeerCount >= pst_Entry->ui8_MaxConnections))
    {
        return false;
    }
    if (ppst_Entry != NULL)
    {
        *ppst_Entry = pst_Entry;
    }
    return true;
}

/** @brief Convert a peer address to printable dotted-decimal text. */
static const char *fn_MODB_PeerText(const uint32_t ui32_PeerIpv4,
                                    char *pc_Buffer,
                                    const size_t sz_Buffer)
{
    struct in_addr st_Address;
    st_Address.s_addr = ui32_PeerIpv4;
    if ((pc_Buffer == NULL) || (sz_Buffer < INET_ADDRSTRLEN) ||
        (inet_ntop(AF_INET, &st_Address, pc_Buffer,
                   (socklen_t)sz_Buffer) == NULL))
    {
        return "<invalid-ip>";
    }
    return pc_Buffer;
}

/** @brief Select EPOLLOUT only while a client has queued response bytes. */
static E_GW_RESULT fn_MODB_UpdateEvents(const int i32_Epoll,
                                        const uint32_t ui32_ClientIndex,
                                        const S_MODB_CLIENT *pst_Client)
{
    struct epoll_event st_Event;
    (void)memset(&st_Event, 0, sizeof(st_Event));
    st_Event.events = EPOLLRDHUP;
    if (!pst_Client->b_PeerReadClosed)
    {
        st_Event.events |= EPOLLIN;
    }
    if (pst_Client->sz_TransmitLength > pst_Client->sz_TransmitOffset)
    {
        st_Event.events |= EPOLLOUT;
    }
    st_Event.data.u32 = ui32_ClientIndex + 1U;
    return (epoll_ctl(i32_Epoll, EPOLL_CTL_MOD, pst_Client->i32_FileDescriptor,
                      &st_Event) == 0) ? GW_OK : GW_ERR_IO;
}

/** @brief Update the per-client one-second request window and enforce its limit. */
static bool fn_MODB_RateAllowed(S_MODB_CLIENT *pst_Client,
                                const uint32_t ui32_MaxRequestsPerSecond)
{
    const uint64_t ui64_Now = fn_MODB_MonotonicMs();
    if ((ui64_Now < pst_Client->ui64_RateWindowStartMs) ||
        ((ui64_Now - pst_Client->ui64_RateWindowStartMs) >= 1000U))
    {
        pst_Client->ui64_RateWindowStartMs = ui64_Now;
        pst_Client->ui32_RateWindowRequests = 0U;
    }
    if (pst_Client->ui32_RateWindowRequests < UINT32_MAX)
        ++pst_Client->ui32_RateWindowRequests;
    return (pst_Client->ui32_RateWindowRequests <= ui32_MaxRequestsPerSecond);
}

/** @brief Compact and append one complete response to a client transmit queue. */
static bool fn_MODB_AppendResponse(S_MODB_CLIENT *pst_Client,
                                   const uint8_t *pui8_Response,
                                   const size_t sz_ResponseLength)
{
    size_t sz_Queued;
    if (pst_Client->sz_TransmitOffset == pst_Client->sz_TransmitLength)
    {
        pst_Client->sz_TransmitOffset = 0U;
        pst_Client->sz_TransmitLength = 0U;
    }
    else if (pst_Client->sz_TransmitOffset > 0U)
    {
        sz_Queued = pst_Client->sz_TransmitLength - pst_Client->sz_TransmitOffset;
        (void)memmove(pst_Client->aui8_Transmit,
                      &pst_Client->aui8_Transmit[pst_Client->sz_TransmitOffset],
                      sz_Queued);
        pst_Client->sz_TransmitOffset = 0U;
        pst_Client->sz_TransmitLength = sz_Queued;
    }
    if (sz_ResponseLength > (GW_MODBUS_TX_CAPACITY - pst_Client->sz_TransmitLength))
    {
        return false;
    }
    (void)memcpy(&pst_Client->aui8_Transmit[pst_Client->sz_TransmitLength],
                 pui8_Response, sz_ResponseLength);
    pst_Client->sz_TransmitLength += sz_ResponseLength;
    return true;
}

/** @brief Consume all complete requests that fit the bounded transmit queue. */
static bool fn_MODB_ProcessBuffered(S_MODB_CONTEXT *pst_Context,
                                    S_MODB_CLIENT *pst_Client)
{
    uint8_t aui8_Response[GW_MODBUS_TCP_ADU_MAX];
    size_t sz_Offset = 0U;
    uint8_t ui8_ProcessedRequests = 0U;
    bool b_KeepOpen = true;

    while ((sz_Offset < pst_Client->sz_ReceiveLength) && b_KeepOpen &&
           (ui8_ProcessedRequests <
            pst_Context->pst_Config->ui8_MaxRequestsPerEventPerClient))
    {
        size_t sz_Consumed = 0U;
        size_t sz_ResponseLength = 0U;
        E_MODB_RESULT e_Result;

        if ((GW_MODBUS_TX_CAPACITY -
             (pst_Client->sz_TransmitLength - pst_Client->sz_TransmitOffset)) <
            GW_MODBUS_TCP_ADU_MAX)
        {
            break;
        }
        const uint8_t *request = &pst_Client->aui8_Receive[sz_Offset];
        const size_t available = pst_Client->sz_ReceiveLength - sz_Offset;
        if (available >= 7U)
        {
            const size_t length = ((size_t)request[4] << 8U) | request[5];
            if (request[2] == 0U && request[3] == 0U && length >= 2U &&
                length <= 254U && available >= length + 6U &&
                !fn_MODB_RateAllowed(pst_Client, pst_Client->ui32_MaxRequestsPerSecond))
            {
                /* A valid busy exception retains the persistent TCP connection.
                 * No output operation has run for this request. */
                uint8_t busy[9];
                (void)memcpy(busy, request, 7U);
                busy[4] = 0U; busy[5] = 3U;
                busy[7] = (uint8_t)(request[7] | 0x80U); busy[8] = 6U;
                if (!fn_MODB_AppendResponse(pst_Client, busy, sizeof(busy))) break;
                sz_Offset += length + 6U;
                ++ui8_ProcessedRequests;
                continue;
            }
        }
        e_Result = fn_MODB_ProcessTcpRequest(
            pst_Context,
            &pst_Client->aui8_Receive[sz_Offset],
            pst_Client->sz_ReceiveLength - sz_Offset,
            aui8_Response,
            sizeof(aui8_Response),
            &sz_Consumed,
            &sz_ResponseLength,
            pst_Client->ui32_PeerIpv4);

        if (e_Result == MODB_RESULT_NEED_MORE_DATA)
        {
            break;
        }
        if (sz_Consumed == 0U)
        {
            b_KeepOpen = false;
            break;
        }
        if (e_Result == MODB_RESULT_RESPONSE_READY)
        {
            if (!fn_MODB_AppendResponse(pst_Client, aui8_Response, sz_ResponseLength))
            {
                break;
            }
            pst_Client->ui32_FrameErrors = 0U;
        }
        else if (e_Result == MODB_RESULT_FRAME_ERROR)
        {
            ++pst_Client->ui32_FrameErrors;
            if (pst_Client->ui32_FrameErrors >= MODB_SERVER_MAX_FRAME_ERRORS)
            {
                b_KeepOpen = false;
            }
        }
        else if ((e_Result == MODB_RESULT_INVALID_ARGUMENT) ||
                 (e_Result == MODB_RESULT_BUFFER_TOO_SMALL))
        {
            b_KeepOpen = false;
        }
        else
        {
            /* NOT_FOR_UNIT consumes a complete request without a response. */
        }
        sz_Offset += sz_Consumed;
        ++ui8_ProcessedRequests;
    }

    if (sz_Offset > 0U)
    {
        const size_t sz_Remaining = pst_Client->sz_ReceiveLength - sz_Offset;
        if (sz_Remaining > 0U)
        {
            (void)memmove(pst_Client->aui8_Receive,
                          &pst_Client->aui8_Receive[sz_Offset], sz_Remaining);
        }
        pst_Client->sz_ReceiveLength = sz_Remaining;
    }
    return b_KeepOpen;
}

/** @brief Drain non-blocking input and pass retained stream bytes to the parser. */
static bool fn_MODB_ReceiveClient(S_MODB_CONTEXT *pst_Context,
                                  S_MODB_CLIENT *pst_Client)
{
    bool b_KeepOpen = true;
    for (;;)
    {
        ssize_t sz_Read;
        if (pst_Client->sz_ReceiveLength == sizeof(pst_Client->aui8_Receive))
        {
            b_KeepOpen = false;
            break;
        }
        sz_Read = recv(pst_Client->i32_FileDescriptor,
                       &pst_Client->aui8_Receive[pst_Client->sz_ReceiveLength],
                       sizeof(pst_Client->aui8_Receive) - pst_Client->sz_ReceiveLength,
                       0);
        if (sz_Read > 0)
        {
            pst_Client->sz_ReceiveLength += (size_t)sz_Read;
            pst_Client->ui64_LastActivityMs = fn_MODB_MonotonicMs();
        }
        else if (sz_Read == 0)
        {
            pst_Client->b_PeerReadClosed = true;
            break;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            break;
        }
        else
        {
            b_KeepOpen = false;
            break;
        }
    }
    if (b_KeepOpen)
    {
        b_KeepOpen = fn_MODB_ProcessBuffered(pst_Context, pst_Client);
    }
    if (b_KeepOpen && pst_Client->b_PeerReadClosed)
    {
        if (pst_Client->sz_ReceiveLength > 0U)
        {
            /* EOF with an incomplete ADU cannot be resynchronized. */
            b_KeepOpen = false;
        }
        else if (pst_Client->sz_TransmitOffset == pst_Client->sz_TransmitLength)
        {
            b_KeepOpen = false;
        }
        else
        {
            /* Retain the socket until every queued response is transmitted. */
        }
    }
    return b_KeepOpen;
}

/** @brief Drain queued response bytes while handling partial non-blocking sends. */
static bool fn_MODB_SendClient(S_MODB_CONTEXT *pst_Context,
                               S_MODB_CLIENT *pst_Client)
{
    bool b_KeepOpen = true;
    while (pst_Client->sz_TransmitOffset < pst_Client->sz_TransmitLength)
    {
        const ssize_t sz_Sent = send(
            pst_Client->i32_FileDescriptor,
            &pst_Client->aui8_Transmit[pst_Client->sz_TransmitOffset],
            pst_Client->sz_TransmitLength - pst_Client->sz_TransmitOffset,
            MSG_NOSIGNAL);
        if (sz_Sent > 0)
        {
            pst_Client->sz_TransmitOffset += (size_t)sz_Sent;
            pst_Client->ui64_LastActivityMs = fn_MODB_MonotonicMs();
        }
        else if (sz_Sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                break;
            }
            b_KeepOpen = false;
            break;
        }
        else
        {
            b_KeepOpen = false;
            break;
        }
    }
    if (b_KeepOpen &&
        (pst_Client->sz_TransmitOffset == pst_Client->sz_TransmitLength))
    {
        pst_Client->sz_TransmitOffset = 0U;
        pst_Client->sz_TransmitLength = 0U;
        b_KeepOpen = fn_MODB_ProcessBuffered(pst_Context, pst_Client);
        if (b_KeepOpen && pst_Client->b_PeerReadClosed &&
            (pst_Client->sz_ReceiveLength == 0U) &&
            (pst_Client->sz_TransmitLength == 0U))
        {
            b_KeepOpen = false;
        }
    }
    return b_KeepOpen;
}

/** @brief Locate the first unused bounded client slot. */
static int fn_MODB_FindFreeClient(const S_MODB_CLIENT *past_Clients,
                                  const uint8_t ui8_MaxClients)
{
    uint8_t ui8_Index;
    for (ui8_Index = 0U; ui8_Index < ui8_MaxClients; ++ui8_Index)
    {
        if (!past_Clients[ui8_Index].b_InUse)
        {
            return (int)ui8_Index;
        }
    }
    return -1;
}

/** @brief Accept all pending allowlisted masters into bounded client slots. */
static void fn_MODB_AcceptClients(const int i32_Listen,
                                  const int i32_Epoll,
                                  S_MODB_CLIENT *past_Clients,
                                  const S_GW_CONFIG *pst_Config,
                                  const S_MODB_MASTER_ACL *pst_Acl,
                                  S_GW_ROUTE_MANAGER *pst_RouteManager)
{
    for (;;)
    {
        struct sockaddr_in st_Peer;
        socklen_t sz_PeerLength = (socklen_t)sizeof(st_Peer);
        int i32_Client;
        int i32_Index;
        int i32_One = 1;
        struct epoll_event st_Event;
        const S_MODB_MASTER_ACL_ENTRY *pst_Master = NULL;
        char ac_PeerIp[INET_ADDRSTRLEN];

        (void)memset(&st_Peer, 0, sizeof(st_Peer));
        i32_Client = accept4(i32_Listen, (struct sockaddr *)&st_Peer,
                             &sz_PeerLength, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (i32_Client < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (!fn_MODB_ClientAllowed(past_Clients, pst_Config->ui8_MaxClients,
                                   pst_Acl, st_Peer.sin_addr.s_addr,
                                   &pst_Master))
        {
            (void)fprintf(stderr,
                          "Rejected Modbus master %s: not allowed or "
                          "connection limit reached\n",
                          fn_MODB_PeerText(st_Peer.sin_addr.s_addr,
                                           ac_PeerIp, sizeof(ac_PeerIp)));
            fn_GW_RecordConnection(pst_RouteManager, false);
            (void)close(i32_Client);
            continue;
        }

        i32_Index = fn_MODB_FindFreeClient(past_Clients,
                                           pst_Config->ui8_MaxClients);
        if ((i32_Index < 0) || (pst_Master == NULL))
        {
            fn_GW_RecordConnection(pst_RouteManager, false);
            (void)close(i32_Client);
            continue;
        }
        (void)setsockopt(i32_Client, IPPROTO_TCP, TCP_NODELAY, &i32_One,
                         (socklen_t)sizeof(i32_One));
        if (pst_Config->b_EnableTcpKeepalive)
        {
            (void)setsockopt(i32_Client, SOL_SOCKET, SO_KEEPALIVE, &i32_One,
                             (socklen_t)sizeof(i32_One));
        }
        (void)memset(&past_Clients[i32_Index], 0,
                     sizeof(past_Clients[i32_Index]));
        past_Clients[i32_Index].i32_FileDescriptor = i32_Client;
        past_Clients[i32_Index].ui32_PeerIpv4 = st_Peer.sin_addr.s_addr;
        past_Clients[i32_Index].ui64_LastActivityMs = fn_MODB_MonotonicMs();
        past_Clients[i32_Index].ui64_RateWindowStartMs =
            past_Clients[i32_Index].ui64_LastActivityMs;
        past_Clients[i32_Index].ui32_MaxRequestsPerSecond =
            fn_MODB_MasterAclEffectiveRate(
                pst_Master, pst_Config->ui32_MaxRequestsPerSecondPerClient);
        (void)snprintf(past_Clients[i32_Index].ac_MasterName,
                       sizeof(past_Clients[i32_Index].ac_MasterName), "%s",
                       pst_Master->ac_Name);
        past_Clients[i32_Index].b_InUse = true;

        (void)memset(&st_Event, 0, sizeof(st_Event));
        st_Event.events = EPOLLIN | EPOLLRDHUP;
        st_Event.data.u32 = (uint32_t)i32_Index + 1U;
        if (epoll_ctl(i32_Epoll, EPOLL_CTL_ADD, i32_Client, &st_Event) != 0)
        {
            fn_MODB_CloseClient(i32_Epoll, &past_Clients[i32_Index]);
            fn_GW_RecordConnection(pst_RouteManager, false);
        }
        else
        {
            (void)fprintf(stdout,
                          "Accepted Modbus master %s (%s) in slot %d; rate=%u req/s\n",
                          pst_Master->ac_Name,
                          fn_MODB_PeerText(st_Peer.sin_addr.s_addr,
                                           ac_PeerIp, sizeof(ac_PeerIp)),
                          i32_Index,
                          (unsigned int)past_Clients[i32_Index].ui32_MaxRequestsPerSecond);
            fn_GW_RecordConnection(pst_RouteManager, true);
        }
    }
    fn_GW_SetActiveClients(
        pst_RouteManager, fn_MODB_CountClients(past_Clients,
                                               pst_Config->ui8_MaxClients));
}

/** @brief Print the active master allowlist. */
static void fn_MODB_PrintAcl(const S_MODB_MASTER_ACL *pst_Acl)
{
    uint8_t ui8_Index;

    (void)fprintf(stdout, "Allowed Modbus master hosts: %u\n",
                  (unsigned int)pst_Acl->ui8_Count);
    for (ui8_Index = 0U; ui8_Index < pst_Acl->ui8_Count; ++ui8_Index)
    {
        const S_MODB_MASTER_ACL_ENTRY *pst_Entry =
            &pst_Acl->ast_Entries[ui8_Index];
        (void)fprintf(stdout,
                      "  %s -> %s; maximum connections=%u; rate=%s",
                      pst_Entry->ac_Name, pst_Entry->ac_IpAddress,
                      (unsigned int)pst_Entry->ui8_MaxConnections,
                      (pst_Entry->ui32_MaxRequestsPerSecond == 0U) ?
                      "gateway-default" : "custom");
        if (pst_Entry->ui32_MaxRequestsPerSecond != 0U)
        {
            (void)fprintf(stdout, " (%u req/s)",
                          (unsigned int)pst_Entry->ui32_MaxRequestsPerSecond);
        }
        (void)fprintf(stdout, "\n");
    }
    if (pst_Acl->ui8_Count == 0U)
    {
        (void)fprintf(stdout,
                      "  No masters are allowed until masters.conf is updated "
                      "and reloaded.\n");
    }
}

/**
 * @brief Apply a newly loaded ACL and disconnect removed or excess masters.
 */
static void fn_MODB_ReconcileClients(const int i32_Epoll,
                                     S_MODB_CLIENT *past_Clients,
                                     const uint8_t ui8_MaxClients,
                                     const S_MODB_MASTER_ACL *pst_Acl,
                                     const uint32_t ui32_DefaultRate)
{
    uint8_t aui8_ConnectionCounts[MODB_MASTER_ACL_MAX_ENTRIES];
    uint8_t ui8_ClientIndex;

    (void)memset(aui8_ConnectionCounts, 0, sizeof(aui8_ConnectionCounts));
    for (ui8_ClientIndex = 0U; ui8_ClientIndex < ui8_MaxClients;
         ++ui8_ClientIndex)
    {
        S_MODB_CLIENT *pst_Client = &past_Clients[ui8_ClientIndex];
        uint8_t ui8_AclIndex = 0U;
        bool b_Keep = false;

        if (!pst_Client->b_InUse)
        {
            continue;
        }
        if (fn_MODB_MasterAclFindIndex(pst_Acl, pst_Client->ui32_PeerIpv4,
                                       &ui8_AclIndex))
        {
            const S_MODB_MASTER_ACL_ENTRY *pst_Entry =
                &pst_Acl->ast_Entries[ui8_AclIndex];
            if (aui8_ConnectionCounts[ui8_AclIndex] <
                pst_Entry->ui8_MaxConnections)
            {
                ++aui8_ConnectionCounts[ui8_AclIndex];
                (void)snprintf(pst_Client->ac_MasterName,
                               sizeof(pst_Client->ac_MasterName), "%s",
                               pst_Entry->ac_Name);
                pst_Client->ui32_MaxRequestsPerSecond =
                    fn_MODB_MasterAclEffectiveRate(pst_Entry, ui32_DefaultRate);
                b_Keep = true;
            }
        }
        if (!b_Keep)
        {
            char ac_PeerIp[INET_ADDRSTRLEN];
            (void)fprintf(stdout,
                          "Disconnected removed/excess Modbus master %s (%s)\n",
                          pst_Client->ac_MasterName,
                          fn_MODB_PeerText(pst_Client->ui32_PeerIpv4,
                                           ac_PeerIp, sizeof(ac_PeerIp)));
            fn_MODB_CloseClient(i32_Epoll, pst_Client);
        }
    }
}

/** @brief Create, bind, and listen on the configured non-blocking endpoint. */
static E_GW_RESULT fn_MODB_CreateListener(const S_GW_CONFIG *pst_Config,
                                          const uint16_t ui16_Port,
                                          int *pi32_Listen)
{
    struct sockaddr_in st_Address;
    int i32_Listen;
    int i32_One = 1;
    if ((pst_Config == NULL) || (pi32_Listen == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    i32_Listen = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (i32_Listen < 0)
    {
        return GW_ERR_IO;
    }
    if ((setsockopt(i32_Listen, SOL_SOCKET, SO_REUSEADDR, &i32_One,
                    (socklen_t)sizeof(i32_One)) != 0) ||
        (fn_MODB_SetNonBlocking(i32_Listen) != GW_OK))
    {
        (void)close(i32_Listen);
        return GW_ERR_IO;
    }
    (void)memset(&st_Address, 0, sizeof(st_Address));
    st_Address.sin_family = AF_INET;
    st_Address.sin_port = htons(ui16_Port);
    if (inet_pton(AF_INET, pst_Config->ac_BindIp, &st_Address.sin_addr) != 1)
    {
        (void)close(i32_Listen);
        return GW_ERR_CONFIG;
    }
    if ((bind(i32_Listen, (const struct sockaddr *)&st_Address,
              (socklen_t)sizeof(st_Address)) != 0) ||
        (listen(i32_Listen, MODB_SERVER_BACKLOG) != 0))
    {
        (void)close(i32_Listen);
        return GW_ERR_IO;
    }
    *pi32_Listen = i32_Listen;
    return GW_OK;
}

E_GW_RESULT fn_MODB_ServerRun(const S_GW_CONFIG *pst_Config,
                              S_GW_ROUTE_MANAGER *pst_RouteManager,
                              atomic_bool *pb_StopRequested,
                              atomic_bool *pb_ReloadMastersRequested)
{
    S_MODB_CLIENT ast_Clients[GW_MAX_MODBUS_CLIENTS];
    struct epoll_event ast_Events[MODB_SERVER_EVENT_COUNT];
    struct epoll_event st_ListenEvent;
    S_MODB_CONTEXT st_Context;
    S_MODB_MASTER_ACL st_MasterAcl;
    char ac_AclError[256];
    int i32_ListenPrimary = -1;
    int i32_ListenSecondary = -1;
    int i32_Epoll = -1;
    E_GW_RESULT e_Result = GW_OK;
    uint8_t ui8_Index;

    if ((pst_Config == NULL) || (pst_RouteManager == NULL) ||
        (pb_StopRequested == NULL) || (pb_ReloadMastersRequested == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    (void)memset(ast_Clients, 0, sizeof(ast_Clients));
    for (ui8_Index = 0U; ui8_Index < GW_MAX_MODBUS_CLIENTS; ++ui8_Index)
    {
        ast_Clients[ui8_Index].i32_FileDescriptor = -1;
    }
    e_Result = fn_MODB_ProtocolInitialize(&st_Context, pst_RouteManager, pst_Config);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }
    (void)memset(ac_AclError, 0, sizeof(ac_AclError));
    e_Result = fn_MODB_MasterAclLoad(pst_Config->ac_MasterAclPath,
                                     &st_MasterAcl, ac_AclError,
                                     sizeof(ac_AclError));
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "Master ACL load failed: %s\n", ac_AclError);
        return e_Result;
    }
    e_Result = fn_MODB_CreateListener(pst_Config, pst_Config->ui16_Port,
                                      &i32_ListenPrimary);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }
    if (pst_Config->ui16_SecondaryPort != 0U)
    {
        e_Result = fn_MODB_CreateListener(pst_Config,
                                          pst_Config->ui16_SecondaryPort,
                                          &i32_ListenSecondary);
        if (e_Result != GW_OK)
        {
            (void)close(i32_ListenPrimary);
            return e_Result;
        }
    }
    i32_Epoll = epoll_create1(EPOLL_CLOEXEC);
    if (i32_Epoll < 0)
    {
        if (i32_ListenSecondary >= 0)
        {
            (void)close(i32_ListenSecondary);
        }
        (void)close(i32_ListenPrimary);
        return GW_ERR_IO;
    }
    (void)memset(&st_ListenEvent, 0, sizeof(st_ListenEvent));
    st_ListenEvent.events = EPOLLIN;
    st_ListenEvent.data.u32 = 0U;
    if (epoll_ctl(i32_Epoll, EPOLL_CTL_ADD, i32_ListenPrimary,
                  &st_ListenEvent) != 0)
    {
        (void)close(i32_Epoll);
        if (i32_ListenSecondary >= 0)
        {
            (void)close(i32_ListenSecondary);
        }
        (void)close(i32_ListenPrimary);
        return GW_ERR_IO;
    }
    if (i32_ListenSecondary >= 0)
    {
        (void)memset(&st_ListenEvent, 0, sizeof(st_ListenEvent));
        st_ListenEvent.events = EPOLLIN;
        st_ListenEvent.data.u32 = MODB_SERVER_SECONDARY_TAG;
        if (epoll_ctl(i32_Epoll, EPOLL_CTL_ADD, i32_ListenSecondary,
                      &st_ListenEvent) != 0)
        {
            (void)close(i32_Epoll);
            (void)close(i32_ListenSecondary);
            (void)close(i32_ListenPrimary);
            return GW_ERR_IO;
        }
    }
    (void)fprintf(stdout,
                  "Modbus TCP listening on %s:%u",
                  pst_Config->ac_BindIp, pst_Config->ui16_Port);
    if (pst_Config->ui16_SecondaryPort != 0U)
    {
        (void)fprintf(stdout, " and %s:%u", pst_Config->ac_BindIp,
                      pst_Config->ui16_SecondaryPort);
    }
    (void)fprintf(stdout, "; maximum clients=%u shared across listeners\n",
                  pst_Config->ui8_MaxClients);
    (void)fprintf(stdout, "Master ACL file: %s\n",
                  pst_Config->ac_MasterAclPath);
    fn_MODB_PrintAcl(&st_MasterAcl);

    while (!atomic_load_explicit(pb_StopRequested, memory_order_relaxed))
    {
        const int i32_EventCount = epoll_wait(i32_Epoll, ast_Events,
                                             (int)MODB_SERVER_EVENT_COUNT,
                                             MODB_SERVER_EPOLL_WAIT_MS);
        int i32_EventIndex;
        const uint64_t ui64_Now = fn_MODB_MonotonicMs();
        if (i32_EventCount < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            e_Result = GW_ERR_IO;
            break;
        }
        if (atomic_exchange_explicit(pb_ReloadMastersRequested, false,
                                     memory_order_relaxed))
        {
            S_MODB_MASTER_ACL st_NewAcl;
            (void)memset(ac_AclError, 0, sizeof(ac_AclError));
            if (fn_MODB_MasterAclLoad(pst_Config->ac_MasterAclPath,
                                      &st_NewAcl, ac_AclError,
                                      sizeof(ac_AclError)) == GW_OK)
            {
                st_MasterAcl = st_NewAcl;
                (void)fprintf(stdout, "Reloaded Modbus master ACL\n");
                fn_MODB_PrintAcl(&st_MasterAcl);
                fn_MODB_ReconcileClients(i32_Epoll, ast_Clients,
                                         pst_Config->ui8_MaxClients,
                                         &st_MasterAcl,
                                         pst_Config->ui32_MaxRequestsPerSecondPerClient);
            }
            else
            {
                (void)fprintf(stderr,
                              "Master ACL reload rejected; previous ACL retained: %s\n",
                              ac_AclError);
            }
        }
        for (i32_EventIndex = 0; i32_EventIndex < i32_EventCount; ++i32_EventIndex)
        {
            const uint32_t ui32_Tag = ast_Events[i32_EventIndex].data.u32;
            if (ui32_Tag == 0U)
            {
                fn_MODB_AcceptClients(i32_ListenPrimary, i32_Epoll,
                                      ast_Clients, pst_Config, &st_MasterAcl,
                                      pst_RouteManager);
            }
            else if (ui32_Tag == MODB_SERVER_SECONDARY_TAG)
            {
                fn_MODB_AcceptClients(i32_ListenSecondary, i32_Epoll,
                                      ast_Clients, pst_Config, &st_MasterAcl,
                                      pst_RouteManager);
            }
            else
            {
                const uint32_t ui32_ClientIndex = ui32_Tag - 1U;
                bool b_KeepOpen = true;
                if ((ui32_ClientIndex >= pst_Config->ui8_MaxClients) ||
                    !ast_Clients[ui32_ClientIndex].b_InUse)
                {
                    continue;
                }
                if ((ast_Events[i32_EventIndex].events &
                     (EPOLLERR | EPOLLHUP)) != 0U)
                {
                    b_KeepOpen = false;
                }
                if (b_KeepOpen &&
                    ((ast_Events[i32_EventIndex].events &
                      (EPOLLIN | EPOLLRDHUP)) != 0U))
                {
                    b_KeepOpen = fn_MODB_ReceiveClient(&st_Context,
                                                       &ast_Clients[ui32_ClientIndex]);
                }
                if (b_KeepOpen &&
                    ((ast_Events[i32_EventIndex].events & EPOLLOUT) != 0U))
                {
                    b_KeepOpen = fn_MODB_SendClient(&st_Context,
                                                    &ast_Clients[ui32_ClientIndex]);
                }
                if (!b_KeepOpen ||
                    (fn_MODB_UpdateEvents(i32_Epoll, ui32_ClientIndex,
                                          &ast_Clients[ui32_ClientIndex]) != GW_OK))
                {
                    fn_MODB_CloseClient(i32_Epoll, &ast_Clients[ui32_ClientIndex]);
                }
            }
        }
        for (ui8_Index = 0U; ui8_Index < pst_Config->ui8_MaxClients; ++ui8_Index)
        {
            if (ast_Clients[ui8_Index].b_InUse &&
                (ui64_Now >= ast_Clients[ui8_Index].ui64_LastActivityMs) &&
                ((ui64_Now - ast_Clients[ui8_Index].ui64_LastActivityMs) >
                 pst_Config->ui32_IdleTimeoutMs))
            {
                fn_MODB_CloseClient(i32_Epoll, &ast_Clients[ui8_Index]);
            }
        }
        fn_GW_SetActiveClients(
            pst_RouteManager, fn_MODB_CountClients(ast_Clients,
                                                   pst_Config->ui8_MaxClients));
    }

    for (ui8_Index = 0U; ui8_Index < pst_Config->ui8_MaxClients; ++ui8_Index)
    {
        fn_MODB_CloseClient(i32_Epoll, &ast_Clients[ui8_Index]);
    }
    fn_GW_SetActiveClients(pst_RouteManager, 0U);
    (void)close(i32_Epoll);
    if (i32_ListenSecondary >= 0)
    {
        (void)close(i32_ListenSecondary);
    }
    (void)close(i32_ListenPrimary);
    return e_Result;
}
