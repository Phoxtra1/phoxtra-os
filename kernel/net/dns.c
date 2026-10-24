#include "dns.h"
#include "udp.h"
#include "socket.h"
#include "ipv4.h"
#include "../include/timer.h"
#include "../log/logger.h"

#define DNS_PORT 53u
#define DNS_MAX_PACKET 512u
#define DNS_TCP_MAX_PACKET 4096u
#define DNS_MAX_NAME 254u
#define DNS_MAX_ANSWERS 32u
#define DNS_TIMEOUT_TICKS 300u
#define DNS_SEND_TIMEOUT_TICKS 400u
#define DNS_MAX_POLLS 20000000u

typedef struct dns_record {
    char owner[DNS_MAX_NAME];
    uint16_t type;
    uint16_t class_code;
    uint16_t data_length;
    uint32_t data_offset;
} dns_record_t;

typedef struct dns_query_context {
    netdev_t* device;
    uint16_t id;
    uint16_t local_port;
    char expected_name[DNS_MAX_NAME];
    uint32_t result_ip;
    uint8_t complete;
    uint8_t truncated;
} dns_query_context_t;

static uint16_t next_query_id = 0x5A00u;
static uint16_t next_local_port = 53000u;
/* Shared bounded scratch avoids an >8 KiB DNS answer array on the kernel stack.
   The current kernel executes these synchronous queries serially. */
static dns_record_t dns_answer_scratch[DNS_MAX_ANSWERS];
static uint8_t dns_tcp_response_scratch[DNS_TCP_MAX_PACKET];

static uint16_t read_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_be16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static int string_length_bounded(const char* value, uint32_t limit, uint32_t* length) {
    if (!value || !length) return 0;
    uint32_t i = 0;
    while (i < limit && value[i]) ++i;
    if (i == limit) return 0;
    *length = i;
    return 1;
}

static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int name_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (ascii_lower(*a) != ascii_lower(*b)) return 0;
        ++a; ++b;
    }
    return *a == *b;
}

static int normalize_hostname(const char* input, char output[DNS_MAX_NAME]) {
    uint32_t length = 0;
    if (!string_length_bounded(input, DNS_MAX_NAME, &length) || length == 0) return 0;
    if (input[length - 1u] == '.') --length;
    if (length == 0 || length > 253u) return 0;

    uint32_t label_length = 0;
    for (uint32_t i = 0; i < length; ++i) {
        char c = input[i];
        if (c == '.') {
            if (label_length == 0 || label_length > 63u || output[i - 1u] == '-') return 0;
            label_length = 0;
            output[i] = '.';
            continue;
        }
        if (c >= 'A' && c <= 'Z') c = ascii_lower(c);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
        if (label_length == 0 && c == '-') return 0;
        ++label_length;
        if (label_length > 63u) return 0;
        output[i] = c;
    }
    if (label_length == 0 || output[length - 1u] == '-') return 0;
    output[length] = '\0';
    return 1;
}

static uint32_t encode_name(const char* name, uint8_t* packet, uint32_t offset) {
    const char* label = name;
    const char* p = name;
    while (1) {
        if (*p == '.' || *p == '\0') {
            uint32_t length = (uint32_t)(p - label);
            if (length == 0 || length > 63u) return 0;
            packet[offset++] = (uint8_t)length;
            for (uint32_t i = 0; i < length; ++i) packet[offset++] = (uint8_t)label[i];
            if (*p == '\0') break;
            label = p + 1;
        }
        ++p;
    }
    packet[offset++] = 0;
    return offset;
}

static int read_dns_name(const uint8_t* packet, uint32_t length, uint32_t* offset,
                         char* output, uint32_t output_capacity) {
    if (!packet || !offset || !output || output_capacity == 0 || *offset >= length) return 0;
    uint32_t position = *offset;
    uint32_t resume = 0;
    uint32_t output_length = 0;
    uint32_t hops = 0;
    int jumped = 0;

    while (hops++ < length) {
        if (position >= length) return 0;
        uint8_t size = packet[position++];
        if (size == 0) {
            if (!jumped) resume = position;
            if (output_length >= output_capacity) return 0;
            output[output_length] = '\0';
            *offset = resume;
            return 1;
        }
        if ((size & 0xC0u) == 0xC0u) {
            if (position >= length) return 0;
            uint16_t pointer = (uint16_t)(((uint16_t)(size & 0x3Fu) << 8) | packet[position++]);
            if (pointer >= length) return 0;
            if (!jumped) { resume = position; jumped = 1; }
            position = pointer;
            continue;
        }
        if ((size & 0xC0u) != 0 || size > 63u || size > length - position) return 0;
        if (output_length != 0) {
            if (output_length + 1u >= output_capacity) return 0;
            output[output_length++] = '.';
        }
        if (output_length + size >= output_capacity) return 0;
        for (uint32_t i = 0; i < size; ++i)
            output[output_length++] = ascii_lower((char)packet[position + i]);
        position += size;
    }
    return 0;
}

static uint32_t build_query(uint8_t* packet, uint16_t id, const char* hostname) {
    for (uint32_t i = 0; i < DNS_MAX_PACKET; ++i) packet[i] = 0;
    write_be16(packet, id);
    write_be16(packet + 2, 0x0100u); /* Standard recursive query. */
    write_be16(packet + 4, 1u);
    uint32_t offset = encode_name(hostname, packet, 12u);
    if (!offset || offset + 4u > DNS_MAX_PACKET) return 0;
    write_be16(packet + offset, 1u); /* A */
    write_be16(packet + offset + 2u, 1u); /* IN */
    return offset + 4u;
}

static int valid_answer_ip(uint32_t ip) {
    uint8_t first = (uint8_t)ip;
    return ip != 0 && ip != UINT32_MAX && first != 0 && first < 224;
}

static int dns_parse_response(dns_query_context_t* query, const uint8_t* packet,
                               uint32_t length) {
    if (!query || !packet || length < 12u || length > DNS_TCP_MAX_PACKET) return -1;
    if (read_be16(packet) != query->id) return -1;
    uint16_t flags = read_be16(packet + 2u);
    if (!(flags & 0x8000u) || (flags & 0x7800u) != 0) return -1;
    if (read_be16(packet + 4u) != 1u) return -1;

    uint32_t offset = 12u;
    char question_name[DNS_MAX_NAME];
    if (!read_dns_name(packet, length, &offset, question_name, sizeof(question_name)) ||
        offset + 4u > length || !name_equal(question_name, query->expected_name) ||
        read_be16(packet + offset) != 1u || read_be16(packet + offset + 2u) != 1u)
        return -1;
    offset += 4u;

    /* A truncated UDP response must trigger TCP fallback after its question is verified. */
    if (flags & 0x0200u) {
        query->complete = 1;
        query->truncated = 1;
        query->result_ip = 0;
        return 0;
    }
    if ((flags & 0x000Fu) != 0) {
        query->complete = 1;
        query->truncated = 0;
        query->result_ip = 0;
        return 0;
    }

    uint16_t answer_count = read_be16(packet + 6u);
    if (answer_count > DNS_MAX_ANSWERS) return -1;
    dns_record_t* answers = dns_answer_scratch;
    for (uint32_t i = 0; i < answer_count; ++i) {
        if (!read_dns_name(packet, length, &offset, answers[i].owner, sizeof(answers[i].owner)) ||
            offset + 10u > length) return -1;
        answers[i].type = read_be16(packet + offset);
        answers[i].class_code = read_be16(packet + offset + 2u);
        answers[i].data_length = read_be16(packet + offset + 8u);
        offset += 10u;
        if (answers[i].data_length > length - offset) return -1;
        answers[i].data_offset = offset;
        offset += answers[i].data_length;
    }

    char current_name[DNS_MAX_NAME];
    for (uint32_t i = 0; query->expected_name[i] && i < DNS_MAX_NAME; ++i)
        current_name[i] = query->expected_name[i];
    uint32_t name_length = 0;
    if (!string_length_bounded(query->expected_name, DNS_MAX_NAME, &name_length)) return -1;
    current_name[name_length] = '\0';

    for (uint32_t hop = 0; hop < 8u; ++hop) {
        int followed_alias = 0;
        for (uint32_t i = 0; i < answer_count; ++i) {
            dns_record_t* answer = &answers[i];
            if (answer->class_code != 1u || !name_equal(answer->owner, current_name)) continue;
            if (answer->type == 1u && answer->data_length == 4u) {
                const uint8_t* address = packet + answer->data_offset;
                uint32_t ip = ip_to_uint32(address[0], address[1], address[2], address[3]);
                query->result_ip = valid_answer_ip(ip) ? ip : 0;
                query->complete = 1;
                return 0;
            }
            if (answer->type == 5u && answer->data_length > 0) {
                uint32_t cname_offset = answer->data_offset;
                char target[DNS_MAX_NAME];
                if (!read_dns_name(packet, length, &cname_offset, target, sizeof(target)) ||
                    cname_offset > answer->data_offset + answer->data_length) return -1;
                uint32_t target_length = 0;
                if (!string_length_bounded(target, sizeof(target), &target_length) ||
                    target_length == 0 || target_length >= sizeof(current_name)) return -1;
                for (uint32_t j = 0; j <= target_length; ++j) current_name[j] = target[j];
                followed_alias = 1;
                break;
            }
        }
        if (!followed_alias) break;
    }
    query->complete = 1;
    query->result_ip = 0;
    return 0;
}

static int dns_receive(netdev_t* dev, uint32_t source_ip, uint16_t source_port,
                       uint16_t destination_port, const uint8_t* packet,
                       uint32_t length, void* context) {
    dns_query_context_t* query = (dns_query_context_t*)context;
    if (!query || !dev || dev != query->device || source_ip != dev->dns_server ||
        source_port != DNS_PORT || destination_port != query->local_port ||
        !packet || length < 12u || length > DNS_MAX_PACKET) return -1;
    return dns_parse_response(query, packet, length);
}

static int dns_read_exact(int sockfd, uint8_t* buffer, uint32_t length) {
    if (sockfd < 0 || (!buffer && length)) return 0;
    uint32_t offset = 0;
    while (offset < length) {
        int received = sys_recv(sockfd, buffer + offset, length - offset, 0);
        if (received <= 0) return 0;
        offset += (uint32_t)received;
    }
    return 1;
}

static uint32_t dns_resolve_over_tcp(netdev_t* dev, dns_query_context_t* query) {
    /* The socket API currently uses the default NIC; do not silently switch interfaces. */
    if (!dev || !query || netdev_get_default() != dev) return 0;
    int sockfd = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) return 0;

    sockaddr_in_t remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(DNS_PORT);
    remote.sin_addr = dev->dns_server;
    if (sys_connect(sockfd, &remote) != 0) {
        (void)sys_close(sockfd);
        return 0;
    }

    uint8_t request[DNS_MAX_PACKET + 2u];
    uint32_t query_length = build_query(request + 2u, query->id, query->expected_name);
    if (!query_length || query_length > DNS_MAX_PACKET) {
        (void)sys_close(sockfd);
        return 0;
    }
    write_be16(request, (uint16_t)query_length);
    int sent = sys_send(sockfd, request, query_length + 2u, 0);
    if (sent != (int)(query_length + 2u)) {
        (void)sys_close(sockfd);
        return 0;
    }

    uint8_t length_prefix[2];
    if (!dns_read_exact(sockfd, length_prefix, sizeof(length_prefix))) {
        (void)sys_close(sockfd);
        return 0;
    }
    uint32_t response_length = read_be16(length_prefix);
    if (response_length < 12u || response_length > DNS_TCP_MAX_PACKET) {
        (void)sys_close(sockfd);
        return 0;
    }

    query->complete = 0;
    query->truncated = 0;
    query->result_ip = 0;
    uint32_t result = 0;
    if (dns_read_exact(sockfd, dns_tcp_response_scratch, response_length)) {
        int parsed = dns_parse_response(query, dns_tcp_response_scratch, response_length);
        if (parsed == 0 && query->complete && !query->truncated)
            result = query->result_ip;
    }
    (void)sys_close(sockfd);
    return result;
}

void dns_init(void) {
    klog(LOG_INFO, "DNS resolver initialized with bounded UDP parsing and TCP fallback for truncated responses");
}

uint32_t dns_resolve_hostname(netdev_t* dev, const char* hostname) {
    if (!dev || !dev->send_packet || !dev->ip_addr || !dev->netmask ||
        !dev->dns_server) {
        klog(LOG_WARNING, "DNS resolution requires a configured interface and DNS server");
        return 0;
    }

    dns_query_context_t query;
    query.device = dev;
    query.result_ip = 0;
    query.complete = 0;
    query.truncated = 0;
    query.local_port = 0;
    if (!normalize_hostname(hostname, query.expected_name)) {
        klog(LOG_WARNING, "DNS rejected an invalid hostname");
        return 0;
    }
    query.id = ++next_query_id;
    if (!query.id) query.id = ++next_query_id;

    int registered = 0;
    for (uint32_t attempt = 0; attempt < 16u; ++attempt) {
        uint16_t port = next_local_port++;
        if (next_local_port < 53000u || next_local_port >= 54000u) next_local_port = 53000u;
        if (udp_register_handler(port, dns_receive, &query) == 0) {
            query.local_port = port;
            registered = 1;
            break;
        }
    }
    if (!registered) {
        klog(LOG_WARNING, "DNS could not allocate a temporary UDP port");
        return 0;
    }

    uint8_t packet[DNS_MAX_PACKET];
    uint32_t packet_length = build_query(packet, query.id, query.expected_name);
    if (!packet_length) {
        (void)udp_unregister_handler(query.local_port, dns_receive, &query);
        return 0;
    }

    uint64_t send_start = timer_get_ticks();
    uint64_t last_attempt = send_start - 10u;
    uint32_t polls = 0;
    int sent = 0;
    while (!sent && !query.complete && polls++ < DNS_MAX_POLLS) {
        uint64_t now = timer_get_ticks();
        if (now - send_start >= DNS_SEND_TIMEOUT_TICKS) break;
        if (now - last_attempt >= 10u) {
            int result = udp_send_packet(dev, dev->dns_server, query.local_port,
                                         DNS_PORT, packet, packet_length);
            last_attempt = now;
            if (result >= 0) sent = 1;
        }
        if (!sent) net_poll();
    }

    if (sent) {
        uint64_t wait_start = timer_get_ticks();
        polls = 0;
        while (!query.complete && polls++ < DNS_MAX_POLLS) {
            net_poll();
            if (timer_get_ticks() - wait_start >= DNS_TIMEOUT_TICKS) break;
        }
    }

    (void)udp_unregister_handler(query.local_port, dns_receive, &query);
    if (query.truncated) {
        query.complete = 0;
        query.result_ip = 0;
        query.truncated = 0;
        uint32_t tcp_result = dns_resolve_over_tcp(dev, &query);
        if (tcp_result) return tcp_result;
        klog(LOG_WARNING, "DNS TCP fallback failed or returned no usable A record");
        return 0;
    }
    if (!query.complete || !query.result_ip) {
        klog(LOG_WARNING, "DNS query timed out or returned no usable A record");
        return 0;
    }
    return query.result_ip;
}
