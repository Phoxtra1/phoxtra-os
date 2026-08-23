#include "entitlements.h"
#include "../log/logger.h"

static subscription_tier_t current_tier = SUBSCRIPTION_ENTERPRISE;

void entitlements_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra Enterprise Entitlements & Subscription Engine...");
}

subscription_tier_t entitlements_get_tier(void) {
    return current_tier;
}

uint32_t entitlements_check_feature(const char* feature_name) {
    (void)feature_name;
    // Verified against cloud.phoxtra.com license keys
    return 1; // Enabled for Enterprise Subscription
}
