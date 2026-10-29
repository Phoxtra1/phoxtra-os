#include "https_client.h"
#include "phoxtra/net.h"
#include "phoxtra_guest_test_ca.h"
#include <stddef.h>
#include <stdint.h>

static unsigned char response_buffer[4096];

static void guest_log(const char* message)
{
    register uint64_t rax __asm__("rax") = 1u;
    register uint64_t rdi __asm__("rdi") = (uint64_t)(uintptr_t)message;
    __asm__ volatile ("int $0x80" : "+a"(rax) : "D"(rdi) : "memory", "cc");
}

static void guest_log_hex(const char* label, uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char message[96];
    size_t length = 0;
    while (label[length] && length < sizeof(message) - 18u) {
        message[length] = label[length];
        ++length;
    }
    for (int shift = 60; shift >= 0; shift -= 4)
        message[length++] = digits[(value >> (unsigned)shift) & 0xFu];
    message[length] = '\0';
    guest_log(message);
}

static int64_t guest_time_unix(void)
{
    register uint64_t rax __asm__("rax") = 14u; /* SYS_TIME */
    __asm__ volatile ("int $0x80" : "+a"(rax) : : "memory", "cc");
    return (int64_t)rax;
}

static int contains_bytes(const unsigned char* data, size_t data_length, const char* text)
{
    size_t text_length = 0;
    if (!data || !text) return 0;
    while (text[text_length]) ++text_length;
    if (!text_length || text_length > data_length) return 0;
    for (size_t i = 0; i <= data_length - text_length; ++i) {
        size_t j = 0;
        while (j < text_length && data[i + j] == (unsigned char)text[j]) ++j;
        if (j == text_length) return 1;
    }
    return 0;
}

int phoxtra_https_guest_smoke_main(void)
{
    guest_log("PHOXTRA_GUEST_HTTPS_STARTED");
    size_t response_length = 0;
    const int result = phoxtra_https_get("10.0.2.2", "/",
        phoxtra_guest_test_ca_pem, sizeof(phoxtra_guest_test_ca_pem),
        response_buffer, sizeof(response_buffer), &response_length);
    if (result == PHOXTRA_HTTPS_ERR_CONNECT) {
        guest_log("PHOXTRA_GUEST_HTTPS_CONNECT_FAILED"); return 1;
    }
    if (result == PHOXTRA_HTTPS_ERR_CERTIFICATE) {
        guest_log("PHOXTRA_GUEST_HTTPS_CERTIFICATE_FAILED");
        guest_log_hex("PHOXTRA_GUEST_HTTPS_VERIFY_FLAGS=", phoxtra_https_last_verify_flags());
        guest_log_hex("PHOXTRA_GUEST_HTTPS_TIME_UNIX=", (uint64_t)guest_time_unix());
        return 1;
    }
    if (result == PHOXTRA_HTTPS_ERR_TLS) {
        guest_log("PHOXTRA_GUEST_HTTPS_TLS_FAILED"); return 1;
    }
    if (result != PHOXTRA_HTTPS_OK) {
        guest_log("PHOXTRA_GUEST_HTTPS_IO_OR_SETUP_FAILED"); return 1;
    }
    if (!contains_bytes(response_buffer, response_length, "HTTP/1.1 200 OK") ||
        !contains_bytes(response_buffer, response_length, "PHOXTRA_GUEST_HTTPS_OK")) {
        guest_log("PHOXTRA_GUEST_HTTPS_RESPONSE_FAILED"); return 1;
    }
    guest_log("PHOXTRA_GUEST_HTTPS_OK");
    return 0;
}
