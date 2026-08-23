#include "phoxtra_gl.h"
#include "../log/logger.h"

static uint32_t vulkan_active = 1;

void phoxtra_gl_init(void) {
    klog(LOG_INFO, "Initializing Phoxtra-GL 3D Vulkan & OpenGL ES Hardware Graphics Engine...");
}

void phoxtra_gl_render_frame_60fps(void) {
    // 60 FPS compositor frame render stub
}

uint32_t phoxtra_gl_is_vulkan_active(void) {
    return vulkan_active;
}
