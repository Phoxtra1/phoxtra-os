#ifndef PHOXTRA_LOGGER_H
#define PHOXTRA_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_SECURITY,
    LOG_KERNEL,
    LOG_HARDWARE
} log_level_t;

void logger_init(void);
void klog(log_level_t level, const char* message);

#ifdef __cplusplus
}
#endif

#endif
