#ifndef PHOXTRA_AUDIT_LOGGER_H
#define PHOXTRA_AUDIT_LOGGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIT_TYPE_HARDWARE_ATTESTATION 1
#define AUDIT_TYPE_PROCESS_BINDING      2
#define AUDIT_TYPE_PRIVILEGE_TRANSITION 3
#define AUDIT_TYPE_SYSCALL_GATE         4
#define AUDIT_TYPE_POLICY_VIOLATION     5

typedef struct {
    uint64_t timestamp;
    uint32_t event_type;
    uint32_t pid;
    char     message[64];
    uint8_t  event_hash[32];
} audit_event_t;

void audit_logger_init(void);
int audit_log_event(uint32_t type, uint32_t pid, const char* message);
uint32_t audit_get_count(void);
const audit_event_t* audit_get_event(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
