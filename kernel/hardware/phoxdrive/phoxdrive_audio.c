#include "phoxdrive_audio.h"
#include "../../log/logger.h"

extern void console_write(const char* text);

static char s_last_spoken[128] = "System Initialized. Driver safety voice alerts active.";
static uint32_t s_alert_count = 0;
static bool s_audio_muted = false;

static void kstrncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void phoxdrive_audio_init(void) {
    s_alert_count = 0;
    s_audio_muted = false;
    klog(LOG_HARDWARE, "Phoxdrive Synthetic Voice Audio Alert Dispatcher initialized");
}

void phoxdrive_speak_alert(const char* spoken_text) {
    if (!spoken_text) return;
    if (s_audio_muted) {
        klog(LOG_HARDWARE, "[MUTED] Phoxdrive Spoken Alert suppressed");
        return;
    }

    s_alert_count++;
    kstrncpy(s_last_spoken, spoken_text, sizeof(s_last_spoken));

    /* Kernel log and formatted console notification */
    klog(LOG_HARDWARE, "[VOICE ALERT SPEAK-OUT] Spoken driver alert active");
}

const char* phoxdrive_get_last_spoken_alert(void) {
    return s_last_spoken;
}

uint32_t phoxdrive_get_spoken_alert_count(void) {
    return s_alert_count;
}

bool phoxdrive_audio_is_muted(void) {
    return s_audio_muted;
}

void phoxdrive_audio_set_muted(bool muted) {
    s_audio_muted = muted;
    klog(LOG_HARDWARE, "Phoxdrive Audio Speak-out Mute state changed");
}
