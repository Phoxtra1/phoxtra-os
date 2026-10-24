#ifndef PHOXTRA_TCP_H
#define PHOXTRA_TCP_H

#include "netdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

#define TCP_MSS 1460u
#define TCP_RECV_CAPACITY 4096u
#define TCP_MAX_LISTEN_BACKLOG 8u

typedef enum tcp_state {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

typedef struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

struct socket_entry;
typedef struct socket_entry socket_entry_t;

void tcp_init(void);
int tcp_receive_packet(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                       const uint8_t* packet, uint32_t length);
int tcp_connect_socket(socket_entry_t* socket, netdev_t* dev);
int tcp_send_data(socket_entry_t* socket, netdev_t* dev,
                  const uint8_t* payload, uint32_t length);
int tcp_receive_data(socket_entry_t* socket, netdev_t* dev,
                     uint8_t* buffer, uint32_t length);
int tcp_close_socket(socket_entry_t* socket, netdev_t* dev);
const char* tcp_state_to_string(tcp_state_t state);

#ifdef __cplusplus
}
#endif

#endif
