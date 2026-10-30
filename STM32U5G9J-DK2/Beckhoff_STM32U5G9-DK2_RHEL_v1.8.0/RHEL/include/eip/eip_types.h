/**
 * @file eip_types.h
 * @brief EtherNet/IP encapsulation, CIP path, connection and Class 1 transport data types.
 * @author Prathamesh Chipkar
 */
#ifndef EIP_TYPES_H
#define EIP_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EIP_TCP_PORT_DEFAULT        (44818U)
#define EIP_UDP_PORT_DEFAULT        (2222U)
#define EIP_ENCAP_HEADER_SIZE       (24U)
#define EIP_MAX_PACKET_SIZE         (8192U)
#define EIP_MAX_CPF_ITEMS           (8U)
#define EIP_MAX_ASSEMBLY_SIZE       (2048U)
#define EIP_MAX_PATH_SIZE           (64U)
#define EIP_MAX_CIP_RESPONSE_SIZE   (4096U)

#define EIP_ENCAP_CMD_NOP                (0x0000U)
#define EIP_ENCAP_CMD_LIST_IDENTITY      (0x0063U)
#define EIP_ENCAP_CMD_REGISTER_SESSION   (0x0065U)
#define EIP_ENCAP_CMD_UNREGISTER_SESSION (0x0066U)
#define EIP_ENCAP_CMD_SEND_RR_DATA       (0x006FU)

#define EIP_CPF_NULL_ADDRESS             (0x0000U)
#define EIP_CPF_CONNECTED_ADDRESS        (0x00A1U)
#define EIP_CPF_CONNECTED_DATA           (0x00B1U)
#define EIP_CPF_UNCONNECTED_DATA         (0x00B2U)
#define EIP_CPF_SEQUENCED_ADDRESS        (0x8002U)
#define EIP_CPF_O2T_SOCKADDR             (0x8000U)
#define EIP_CPF_T2O_SOCKADDR             (0x8001U)

#define CIP_SERVICE_GET_ATTRIBUTES_ALL   (0x01U)
#define CIP_SERVICE_GET_ATTRIBUTE_SINGLE (0x0EU)
#define CIP_SERVICE_SET_ATTRIBUTE_SINGLE (0x10U)
#define CIP_SERVICE_FORWARD_OPEN         (0x54U)
#define CIP_SERVICE_LARGE_FORWARD_OPEN   (0x5BU)
#define CIP_SERVICE_FORWARD_CLOSE        (0x4EU)

#define CIP_CLASS_IDENTITY               (0x01U)
#define CIP_CLASS_ASSEMBLY               (0x04U)
#define CIP_CLASS_CONNECTION_MANAGER     (0x06U)

#define CIP_GENERAL_STATUS_SUCCESS       (0x00U)

#define CIP_NCP_VARIABLE                 (1U << 9)
#define CIP_NCP_PRIORITY_HIGH            (1U << 10)
#define CIP_NCP_PRIORITY_SCHEDULED       (2U << 10)
#define CIP_NCP_PRIORITY_URGENT          (3U << 10)
#define CIP_NCP_CONNECTION_MULTICAST     (1U << 13)
#define CIP_NCP_CONNECTION_P2P           (2U << 13)
#define CIP_NCP_REDUNDANT_OWNER          (1U << 15)

#define CIP_TRIGGER_CYCLIC               (0U)
#define CIP_TRIGGER_CHANGE_OF_STATE      (1U << 4)
#define CIP_TRIGGER_APPLICATION          (2U << 4)
#define CIP_TRANSPORT_CLASS_1            (1U)
#define CIP_TRANSPORT_SERVER             (0x80U)

/** @brief EtherNet/IP/CIP protocol state or service enumeration used by this API. */
typedef enum
{
    EIP_OK = 0,
    EIP_ERR_ARGUMENT = -1,
    EIP_ERR_SOCKET = -2,
    EIP_ERR_TIMEOUT = -3,
    EIP_ERR_PROTOCOL = -4,
    EIP_ERR_REMOTE = -5,
    EIP_ERR_OVERFLOW = -6,
    EIP_ERR_NOT_SUPPORTED = -7,
    EIP_ERR_CONFIG = -8,
    EIP_ERR_IO = -9
} eip_result_t;

/** @brief EtherNet/IP/CIP protocol data container used by session or Class 1 I/O handling. */
typedef struct
{
    uint16_t type_id;
    uint8_t data[32];
    uint16_t length;
} eip_cpf_item_t;

/** @brief EtherNet/IP/CIP protocol data container used by session or Class 1 I/O handling. */
typedef struct
{
    uint8_t response_service;
    uint8_t general_status;
    uint16_t additional_status[8];
    size_t additional_status_count;
    const uint8_t *data;
    size_t data_length;
} cip_response_view_t;

/** @brief EtherNet/IP/CIP protocol data container used by session or Class 1 I/O handling. */
typedef struct
{
    int tcp_fd;
    uint32_t session_handle;
    char target_ip[64];
    uint16_t target_tcp_port;
    int tcp_timeout_ms;
    uint64_t sender_context_counter;
} eip_session_t;

/** @brief EtherNet/IP/CIP protocol data container used by session or Class 1 I/O handling. */
typedef struct
{
    uint32_t o2t_connection_id;
    uint32_t t2o_connection_id;
    uint16_t connection_serial;
    uint16_t originator_vendor_id;
    uint32_t originator_serial;
    uint32_t o2t_actual_packet_interval_us;
    uint32_t t2o_actual_packet_interval_us;
    uint8_t connection_path[EIP_MAX_PATH_SIZE];
    size_t connection_path_length;
    uint16_t o2t_application_size;
    uint16_t t2o_application_size;
    uint16_t o2t_wire_size;
    uint16_t t2o_wire_size;
    bool o2t_run_idle_header;
    bool t2o_run_idle_header;
    uint8_t timeout_multiplier;
    uint8_t transport_type_trigger;
    char o2t_ip[64];
    uint16_t o2t_port;
    char t2o_ip[64];
    uint16_t t2o_port;
} eip_io_connection_t;

#endif
