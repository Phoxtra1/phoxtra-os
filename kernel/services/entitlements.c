#include "entitlements.h"
#include "../log/logger.h"
static subscription_tier_t current_tier = SUBSCRIPTION_COMMUNITY;
void entitlements_init(void) {
    klog(LOG_WARNING, "Cloud licensing is not connected; prototype grants no paid or enterprise entitlements");
}
subscription_tier_t entitlements_get_tier(void) { return current_tier; }
uint32_t entitlements_check_feature(const char* feature_name) {
    (void)feature_name;
    return 0;
}
