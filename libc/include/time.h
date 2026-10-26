#ifndef PHOXTRA_LIBC_TIME_H
#define PHOXTRA_LIBC_TIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int64_t time_t;
struct tm { int tm_sec,tm_min,tm_hour,tm_mday,tm_mon,tm_year,tm_wday,tm_yday,tm_isdst; };
time_t time(time_t* timer);
struct tm* gmtime_r(const time_t* timer,struct tm* result);
#ifdef __cplusplus
}
#endif
#endif
