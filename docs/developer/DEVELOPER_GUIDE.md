# Phoxtra OS Developer Guide

Welcome to the official **Phoxtra OS Developer Guide**. This manual details how to construct, compile, link, and deploy Ring 3 user applications on Phoxtra OS using `phoxtra-gcc` and `phoxtra-sdk`.

---

## 1. Prerequisites & Toolchain Setup

The Phoxtra OS Developer SDK requires an x86_64 toolchain environment. The cross-compiler wrapper script `phoxtra-gcc` and build utility `phoxtra-sdk` are located in `sdk/tools/`.

Add the tools directory to your system path:

```bash
export PATH=$PATH:/path/to/phoxtra-os/sdk/tools
```

---

## 2. Creating a New Ring 3 Application

Use `phoxtra-sdk` to bootstrap a new user project:

```bash
phoxtra-sdk init my_app
```

This creates the canonical directory layout and a `main.c` starter template:

```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    printf("Akwaaba from Phoxtra OS User Application!\n");
    printf("Current PID: %d\n", getpid());
    return 0;
}
```

---

## 3. Compiling & Linking

Compile your application into a 64-bit ELF (`.elf`) binary:

```bash
phoxtra-sdk build
```

Under the hood, `phoxtra-gcc` executes:

```bash
x86_64-phoxtra-elf-gcc -ffreestanding -nostdlib -I/sysroot/libc/include -o app.elf src/main.c /sysroot/libc/libphoxtra-c.a
```

---

## 4. Deploying to Phoxtra OS VFS

Deploy the compiled executable binary directly to the virtual filesystem `/boot` image:

```bash
phoxtra-sdk deploy
```

In the Phoxtra kernel shell, run your program:

```bash
phoxtra> elf /boot/app.elf
```
