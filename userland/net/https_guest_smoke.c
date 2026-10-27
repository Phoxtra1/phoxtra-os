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
        phoxtra_guest_test_ca_pem, sizeof(phoxtra_guest_test_ca_pem) - 1u,
        response_buffer, sizeof(response_buffer), &response_length);
    if (result == PHOXTRA_HTTPS_ERR_CONNECT) {
        guest_log("PHOXTRA_GUEST_HTTPS_CONNECT_FAILED"); return 1;
    }
    if (result == PHOXTRA_HTTPS_ERR_CERTIFICATE) {
        guest_log("PHOXTRA_GUEST_HTTPS_CERTIFICATE_FAILED"); return 1;
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
