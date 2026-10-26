#define _POSIX_C_SOURCE 200809L
#include "../userland/net/https_client.h"
#include <errno.h>
#include <limits.h>
#include <psa/crypto.h>
#include <psa/crypto_platform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef struct test_transport{uint16_t port;}test_transport_t;
static int64_t time_offset_seconds;
time_t time(time_t* out){struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now)!=0)return (time_t)-1;time_t v=now.tv_sec+(time_t)time_offset_seconds;if(out)*out=v;return v;}
psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t* context,uint8_t* output,size_t size,size_t* output_size){
 (void)context;if(!output_size||(!output&&size))return PSA_ERROR_INVALID_ARGUMENT;*output_size=0;size_t offset=0;
 while(offset<size){ssize_t n=getrandom(output+offset,size-offset,0);if(n<0&&errno==EINTR)continue;if(n<=0)return PSA_ERROR_INSUFFICIENT_ENTROPY;offset+=(size_t)n;}
 *output_size=size;return PSA_SUCCESS;
}
static int host_connect(void* context,const char* host,uint16_t port){
 (void)host;(void)port;test_transport_t* t=(test_transport_t*)context;if(!t||!t->port)return -1;
 int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(t->port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(connect(fd,(struct sockaddr*)&a,sizeof(a))!=0){close(fd);return -1;}return fd;
}
static int host_send(void* context,int fd,const unsigned char* data,size_t length){(void)context;ssize_t n;do{n=send(fd,data,length,MSG_NOSIGNAL);}while(n<0&&errno==EINTR);return n>INT_MAX?-1:(int)n;}
static int host_recv(void* context,int fd,unsigned char* data,size_t length){(void)context;ssize_t n;do{n=recv(fd,data,length,0);}while(n<0&&errno==EINTR);return n>INT_MAX?-1:(int)n;}
static int host_close(void* context,int fd){(void)context;return close(fd);}
static unsigned char* read_file(const char* path,size_t* size_out){
 if(!path||!size_out)return NULL;FILE* f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)!=0){fclose(f);return NULL;}
 long n=ftell(f);if(n<0||n>4*1024*1024L){fclose(f);return NULL;}if(fseek(f,0,SEEK_SET)!=0){fclose(f);return NULL;}
 unsigned char* p=(unsigned char*)malloc((size_t)n+1u);if(!p){fclose(f);return NULL;}size_t got=fread(p,1u,(size_t)n,f);fclose(f);
 if(got!=(size_t)n){free(p);return NULL;}p[got]=0;*size_out=got+1u;return p;
}
static int contains(const unsigned char* data,size_t size,const char* needle){size_t n=strlen(needle);if(!data||n>size)return 0;for(size_t i=0;i<=size-n;++i)if(memcmp(data+i,needle,n)==0)return 1;return 0;}
int main(int argc,char** argv){
 if(argc!=6){fprintf(stderr,"usage: %s port hostname ca.pem time-offset success|reject\n",argv[0]);return 2;}
 char* end=NULL;long port=strtol(argv[1],&end,10);if(!end||*end||port<1||port>65535)return 2;
 time_offset_seconds=(int64_t)strtoll(argv[4],NULL,10);int success=strcmp(argv[5],"success")==0;
 size_t ca_size=0;unsigned char* ca=strcmp(argv[3],"-")==0?NULL:read_file(argv[3],&ca_size);
 if(strcmp(argv[3],"-")!=0&&!ca){fprintf(stderr,"cannot read trust PEM\n");return 2;}
 test_transport_t transport={(uint16_t)port};const phoxtra_https_io_t io={&transport,host_connect,host_send,host_recv,host_close};
 unsigned char response[8192];size_t response_size=0;
 int result=phoxtra_https_get_with_io(&io,argv[2],"/",ca,ca_size,response,sizeof(response),&response_size);
 int passed=success?(result==PHOXTRA_HTTPS_OK&&contains(response,response_size,"HTTP/1.1 200 OK")&&contains(response,response_size,"PHOXTRA_TLS_OK")):(result!=PHOXTRA_HTTPS_OK&&response_size==0);
 if(!passed){fprintf(stderr,"FAIL: expectation=%s result=%d response=%zu\n",success?"success":"reject",result,response_size);free(ca);return 1;}
 printf("PASS: HTTPS peer %s; result=%d\n",success?"verified":"rejected",result);free(ca);return 0;
}
