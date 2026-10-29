#include "../include/time.h"
#include <mbedtls/platform_time.h>
#include <stdint.h>
static int64_t get_time_syscall(void){register uint64_t rax __asm__("rax")=14u;__asm__ volatile("int $0x80":"+a"(rax)::"memory","cc");return (int64_t)rax;}
time_t time(time_t* timer){int64_t value=get_time_syscall();if(value<0)return (time_t)-1;if(timer)*timer=(time_t)value;return (time_t)value;}

static int64_t get_monotonic_ms_syscall(void) {
    register uint64_t rax __asm__("rax") = 15u;
    __asm__ volatile("int $0x80" : "+a"(rax) : : "memory", "cc");
    return (int64_t)rax;
}

/* TF-PSA-Crypto's freestanding monotonic clock hook. */
mbedtls_ms_time_t mbedtls_ms_time(void) {
    static mbedtls_ms_time_t last_ms;
    const int64_t now = get_monotonic_ms_syscall();
    if (now > (int64_t)last_ms) last_ms = (mbedtls_ms_time_t)now;
    return last_ms;
}
static int leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
struct tm* gmtime_r(const time_t* timer,struct tm* out){
 if(!timer||!out)return (struct tm*)0;
 int64_t days=(int64_t)*timer/86400,rem=(int64_t)*timer%86400;
 if(rem<0){rem+=86400;--days;}
 out->tm_hour=(int)(rem/3600);rem%=3600;out->tm_min=(int)(rem/60);out->tm_sec=(int)(rem%60);
 int year=1970;int64_t left=days;
 while(left<0){--year;left+=leap(year)?366:365;}
 while(left>=(leap(year)?366:365)){left-=leap(year)?366:365;++year;}
 out->tm_year=year-1900;out->tm_yday=(int)left;out->tm_wday=(int)((days+4)%7);if(out->tm_wday<0)out->tm_wday+=7;
 static const int mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 int month=0;while(month<11){int count=mdays[month]+((month==1&&leap(year))?1:0);if(left<count)break;left-=count;++month;}
 out->tm_mon=month;out->tm_mday=(int)left+1;out->tm_isdst=0;return out;
}
