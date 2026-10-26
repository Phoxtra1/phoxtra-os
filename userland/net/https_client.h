#ifndef PHOXTRA_USERLAND_HTTPS_CLIENT_H
#define PHOXTRA_USERLAND_HTTPS_CLIENT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PHOXTRA_HTTPS_PORT 443u
#define PHOXTRA_HTTPS_MAX_HOSTNAME 253u
#define PHOXTRA_HTTPS_MAX_PATH 1024u
#define PHOXTRA_HTTPS_MAX_REQUEST 1400u
typedef struct phoxtra_https_io {
 void* context;
 int (*connect_tcp)(void*,const char*,uint16_t);
 int (*send)(void*,int,const unsigned char*,size_t);
 int (*recv)(void*,int,unsigned char*,size_t);
 int (*close)(void*,int);
} phoxtra_https_io_t;
enum { PHOXTRA_HTTPS_OK=0, PHOXTRA_HTTPS_ERR_ARGUMENT=-1,
 PHOXTRA_HTTPS_ERR_CA_STORE=-2, PHOXTRA_HTTPS_ERR_CRYPTO_INIT=-3,
 PHOXTRA_HTTPS_ERR_CONNECT=-4, PHOXTRA_HTTPS_ERR_TLS=-5,
 PHOXTRA_HTTPS_ERR_CERTIFICATE=-6, PHOXTRA_HTTPS_ERR_HTTP=-7,
 PHOXTRA_HTTPS_ERR_RESPONSE_TOO_LARGE=-8, PHOXTRA_HTTPS_ERR_IO=-9 };
int phoxtra_https_get_with_io(const phoxtra_https_io_t*,const char*,const char*,
 const unsigned char*,size_t,unsigned char*,size_t,size_t*);
int phoxtra_https_get(const char*,const char*,const unsigned char*,size_t,unsigned char*,size_t,size_t*);
int phoxtra_https_get_default(const char*,const char*,unsigned char*,size_t,size_t*);
#ifdef __cplusplus
}
#endif
#endif
