#ifndef PHOXTRA_ENTITLEMENTS_H
#define PHOXTRA_ENTITLEMENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUBSCRIPTION_COMMUNITY = 0,
    SUBSCRIPTION_PRO = 1,
    SUBSCRIPTION_ENTERPRISE = 2,
    SUBSCRIPTION_GOVERNMENT = 3
} subscription_tier_t;

void entitlements_init(void);
subscription_tier_t entitlements_get_tier(void);
uint32_t entitlements_check_feature(const char* feature_name);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_ENTITLEMENTS_H
