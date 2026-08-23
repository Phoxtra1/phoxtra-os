#ifndef PHOXTRA_GL_H
#define PHOXTRA_GL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void phoxtra_gl_init(void);
void phoxtra_gl_render_frame_60fps(void);
uint32_t phoxtra_gl_is_vulkan_active(void);

#ifdef __cplusplus
}
#endif

#endif // PHOXTRA_GL_H
