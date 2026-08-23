# Phoxtra OS System Call Reference

Phoxtra OS utilizes Model-Specific Register (MSR `0xC0000082`) Fast System Call Gate routing via the x86_64 `syscall` instruction. System calls transition CPU execution state from Ring 3 (User Mode) to Ring 0 (Kernel Privilege Mode).

---

## System Call Vector Table

| Syscall Vector | Name | Parameters | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `sys_read` | `int fd, void* buf, size_t count` | Read bytes from VFS file descriptor |
| `0x01` | `sys_write` | `int fd, const void* buf, size_t count` | Write bytes to VFS file descriptor or console |
| `0x02` | `sys_open` | `const char* pathname, int flags` | Open VFS file and return file descriptor |
| `0x03` | `sys_close` | `int fd` | Close open VFS file descriptor |
| `0x18` | `sys_socket` | `int domain, int type, int protocol` | Create BSD socket endpoint |
| `0x19` | `sys_bind` | `int sockfd, const struct sockaddr* addr` | Bind BSD socket to local IP / Port |
| `0x1A` | `sys_connect` | `int sockfd, const struct sockaddr* addr` | Establish TCP connection to target |
| `0x1B` | `sys_listen` | `int sockfd, int backlog` | Set TCP socket into listening state |
| `0x1C` | `sys_send` | `int sockfd, const void* buf, size_t len` | Send packet over BSD socket |
| `0x1D` | `sys_recv` | `int sockfd, void* buf, size_t len` | Receive packet from BSD socket |
| `0x27` | `sys_getpid` | `void` | Return active process ID (PID) |
| `0x3B` | `sys_execve` | `const char* filename, char** argv` | Execute ELF64 executable binary |
| `0x3C` | `sys_exit` | `int status` | Terminate process and free memory |
| `0x70` | `sys_tpm_attest`| `void* hash_out` | Hardware-rooted TPM 2.0 SPI attestation |
| `0x71` | `sys_cap_grant` | `uint64_t cap_mask` | Request Capability Access Control bitmask |

---

## Register Calling Convention

- `RAX`: System Call Vector Number
- `RDI`: Argument 1
- `RSI`: Argument 2
- `RDX`: Argument 3
- `R10`: Argument 4
- `R8` : Argument 5
- `R9` : Argument 6
- `RAX`: Return Value (signed 64-bit integer)
