#include "../kernel/services/rtc.h"
#include <stdint.h>
#include <stdio.h>
#define CHECK(label,cond) do {if(!(cond)){fprintf(stderr,"FAIL: %s\n",label);return 1;}printf("PASS: %s\n",label);}while(0)
int main(void){
 uint64_t epoch=0;
 phoxtra_rtc_snapshot_t s={0x00,0x00,0x08,0x26,0x10,0x26,0x20,0x02};
 CHECK("BCD RTC decodes UTC date",rtc_snapshot_to_unix_time(&s,&epoch)==0&&epoch==1793001600ULL);
 s=(phoxtra_rtc_snapshot_t){59,59,23,29,2,24,20,0x06};
 CHECK("binary RTC handles leap day",rtc_snapshot_to_unix_time(&s,&epoch)==0&&epoch==1709251199ULL);
 s=(phoxtra_rtc_snapshot_t){0x05,0x04,0x83,0x26,0x10,0x26,0x20,0x00};
 CHECK("12-hour BCD RTC decodes PM",rtc_snapshot_to_unix_time(&s,&epoch)==0&&epoch==1793027045ULL);
 s=(phoxtra_rtc_snapshot_t){0,0,8,0x30,0x02,0x26,0x20,0x02};
 CHECK("RTC rejects impossible date",rtc_snapshot_to_unix_time(&s,&epoch)!=0);
 s=(phoxtra_rtc_snapshot_t){0,0,0x25,0x26,0x10,0x26,0x20,0x02};
 CHECK("RTC rejects invalid hour",rtc_snapshot_to_unix_time(&s,&epoch)!=0);
 puts("RTC conversion tests passed.");
 return 0;
}
