#define _POSIX_C_SOURCE 200809L

#include "../userland/net/https_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <psa/crypto.h>
#include <psa/crypto_platform.h>
#include <mbedtls/platform_time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct test_transport {
    uint16_t port;
} test_transport_t;

static int64_t time_offset_seconds;

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (mbedtls_ms_time_t)now.tv_sec * 1000 +
           (mbedtls_ms_time_t)now.tv_nsec / 1000000;
}

time_t time(time_t* out)
{
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return (time_t)-1;
    }

    time_t value = now.tv_sec + (time_t)time_offset_seconds;
    if (out) {
        *out = value;
    }
    return value;
}

psa_status_t mbedtls_psa_external_get_random(
    mbedtls_psa_external_random_context_t* context,
    uint8_t* output,
    size_t size,
    size_t* output_size)
{
    (void)context;

    if (!output_size || (!output && size)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    *output_size = 0;

    size_t offset = 0;
    while (offset < size) {
        ssize_t count = getrandom(output + offset, size - offset, 0);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        }
        offset += (size_t)count;
    }

    *output_size = size;
    return PSA_SUCCESS;
}

static int host_connect(void* context, const char* host, uint16_t port)
{
    (void)host;
    (void)port;

    test_transport_t* transport = (test_transport_t*)context;
    if (!transport || !transport->port) {
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(transport->port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr*)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int host_send(
    void* context,
    int fd,
    const unsigned char* data,
    size_t length)
{
    (void)context;

    ssize_t count;
    do {
        count = send(fd, data, length, MSG_NOSIGNAL);
    } while (count < 0 && errno == EINTR);

    return count > INT_MAX ? -1 : (int)count;
}

static int host_recv(
    void* context,
    int fd,
    unsigned char* data,
    size_t length)
{
    (void)context;

    ssize_t count;
    do {
        count = recv(fd, data, length, 0);
    } while (count < 0 && errno == EINTR);

    return count > INT_MAX ? -1 : (int)count;
}

static int host_close(void* context, int fd)
{
    (void)context;
    return close(fd);
}

static unsigned char* read_file(const char* path, size_t* size_out)
{
    if (!path || !size_out) {
        return NULL;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0 || file_size > 4 * 1024 * 1024L) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    unsigned char* data = (unsigned char*)malloc((size_t)file_size + 1u);
    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(data, 1u, (size_t)file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        free(data);
        return NULL;
    }

    data[bytes_read] = 0;
    *size_out = bytes_read + 1u;
    return data;
}

static int contains(
    const unsigned char* data,
    size_t data_size,
    const char* needle)
{
    size_t needle_size = strlen(needle);
    if (!data || needle_size > data_size) {
        return 0;
    }

    for (size_t i = 0; i <= data_size - needle_size; ++i) {
        if (memcmp(data + i, needle, needle_size) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 6) {
        fprintf(stderr,
            "usage: %s port hostname ca.pem time-offset success|reject\n",
            argv[0]);
        return 2;
    }

    char* end = NULL;
    long port = strtol(argv[1], &end, 10);
    if (!end || *end || port < 1 || port > 65535) {
        return 2;
    }

    time_offset_seconds = (int64_t)strtoll(argv[4], NULL, 10);
    int expect_success = strcmp(argv[5], "success") == 0;
    int expect_certificate_failure = strcmp(argv[5], "reject-cert") == 0;
    if (!expect_success && !expect_certificate_failure &&
        strcmp(argv[5], "reject") != 0) {
        fprintf(stderr, "expectation must be success, reject-cert or reject\\n");
        return 2;
    }

    size_t ca_size = 0;
    unsigned char* ca = NULL;
    if (strcmp(argv[3], "-") != 0) {
        ca = read_file(argv[3], &ca_size);
        if (!ca) {
            fprintf(stderr, "cannot read trust PEM\n");
            return 2;
        }
    }

    test_transport_t transport = { (uint16_t)port };
    const phoxtra_https_io_t io = {
        &transport,
        host_connect,
        host_send,
        host_recv,
        host_close
    };

    unsigned char response[8192];
    size_t response_size = 0;
    int result = phoxtra_https_get_with_io(
        &io, argv[2], "/", ca, ca_size,
        response, sizeof(response), &response_size);

    int passed;
    if (expect_success) {
        passed = result == PHOXTRA_HTTPS_OK
            && phoxtra_https_last_verify_flags() == 0u
            && phoxtra_https_last_tls_error() == 0
            && contains(response, response_size, "HTTP/1.1 200 OK")
            && contains(response, response_size, "PHOXTRA_TLS_OK");
    } else if (expect_certificate_failure) {
        passed = result == PHOXTRA_HTTPS_ERR_CERTIFICATE
            && phoxtra_https_last_verify_flags() != 0u
            && phoxtra_https_last_verify_flags() != UINT32_MAX
            && phoxtra_https_last_tls_error() != 0
            && response_size == 0;
    } else {
        passed = result != PHOXTRA_HTTPS_OK && response_size == 0;
    }

    if (!passed) {
        fprintf(stderr,
            "FAIL: expectation=%s result=%d response=%zu\n",
            expect_success ? "success" : "reject",
            result,
            response_size);
        free(ca);
        return 1;
    }

    printf("PASS: HTTPS peer %s; result=%d\n",
        expect_success ? "verified" : "rejected", result);
    free(ca);
    return 0;
}
