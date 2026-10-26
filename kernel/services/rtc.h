#ifndef PHOXTRA_SERVICES_RTC_H
#define PHOXTRA_SERVICES_RTC_H
#include <stdint.h>
typedef struct phoxtra_rtc_snapshot {
    uint8_t second, minute, hour, day, month, year, century, status_b;
} phoxtra_rtc_snapshot_t;
int rtc_snapshot_to_unix_time(const phoxtra_rtc_snapshot_t* snapshot,uint64_t* unix_seconds_out);
int rtc_get_unix_time(uint64_t* unix_seconds_out);
#endif
