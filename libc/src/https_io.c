#include "../include/phoxtra/net.h"
#include "../../userland/net/https_client.h"
#include <stddef.h>
#include <stdint.h>
static int os_connect_tcp(void* context,const char* hostname,uint16_t port){
 (void)context;uint32_t address=0;
 if(port!=PHOXTRA_HTTPS_PORT||phoxtra_dns_resolve_ipv4(hostname,&address)!=0||!address)return -1;
 int fd=phoxtra_socket(PHOXTRA_AF_INET,PHOXTRA_SOCK_STREAM,PHOXTRA_IPPROTO_TCP);
 if(fd<0)return -1;
 phoxtra_sockaddr_in_t remote={0};remote.sin_family=PHOXTRA_AF_INET;
 remote.sin_port=phoxtra_htons(port);remote.sin_addr=address;
 if(phoxtra_connect(fd,&remote)!=0){(void)phoxtra_close(fd);return -1;}
 return fd;
}
static int os_send(void* context,int socket,const unsigned char* data,size_t length){
 (void)context;if(!data&&length)return -1;if(length>1460u)length=1460u;
 return phoxtra_send(socket,data,length,0);
}
static int os_recv(void* context,int socket,unsigned char* data,size_t length){
 (void)context;if(!data&&length)return -1;if(length>1460u)length=1460u;
 return phoxtra_recv(socket,data,length,0);
}
static int os_close(void* context,int socket){(void)context;return phoxtra_close(socket);}
int phoxtra_https_get(const char* hostname,const char* path,const unsigned char* ca_pem,size_t ca_pem_length,unsigned char* response_buffer,size_t response_capacity,size_t* response_length){
 const phoxtra_https_io_t io={0,os_connect_tcp,os_send,os_recv,os_close};
 return phoxtra_https_get_with_io(&io,hostname,path,ca_pem,ca_pem_length,response_buffer,response_capacity,response_length);
}
