#include "rtc.h"
#include <stdint.h>
#define CMOS_INDEX 0x70u
#define CMOS_DATA 0x71u
#define UIP_BIT 0x80u
#define BINARY_MODE 0x04u
#define HOUR_24 0x02u
#define SNAPSHOT_TRIES 16u
#define UIP_TRIES 100000u
static inline void outb(uint16_t port,uint8_t value){__asm__ volatile("outb %0, %1"::"a"(value),"Nd"(port));}
static inline uint8_t inb(uint16_t port){uint8_t v;__asm__ volatile("inb %1, %0":"=a"(v):"Nd"(port));return v;}
static uint8_t cmos_read(uint8_t reg){
    uint8_t old_index=inb(CMOS_INDEX);
    outb(CMOS_INDEX,(uint8_t)((old_index&0x80u)|reg));
    uint8_t value=inb(CMOS_DATA);
    outb(CMOS_INDEX,old_index);
    return value;
}
static int bcd(uint8_t raw,uint8_t* value){
    if(!value||(raw&15u)>9u||((raw>>4)&15u)>9u)return -1;
    *value=(uint8_t)(((raw>>4)*10u)+(raw&15u));return 0;
}
static int decode(uint8_t raw,int binary,uint8_t* value){
    if(!value)return -1;
    if(binary){*value=raw;return 0;}
    return bcd(raw,value);
}
static int leap(uint32_t y){return y%4u==0u&&(y%100u!=0u||y%400u==0u);}
static uint32_t month_days(uint32_t y,uint32_t m){
    static const uint8_t days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(m<1u||m>12u)return 0;
    return m==2u&&leap(y)?29u:days[m-1u];
}
int rtc_snapshot_to_unix_time(const phoxtra_rtc_snapshot_t* s,uint64_t* output){
    if(!s||!output)return -1;*output=0;
    int binary=(s->status_b&BINARY_MODE)!=0;
    uint8_t sec,min,hour,day,month,year,century=0;
    if(decode(s->second,binary,&sec)||decode(s->minute,binary,&min)||
       decode((uint8_t)(s->hour&0x7Fu),binary,&hour)||decode(s->day,binary,&day)||
       decode(s->month,binary,&month)||decode(s->year,binary,&year))return -1;
    if(s->century&&s->century!=0xFFu&&decode(s->century,binary,&century)==0&&century>=19u&&century<=23u) {
        /* Use a plausible firmware century register. */
    } else century=20u;
    uint32_t full_year=(uint32_t)century*100u+year;
    if(!(s->status_b&HOUR_24)){
        int pm=(s->hour&0x80u)!=0;
        if(hour<1u||hour>12u)return -1;
        hour=(uint8_t)(hour%12u);if(pm)hour=(uint8_t)(hour+12u);
    }
    if(full_year<1970u||full_year>2199u||month<1u||month>12u||
       day<1u||day>month_days(full_year,month)||hour>23u||min>59u||sec>59u)return -1;
    uint64_t days=0;
    for(uint32_t y=1970u;y<full_year;++y)days+=leap(y)?366u:365u;
    for(uint32_t m=1u;m<month;++m)days+=month_days(full_year,m);
    days+=(uint64_t)day-1u;
    *output=days*86400u+(uint64_t)hour*3600u+(uint64_t)min*60u+sec;
    return 0;
}
static void read_snapshot(phoxtra_rtc_snapshot_t* s){
    s->second=cmos_read(0x00u);s->minute=cmos_read(0x02u);s->hour=cmos_read(0x04u);
    s->day=cmos_read(0x07u);s->month=cmos_read(0x08u);s->year=cmos_read(0x09u);
    s->century=cmos_read(0x32u);s->status_b=cmos_read(0x0Bu);
}
static int same_snapshot(const phoxtra_rtc_snapshot_t* a,const phoxtra_rtc_snapshot_t* b){
    return a->second==b->second&&a->minute==b->minute&&a->hour==b->hour&&
           a->day==b->day&&a->month==b->month&&a->year==b->year&&
           a->century==b->century&&a->status_b==b->status_b;
}
int rtc_get_unix_time(uint64_t* output){
    if(!output)return -1;*output=0;
    for(uint32_t tries=0;tries<SNAPSHOT_TRIES;++tries){
        uint32_t wait=0;while((cmos_read(0x0Au)&UIP_BIT)&&wait++<UIP_TRIES){}
        if(wait>=UIP_TRIES)continue;
        phoxtra_rtc_snapshot_t a,b;read_snapshot(&a);
        if(cmos_read(0x0Au)&UIP_BIT)continue;
        read_snapshot(&b);
        if((cmos_read(0x0Au)&UIP_BIT)||!same_snapshot(&a,&b))continue;
        return rtc_snapshot_to_unix_time(&b,output);
    }
    return -1;
}
