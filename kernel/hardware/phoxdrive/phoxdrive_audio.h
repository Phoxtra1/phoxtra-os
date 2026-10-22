#ifndef PHOXDRIVE_AUDIO_H
#define PHOXDRIVE_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────
   Phoxdrive Voice / Audio Speak-out Alert Engine
   Synthesizes and broadcasts real-time spoken voice alerts
   to drivers for hazards, potholes, broken vehicles & speed.
   ────────────────────────────────────────────────────────── */

void        phoxdrive_audio_init(void);
void        phoxdrive_speak_alert(const char* spoken_text);
const char* phoxdrive_get_last_spoken_alert(void);
uint32_t    phoxdrive_get_spoken_alert_count(void);
bool        phoxdrive_audio_is_muted(void);
void        phoxdrive_audio_set_muted(bool muted);

#ifdef __cplusplus
}
#endif

#endif /* PHOXDRIVE_AUDIO_H */
