#ifndef PHOXTRA_X86_64_FPU_H
#define PHOXTRA_X86_64_FPU_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The boot path enables FXSR/SSE before cpu_init() is called. Each PCB owns
 * a 16-byte-aligned 512-byte FXSAVE image; these helpers use only integer
 * registers and do not let compiler-generated SIMD code touch the live state.
 */
void cpu_fpu_init_state(void* state);
void cpu_fpu_save_state(void* state);
void cpu_fpu_restore_state(const void* state);

#ifdef __cplusplus
}
#endif

#endif
