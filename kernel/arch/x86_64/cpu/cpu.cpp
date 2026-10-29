#include "../include/cpu.hpp"
#include "../include/fpu.h"
#include <stdint.h>
#include <stddef.h>

namespace {
alignas(16) static uint8_t default_fpu_state[512];
static bool default_fpu_state_ready = false;

static void initialize_fpu_state(void)
{
    const uint32_t default_mxcsr = 0x1F80u;

    __asm__ volatile ("fninit" ::: "memory");
    __asm__ volatile ("ldmxcsr %0" : : "m"(default_mxcsr) : "memory");
    /*
     * Do not seed new processes with residual XMM values left by firmware.
     * The kernel is built with -mno-sse, so explicit initialization here is
     * the only SIMD operation performed before process contexts are ready.
     */
    __asm__ volatile (
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        "pxor %%xmm3, %%xmm3\n\t"
        "pxor %%xmm4, %%xmm4\n\t"
        "pxor %%xmm5, %%xmm5\n\t"
        "pxor %%xmm6, %%xmm6\n\t"
        "pxor %%xmm7, %%xmm7\n\t"
        "pxor %%xmm8, %%xmm8\n\t"
        "pxor %%xmm9, %%xmm9\n\t"
        "pxor %%xmm10, %%xmm10\n\t"
        "pxor %%xmm11, %%xmm11\n\t"
        "pxor %%xmm12, %%xmm12\n\t"
        "pxor %%xmm13, %%xmm13\n\t"
        "pxor %%xmm14, %%xmm14\n\t"
        "pxor %%xmm15, %%xmm15"
        :
        :
        : "memory"
    );
    __asm__ volatile ("fxsave64 (%0)" : : "r"(default_fpu_state) : "memory");
    default_fpu_state_ready = true;
}
}

extern "C" void cpu_fpu_init_state(void* state)
{
    if (!state || !default_fpu_state_ready) return;
    uint8_t* destination = static_cast<uint8_t*>(state);
    for (size_t i = 0; i < sizeof(default_fpu_state); ++i)
        destination[i] = default_fpu_state[i];
}

extern "C" void cpu_fpu_save_state(void* state)
{
    if (!state) return;
    __asm__ volatile ("fxsave64 (%0)" : : "r"(state) : "memory");
}

extern "C" void cpu_fpu_restore_state(const void* state)
{
    if (!state) return;
    __asm__ volatile ("fxrstor64 (%0)" : : "r"(state) : "memory");
}

void cpu_init()
{
    initialize_fpu_state();
}
