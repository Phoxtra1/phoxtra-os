#include "tcp.h"
#include "socket.h"
#include "ipv4.h"
#include "../include/timer.h"
#include "../log/logger.h"

#define TCP_MAX_SEGMENT 1480u
#define TCP_CONNECT_TIMEOUT_TICKS 600u
#define TCP_IO_TIMEOUT_TICKS 500u
#define TCP_RETRY_TICKS 100u
#define TCP_MAX_RETRIES 5u
#define TCP_MAX_POLLS 20000000u

static int sequence_between(uint32_t value, uint32_t low, uint32_t high) {
    return (int32_t)(value - low) >= 0 && (int32_t)(high - value) >= 0;
}

static uint32_t socket_receive_window(const socket_entry_t* socket) {
    if (!socket || socket->tcp_rx_count >= TCP_RECV_CAPACITY) return 0;
    return TCP_RECV_CAPACITY - socket->tcp_rx_count;
}

static int tcp_send_segment(netdev_t* dev, uint32_t local_ip, uint16_t local_port,
                            uint32_t remote_ip, uint16_t remote_port,
                            uint32_t sequence, uint32_t acknowledgement,
                            uint8_t flags, const uint8_t* payload,
                            uint32_t payload_length, uint32_t receive_window) {
    if (!dev || !dev->send_packet || !local_ip || !remote_ip ||
        !local_port || !remote_port || (!payload && payload_length) ||
        payload_length > TCP_MSS) return -1;

    uint32_t options_length = (flags & TCP_FLAG_SYN) ? 4u : 0u;
    if (payload_length + sizeof(tcp_header_t) + options_length > TCP_MAX_SEGMENT) return -1;

    uint8_t segment[TCP_MAX_SEGMENT];
    for (uint32_t i = 0; i < sizeof(tcp_header_t) + options_length + payload_length; ++i)
        segment[i] = 0;
    tcp_header_t* header = (tcp_header_t*)segment;
    header->src_port = htons(local_port);
    header->dest_port = htons(remote_port);
    header->seq_num = htonl(sequence);
    header->ack_num = htonl(acknowledgement);
    header->data_offset = (uint8_t)(((sizeof(tcp_header_t) + options_length) / 4u) << 4);
    header->flags = flags;
    if (receive_window > 65535u) receive_window = 65535u;
    header->window_size = htons((uint16_t)receive_window);
    header->checksum = 0;
    header->urgent_ptr = 0;

    if (options_length) {
        /* Advertise the supported maximum segment size. */
        segment[sizeof(tcp_header_t)] = 2u;
        segment[sizeof(tcp_header_t) + 1u] = 4u;
        segment[sizeof(tcp_header_t) + 2u] = (uint8_t)(TCP_MSS >> 8);
        segment[sizeof(tcp_header_t) + 3u] = (uint8_t)TCP_MSS;
    }
    uint32_t header_length = (uint32_t)sizeof(tcp_header_t) + options_length;
    for (uint32_t i = 0; i < payload_length; ++i) segment[header_length + i] = payload[i];
    uint32_t segment_length = header_length + payload_length;

    uint8_t pseudo[12u + TCP_MAX_SEGMENT];
    const uint8_t* src_bytes = (const uint8_t*)&local_ip;
    const uint8_t* dst_bytes = (const uint8_t*)&remote_ip;
    for (uint32_t i = 0; i < 4u; ++i) {
        pseudo[i] = src_bytes[i];
        pseudo[4u + i] = dst_bytes[i];
    }
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_TCP;
    pseudo[10] = (uint8_t)(segment_length >> 8);
    pseudo[11] = (uint8_t)segment_length;
    for (uint32_t i = 0; i < segment_length; ++i) pseudo[12u + i] = segment[i];
    header->checksum = net_checksum(pseudo, 12u + segment_length);

    int result = ipv4_send_packet(dev, remote_ip, IPPROTO_TCP, segment, segment_length);
    return result < 0 ? result : 0;
}

static int tcp_checksum_valid(uint32_t source_ip, uint32_t destination_ip,
                              const uint8_t* segment, uint32_t length) {
    if (!segment || length < sizeof(tcp_header_t) || length > TCP_MAX_SEGMENT) return 0;
    uint8_t pseudo[12u + TCP_MAX_SEGMENT];
    const uint8_t* src_bytes = (const uint8_t*)&source_ip;
    const uint8_t* dst_bytes = (const uint8_t*)&destination_ip;
    for (uint32_t i = 0; i < 4u; ++i) {
        pseudo[i] = src_bytes[i];
        pseudo[4u + i] = dst_bytes[i];
    }
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_TCP;
    pseudo[10] = (uint8_t)(length >> 8);
    pseudo[11] = (uint8_t)length;
    for (uint32_t i = 0; i < length; ++i) pseudo[12u + i] = segment[i];
    return net_checksum(pseudo, 12u + length) == 0;
}

static int send_reset(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                      const tcp_header_t* incoming, uint32_t payload_length) {
    if (!incoming || (incoming->flags & TCP_FLAG_RST)) return 0;
    uint32_t incoming_seq = ntohl(incoming->seq_num);
    uint32_t incoming_ack = ntohl(incoming->ack_num);
    uint32_t local_port = ntohs(incoming->dest_port);
    uint32_t remote_port = ntohs(incoming->src_port);
    if (incoming->flags & TCP_FLAG_ACK) {
        return tcp_send_segment(dev, destination_ip, (uint16_t)local_port,
                                source_ip, (uint16_t)remote_port,
                                incoming_ack, 0, TCP_FLAG_RST, 0, 0, 0);
    }
    uint32_t consumed = payload_length;
    if (incoming->flags & TCP_FLAG_SYN) ++consumed;
    if (incoming->flags & TCP_FLAG_FIN) ++consumed;
    return tcp_send_segment(dev, destination_ip, (uint16_t)local_port,
                            source_ip, (uint16_t)remote_port, 0,
                            incoming_seq + consumed, TCP_FLAG_RST | TCP_FLAG_ACK,
                            0, 0, 0);
}

static socket_entry_t* find_connection(uint32_t source_ip, uint32_t destination_ip,
                                       uint16_t source_port, uint16_t destination_port) {
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* socket = socket_get_by_id(i);
        if (!socket || socket->type != SOCK_STREAM || socket->state == TCP_STATE_LISTEN ||
            socket->state == TCP_STATE_CLOSED) continue;
        if (socket->local_port == destination_port && socket->remote_port == source_port &&
            socket->remote_ip == source_ip &&
            (!socket->local_ip || socket->local_ip == destination_ip)) return socket;
    }
    return 0;
}

static socket_entry_t* find_listener(uint32_t destination_ip, uint16_t destination_port) {
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* socket = socket_get_by_id(i);
        if (socket && socket->type == SOCK_STREAM && socket->state == TCP_STATE_LISTEN &&
            socket->local_port == destination_port &&
            (!socket->local_ip || socket->local_ip == destination_ip)) return socket;
    }
    return 0;
}

static uint32_t listener_child_count(const socket_entry_t* listener) {
    uint32_t count = 0;
    if (!listener) return 0;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* child = socket_get_by_id(i);
        if (child && child->parent_id == listener->id && child->type == SOCK_STREAM &&
            child->state != TCP_STATE_CLOSED) ++count;
    }
    return count;
}

static void queue_accepted_child(socket_entry_t* child) {
    if (!child || child->parent_id < 0) return;
    socket_entry_t* listener = socket_get_by_id(child->parent_id);
    if (!listener || listener->state != TCP_STATE_LISTEN ||
        listener->pending_count >= listener->listen_backlog) {
        child->error = -1;
        child->state = TCP_STATE_CLOSED;
        child->in_use = 0;
        return;
    }
    uint8_t slot = (uint8_t)((listener->pending_head + listener->pending_count) %
                             TCP_MAX_LISTEN_BACKLOG);
    listener->pending_children[slot] = child->id;
    ++listener->pending_count;
}

void tcp_poll(void) {
    uint64_t now = timer_get_ticks();
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        socket_entry_t* socket = socket_get_by_id(i);
        if (!socket || socket->type != SOCK_STREAM ||
            socket->state != TCP_STATE_SYN_RECEIVED) continue;
        if (now - socket->tcp_tx_sent_at < TCP_RETRY_TICKS) continue;
        if (socket->tcp_tx_retries >= TCP_MAX_RETRIES) {
            socket->error = -2;
            socket->state = TCP_STATE_CLOSED;
            socket->in_use = 0;
            continue;
        }
        netdev_t* dev = netdev_get_default();
        if (!dev) continue;
        int result = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                      socket->remote_ip, socket->remote_port,
                                      socket->snd_una, socket->rcv_nxt,
                                      TCP_FLAG_SYN | TCP_FLAG_ACK, 0, 0,
                                      socket_receive_window(socket));
        socket->tcp_tx_sent_at = now;
        ++socket->tcp_tx_retries;
        if (result < 0 && result != -2) {
            socket->error = result;
            socket->state = TCP_STATE_CLOSED;
            socket->in_use = 0;
        }
    }
}

static int accept_syn(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                      const tcp_header_t* incoming, uint32_t sequence) {
    uint16_t source_port = ntohs(incoming->src_port);
    uint16_t destination_port = ntohs(incoming->dest_port);
    socket_entry_t* listener = find_listener(destination_ip, destination_port);
    if (!listener || listener_child_count(listener) >= listener->listen_backlog) {
        return send_reset(dev, source_ip, destination_ip, incoming, 0);
    }
    int child_id = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socket_entry_t* child = socket_get_by_id(child_id);
    if (!child) return send_reset(dev, source_ip, destination_ip, incoming, 0);
    child->local_ip = destination_ip;
    child->local_port = destination_port;
    child->remote_ip = source_ip;
    child->remote_port = source_port;
    child->parent_id = listener->id;
    child->rcv_nxt = sequence + 1u;
    child->snd_una = (uint32_t)timer_get_ticks() ^ destination_ip ^
                     ((uint32_t)destination_port << 16) ^ source_ip ^ source_port;
    if (!child->snd_una) child->snd_una = 0x50484F58u;
    child->snd_nxt = child->snd_una + 1u;
    child->state = TCP_STATE_SYN_RECEIVED;
    child->tcp_tx_sent_at = timer_get_ticks();
    child->tcp_tx_retries = 0;
    int result = tcp_send_segment(dev, child->local_ip, child->local_port, child->remote_ip,
                                  child->remote_port, child->snd_una, child->rcv_nxt,
                                  TCP_FLAG_SYN | TCP_FLAG_ACK, 0, 0,
                                  socket_receive_window(child));
    /* ARP resolution is asynchronous: keep the half-open socket and retry after net_poll. */
    if (result < 0 && result != -2) {
        child->state = TCP_STATE_CLOSED;
        child->in_use = 0;
        return result;
    }
    return 0;
}

int tcp_receive_packet(netdev_t* dev, uint32_t source_ip, uint32_t destination_ip,
                       const uint8_t* packet, uint32_t length) {
    if (!dev || !packet || length < sizeof(tcp_header_t) ||
        length > TCP_MAX_SEGMENT || !source_ip || source_ip == UINT32_MAX)
        return -1;
    if (!tcp_checksum_valid(source_ip, destination_ip, packet, length)) return -1;

    const tcp_header_t* header = (const tcp_header_t*)packet;
    uint8_t offset_words = (uint8_t)(header->data_offset >> 4);
    if (offset_words < 5u || (header->data_offset & 0x0Fu) != 0u) return -1;
    uint32_t header_length = (uint32_t)offset_words * 4u;
    if (header_length > length || !ntohs(header->src_port) || !ntohs(header->dest_port))
        return -1;
    uint32_t payload_length = length - header_length;
    uint32_t sequence = ntohl(header->seq_num);
    uint32_t acknowledgement = ntohl(header->ack_num);
    uint16_t source_port = ntohs(header->src_port);
    uint16_t destination_port = ntohs(header->dest_port);
    uint8_t flags = header->flags;

    socket_entry_t* socket = find_connection(source_ip, destination_ip,
                                               source_port, destination_port);
    if (!socket) {
        if ((flags & TCP_FLAG_SYN) && !(flags & (TCP_FLAG_ACK | TCP_FLAG_RST)))
            return accept_syn(dev, source_ip, destination_ip, header, sequence);
        return send_reset(dev, source_ip, destination_ip, header, payload_length);
    }

    if (socket->state == TCP_STATE_SYN_RECEIVED &&
        (flags & TCP_FLAG_SYN) && !(flags & (TCP_FLAG_ACK | TCP_FLAG_RST)) &&
        sequence + 1u == socket->rcv_nxt) {
        int result = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                      socket->remote_ip, socket->remote_port,
                                      socket->snd_una, socket->rcv_nxt,
                                      TCP_FLAG_SYN | TCP_FLAG_ACK, 0, 0,
                                      socket_receive_window(socket));
        socket->tcp_tx_sent_at = timer_get_ticks();
        if (result < 0 && result != -2) return result;
        return 0;
    }

    if (flags & TCP_FLAG_RST) {
        socket->error = -1;
        socket->peer_closed = 1;
        socket->state = TCP_STATE_CLOSED;
        return 0;
    }

    if (socket->state == TCP_STATE_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) != (TCP_FLAG_SYN | TCP_FLAG_ACK) ||
            acknowledgement != socket->snd_nxt) return -1;
        socket->snd_una = acknowledgement;
        socket->rcv_nxt = sequence + 1u;
        socket->state = TCP_STATE_ESTABLISHED;
        return tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                socket->remote_ip, socket->remote_port,
                                socket->snd_nxt, socket->rcv_nxt, TCP_FLAG_ACK,
                                0, 0, socket_receive_window(socket));
    }

    if (socket->state == TCP_STATE_SYN_RECEIVED) {
        if (!(flags & TCP_FLAG_ACK) || acknowledgement != socket->snd_nxt ||
            sequence != socket->rcv_nxt) {
            return send_reset(dev, source_ip, destination_ip, header, payload_length);
        }
        socket->snd_una = acknowledgement;
        socket->state = TCP_STATE_ESTABLISHED;
        queue_accepted_child(socket);
        /* Continue below: the final ACK may carry initial application data. */
    } else if (!(flags & TCP_FLAG_ACK) && socket->state != TCP_STATE_LISTEN) {
        return -1;
    }

    if (flags & TCP_FLAG_ACK) {
        if (sequence_between(acknowledgement, socket->snd_una, socket->snd_nxt)) {
            socket->snd_una = acknowledgement;
            if (socket->tcp_tx_length &&
                acknowledgement == socket->tcp_tx_seq + socket->tcp_tx_length) {
                socket->tcp_tx_length = 0;
                socket->tcp_tx_retries = 0;
            }
            if (socket->state == TCP_STATE_LAST_ACK && acknowledgement == socket->snd_nxt)
                socket->state = TCP_STATE_CLOSED;
        }
    }

    if (payload_length || (flags & TCP_FLAG_FIN)) {
        if (sequence != socket->rcv_nxt) {
            (void)tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                   socket->remote_ip, socket->remote_port,
                                   socket->snd_nxt, socket->rcv_nxt, TCP_FLAG_ACK,
                                   0, 0, socket_receive_window(socket));
            return -2;
        }
        uint32_t free_space = socket_receive_window(socket);
        if (payload_length > free_space) {
            (void)tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                   socket->remote_ip, socket->remote_port,
                                   socket->snd_nxt, socket->rcv_nxt, TCP_FLAG_ACK,
                                   0, 0, free_space);
            return -2;
        }
        for (uint32_t i = 0; i < payload_length; ++i) {
            uint32_t slot = (socket->tcp_rx_head + socket->tcp_rx_count) % TCP_RECV_CAPACITY;
            socket->tcp_rx_buffer[slot] = packet[header_length + i];
            ++socket->tcp_rx_count;
        }
        socket->rcv_nxt += payload_length;

        if (flags & TCP_FLAG_FIN) {
            ++socket->rcv_nxt;
            socket->peer_closed = 1;
            if (socket->state == TCP_STATE_FIN_WAIT) socket->state = TCP_STATE_TIME_WAIT;
            else if (socket->state == TCP_STATE_ESTABLISHED) socket->state = TCP_STATE_CLOSE_WAIT;
        }
        return tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                socket->remote_ip, socket->remote_port,
                                socket->snd_nxt, socket->rcv_nxt, TCP_FLAG_ACK,
                                0, 0, socket_receive_window(socket));
    }

    return 0;
}

int tcp_connect_socket(socket_entry_t* socket, netdev_t* dev) {
    if (!socket || !socket->in_use || socket->type != SOCK_STREAM || !dev ||
        !dev->ip_addr || !socket->remote_ip || !socket->remote_port ||
        !socket->local_port) return -1;
    socket->local_ip = dev->ip_addr;
    socket->snd_una = (uint32_t)timer_get_ticks() ^ socket->local_ip ^
                      ((uint32_t)socket->local_port << 16) ^ socket->remote_ip ^
                      socket->remote_port;
    if (!socket->snd_una) socket->snd_una = 0x50484F58u;
    uint32_t initial_sequence = socket->snd_una;
    socket->snd_nxt = initial_sequence + 1u;
    socket->rcv_nxt = 0;
    socket->error = 0;
    socket->peer_closed = 0;
    socket->state = TCP_STATE_SYN_SENT;
    int sent = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                socket->remote_ip, socket->remote_port,
                                initial_sequence, 0, TCP_FLAG_SYN, 0, 0,
                                TCP_RECV_CAPACITY);
    /* -2 means routing/ARP has deferred transmission; the bounded retry loop handles it. */
    if (sent < 0 && sent != -2) {
        socket->state = TCP_STATE_CLOSED;
        socket->error = sent;
        return sent;
    }

    uint64_t started = timer_get_ticks();
    uint64_t last_sent = started;
    uint32_t polls = 0;
    uint8_t retries = 0;
    while (socket->state == TCP_STATE_SYN_SENT &&
           !socket->error && polls++ < TCP_MAX_POLLS) {
        net_poll();
        uint64_t now = timer_get_ticks();
        if (now - started >= TCP_CONNECT_TIMEOUT_TICKS) break;
        if (now - last_sent >= TCP_RETRY_TICKS) {
            if (retries >= TCP_MAX_RETRIES) break;
            int retry_result = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                                socket->remote_ip, socket->remote_port,
                                                initial_sequence, 0, TCP_FLAG_SYN, 0, 0,
                                                TCP_RECV_CAPACITY);
            if (retry_result < 0 && retry_result != -2) {
                socket->error = retry_result;
                break;
            }
            ++retries;
            last_sent = now;
        }
    }
    if (socket->state != TCP_STATE_ESTABLISHED) {
        socket->state = TCP_STATE_CLOSED;
        socket->error = socket->error ? socket->error : -2;
        return socket->error;
    }
    return 0;
}

static int tcp_wait_for_ack(socket_entry_t* socket, netdev_t* dev) {
    if (!socket || !dev) return -1;
    uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (socket->tcp_tx_length && polls++ < TCP_MAX_POLLS) {
        net_poll();
        if (!socket->tcp_tx_length) return 0;
        uint64_t now = timer_get_ticks();
        if (now - started >= TCP_IO_TIMEOUT_TICKS) return -2;
        if (now - socket->tcp_tx_sent_at >= TCP_RETRY_TICKS) {
            if (socket->tcp_tx_retries >= TCP_MAX_RETRIES) return -2;
            uint32_t offset = socket->snd_una > socket->tcp_tx_seq
                ? socket->snd_una - socket->tcp_tx_seq : 0;
            if (offset >= socket->tcp_tx_length) {
                socket->tcp_tx_length = 0;
                return 0;
            }
            uint32_t remaining = socket->tcp_tx_length - offset;
            int retry_result = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                                socket->remote_ip, socket->remote_port,
                                                socket->tcp_tx_seq + offset, socket->rcv_nxt,
                                                TCP_FLAG_ACK | TCP_FLAG_PSH,
                                                socket->tcp_tx_buffer + offset, remaining,
                                                socket_receive_window(socket));
            if (retry_result < 0 && retry_result != -2) return -2;
            socket->tcp_tx_sent_at = now;
            ++socket->tcp_tx_retries;
        }
    }
    return socket->tcp_tx_length ? -2 : 0;
}

int tcp_send_data(socket_entry_t* socket, netdev_t* dev,
                  const uint8_t* payload, uint32_t length) {
    if (!socket || !socket->in_use || socket->type != SOCK_STREAM ||
        !dev || !payload || !length) return -1;
    if (socket->state != TCP_STATE_ESTABLISHED || socket->peer_closed) return -2;
    if (tcp_wait_for_ack(socket, dev) < 0) return -2;

    uint32_t send_length = length > TCP_MSS ? TCP_MSS : length;
    for (uint32_t i = 0; i < send_length; ++i) socket->tcp_tx_buffer[i] = payload[i];
    socket->tcp_tx_seq = socket->snd_nxt;
    socket->tcp_tx_length = (uint16_t)send_length;
    socket->tcp_tx_retries = 0;
    socket->tcp_tx_sent_at = timer_get_ticks();
    socket->snd_nxt += send_length;

    int send_result = tcp_send_segment(dev, socket->local_ip, socket->local_port,
                                       socket->remote_ip, socket->remote_port,
                                       socket->tcp_tx_seq, socket->rcv_nxt,
                                       TCP_FLAG_ACK | TCP_FLAG_PSH, socket->tcp_tx_buffer,
                                       send_length, socket_receive_window(socket));
    if (send_result < 0 && send_result != -2) {
        socket->snd_nxt -= send_length;
        socket->tcp_tx_length = 0;
        return send_result;
    }
    /* Keep the segment outstanding when ARP/route resolution defers the first send. */
    return tcp_wait_for_ack(socket, dev) == 0 ? (int)send_length : -2;
}

int tcp_receive_data(socket_entry_t* socket, netdev_t* dev,
                     uint8_t* buffer, uint32_t length) {
    if (!socket || !socket->in_use || socket->type != SOCK_STREAM ||
        !dev || (!buffer && length)) return -1;
    if (!length) return 0;
    if (socket->state != TCP_STATE_ESTABLISHED && socket->state != TCP_STATE_CLOSE_WAIT &&
        socket->state != TCP_STATE_FIN_WAIT && socket->state != TCP_STATE_TIME_WAIT)
        return socket->error ? socket->error : -2;

    uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (!socket->tcp_rx_count && !socket->peer_closed && !socket->error &&
           polls++ < TCP_MAX_POLLS) {
        net_poll();
        if (timer_get_ticks() - started >= TCP_IO_TIMEOUT_TICKS) return -2;
    }
    if (!socket->tcp_rx_count) return socket->error ? socket->error : 0;

    uint32_t copy_length = length < socket->tcp_rx_count ? length : socket->tcp_rx_count;
    for (uint32_t i = 0; i < copy_length; ++i) {
        buffer[i] = socket->tcp_rx_buffer[socket->tcp_rx_head];
        socket->tcp_rx_head = (socket->tcp_rx_head + 1u) % TCP_RECV_CAPACITY;
        --socket->tcp_rx_count;
    }
    (void)tcp_send_segment(dev, socket->local_ip, socket->local_port,
                           socket->remote_ip, socket->remote_port,
                           socket->snd_nxt, socket->rcv_nxt, TCP_FLAG_ACK,
                           0, 0, socket_receive_window(socket));
    return (int)copy_length;
}

int tcp_close_socket(socket_entry_t* socket, netdev_t* dev) {
    if (!socket || !socket->in_use || socket->type != SOCK_STREAM || !dev) return -1;
    if (socket->state == TCP_STATE_CLOSED || socket->state == TCP_STATE_LISTEN) {
        socket->state = TCP_STATE_CLOSED;
        return 0;
    }
    if (socket->state == TCP_STATE_SYN_SENT || socket->state == TCP_STATE_SYN_RECEIVED) {
        (void)tcp_send_segment(dev, socket->local_ip, socket->local_port,
                               socket->remote_ip, socket->remote_port,
                               socket->snd_nxt, socket->rcv_nxt,
                               TCP_FLAG_RST | TCP_FLAG_ACK, 0, 0, 0);
        socket->state = TCP_STATE_CLOSED;
        return 0;
    }

    uint8_t old_state = (uint8_t)socket->state;
    uint8_t target_state = old_state == TCP_STATE_CLOSE_WAIT ? TCP_STATE_LAST_ACK : TCP_STATE_FIN_WAIT;
    uint32_t fin_sequence = socket->snd_nxt;
    if (tcp_send_segment(dev, socket->local_ip, socket->local_port,
                         socket->remote_ip, socket->remote_port,
                         fin_sequence, socket->rcv_nxt,
                         TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0,
                         socket_receive_window(socket)) < 0) return -2;
    ++socket->snd_nxt;
    socket->state = (tcp_state_t)target_state;
    uint64_t started = timer_get_ticks();
    uint32_t polls = 0;
    while (socket->state != TCP_STATE_CLOSED && socket->state != TCP_STATE_TIME_WAIT &&
           polls++ < TCP_MAX_POLLS) {
        net_poll();
        if (timer_get_ticks() - started >= TCP_IO_TIMEOUT_TICKS) break;
    }
    if (socket->state == TCP_STATE_TIME_WAIT || socket->state == TCP_STATE_CLOSED) {
        socket->state = TCP_STATE_CLOSED;
        return 0;
    }
    socket->state = TCP_STATE_CLOSED;
    return -2;
}

void tcp_init(void) {
    klog(LOG_INFO, "TCP checksum validation, connection handshake, bounded stream buffers, and retransmission support initialized");
}

const char* tcp_state_to_string(tcp_state_t state) {
    switch (state) {
        case TCP_STATE_CLOSED:       return "CLOSED";
        case TCP_STATE_LISTEN:       return "LISTEN";
        case TCP_STATE_SYN_SENT:     return "SYN_SENT";
        case TCP_STATE_SYN_RECEIVED: return "SYN_RECEIVED";
        case TCP_STATE_ESTABLISHED:  return "ESTABLISHED";
        case TCP_STATE_FIN_WAIT:     return "FIN_WAIT";
        case TCP_STATE_CLOSE_WAIT:   return "CLOSE_WAIT";
        case TCP_STATE_LAST_ACK:     return "LAST_ACK";
        case TCP_STATE_TIME_WAIT:    return "TIME_WAIT";
        default:                     return "UNKNOWN";
    }
}
