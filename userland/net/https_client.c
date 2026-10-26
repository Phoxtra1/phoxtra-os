#include "https_client.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#define TLS_READ_CHUNK 512u
#define TLS_HTTP_VERSION " HTTP/1.1\r\n"
#define TLS_HEADERS "User-Agent: PhoxtraOS/0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n"
typedef struct https_bio { const phoxtra_https_io_t* io; int socket; } https_bio_t;
static int hostname_valid(const char* host){
 if(!host||!host[0])return 0;
 size_t length=0,label=0;
 for(const unsigned char* p=(const unsigned char*)host;*p;++p){
  if(++length>PHOXTRA_HTTPS_MAX_HOSTNAME)return 0;
  if(*p=='.'){if(!label||label>63u||p[-1]=='-')return 0;label=0;continue;}
  if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return 0;
  if(!label&&*p=='-')return 0;
  if(++label>63u)return 0;
 }
 return label>0u&&label<=63u&&host[length-1u]!='-';
}
static int path_valid(const char* path,size_t* n){
 if(!path||path[0]!='/'||!n)return 0;
 size_t len=0;
 for(const unsigned char* p=(const unsigned char*)path;*p;++p){
  if(*p=='\r'||*p=='\n'||*p<0x20u||*p==0x7Fu)return 0;
  if(++len>PHOXTRA_HTTPS_MAX_PATH)return 0;
 }
 *n=len;return 1;
}
static int request_build(const char* host,const char* path,unsigned char* out,size_t cap,size_t* out_len){
 size_t path_len=0;
 if(!hostname_valid(host)||!path_valid(path,&path_len)||!out||!out_len)return -1;
 const char* parts[]={"GET ",path,TLS_HTTP_VERSION,"Host: ",host,"\r\n",TLS_HEADERS};
 const size_t sizes[]={4u,path_len,sizeof(TLS_HTTP_VERSION)-1u,6u,strlen(host),2u,sizeof(TLS_HEADERS)-1u};
 size_t total=0;
 for(size_t i=0;i<sizeof(parts)/sizeof(parts[0]);++i){if(total>cap||sizes[i]>cap-total)return -1;total+=sizes[i];}
 size_t offset=0;
 for(size_t i=0;i<sizeof(parts)/sizeof(parts[0]);++i){memcpy(out+offset,parts[i],sizes[i]);offset+=sizes[i];}
 *out_len=total;return 0;
}
static int tls_send(void* context,const unsigned char* data,size_t length){
 https_bio_t* b=(https_bio_t*)context;
 if(!b||!b->io||!b->io->send||(!data&&length))return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
 int n=b->io->send(b->io->context,b->socket,data,length);
 if(n<=0||(size_t)n>length)return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
 return n;
}
static int tls_recv(void* context,unsigned char* data,size_t length){
 https_bio_t* b=(https_bio_t*)context;
 if(!b||!b->io||!b->io->recv||(!data&&length))return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
 int n=b->io->recv(b->io->context,b->socket,data,length);
 if(n>0&&(size_t)n<=length)return n;
 if(n==0)return MBEDTLS_ERR_SSL_CONN_EOF;
 return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
int phoxtra_https_get_with_io(const phoxtra_https_io_t* io,const char* hostname,const char* path,
 const unsigned char* ca_pem,size_t ca_pem_length,unsigned char* response_buffer,
 size_t response_capacity,size_t* response_length){
 mbedtls_ssl_context ssl;mbedtls_ssl_config config;mbedtls_x509_crt ca;
 unsigned char request[PHOXTRA_HTTPS_MAX_REQUEST];size_t request_len=0,received=0;
 int socket=-1,result=PHOXTRA_HTTPS_ERR_TLS,ssl_init=0,config_init=0,ca_init=0;
 if(response_length)*response_length=0;
 if(!io||!io->connect_tcp||!io->send||!io->recv||!io->close||
    !hostname_valid(hostname)||!path_valid(path,&request_len)||!ca_pem||ca_pem_length<2u||
    !response_buffer||!response_capacity||!response_length)return PHOXTRA_HTTPS_ERR_ARGUMENT;
 if(request_build(hostname,path,request,sizeof(request),&request_len)!=0)return PHOXTRA_HTTPS_ERR_ARGUMENT;
 if(psa_crypto_init()!=PSA_SUCCESS)return PHOXTRA_HTTPS_ERR_CRYPTO_INIT;
 mbedtls_ssl_init(&ssl);ssl_init=1;mbedtls_ssl_config_init(&config);config_init=1;
 mbedtls_x509_crt_init(&ca);ca_init=1;
 int rc=mbedtls_x509_crt_parse(&ca,ca_pem,ca_pem_length);
 if(rc<0){result=PHOXTRA_HTTPS_ERR_CA_STORE;goto cleanup;}
 rc=mbedtls_ssl_config_defaults(&config,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
 if(rc!=0)goto cleanup;
 mbedtls_ssl_conf_authmode(&config,MBEDTLS_SSL_VERIFY_REQUIRED);
 mbedtls_ssl_conf_ca_chain(&config,&ca,NULL);
 mbedtls_ssl_conf_min_tls_version(&config,MBEDTLS_SSL_VERSION_TLS1_2);
 mbedtls_ssl_conf_max_tls_version(&config,MBEDTLS_SSL_VERSION_TLS1_3);
 rc=mbedtls_ssl_setup(&ssl,&config);if(rc!=0)goto cleanup;
 rc=mbedtls_ssl_set_hostname(&ssl,hostname);if(rc!=0)goto cleanup;
 socket=io->connect_tcp(io->context,hostname,PHOXTRA_HTTPS_PORT);
 if(socket<0){result=PHOXTRA_HTTPS_ERR_CONNECT;goto cleanup;}
 https_bio_t bio={io,socket};mbedtls_ssl_set_bio(&ssl,&bio,tls_send,tls_recv,NULL);
 rc=mbedtls_ssl_handshake(&ssl);
 if(rc!=0){result=mbedtls_ssl_get_verify_result(&ssl)!=0?PHOXTRA_HTTPS_ERR_CERTIFICATE:PHOXTRA_HTTPS_ERR_TLS;goto cleanup;}
 if(mbedtls_ssl_get_verify_result(&ssl)!=0){result=PHOXTRA_HTTPS_ERR_CERTIFICATE;goto cleanup;}
 size_t sent=0;
 while(sent<request_len){rc=mbedtls_ssl_write(&ssl,request+sent,request_len-sent);if(rc<=0){result=PHOXTRA_HTTPS_ERR_IO;goto cleanup;}sent+=(size_t)rc;}
 for(;;){
  unsigned char chunk[TLS_READ_CHUNK];rc=mbedtls_ssl_read(&ssl,chunk,sizeof(chunk));
  /*
   * TLS 1.3 may deliver a post-handshake NewSessionTicket while the
   * application is reading. Mbed TLS signals this informational event with
   * a negative code; consume it and continue reading application data.
   */
  if(rc==MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)continue;
  if(rc==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){*response_length=received;result=PHOXTRA_HTTPS_OK;break;}
  if(rc<=0){result=PHOXTRA_HTTPS_ERR_IO;goto cleanup;}
  if((size_t)rc>response_capacity-received){result=PHOXTRA_HTTPS_ERR_RESPONSE_TOO_LARGE;goto cleanup;}
  memcpy(response_buffer+received,chunk,(size_t)rc);received+=(size_t)rc;
 }
cleanup:
 if(socket>=0&&result==PHOXTRA_HTTPS_OK)(void)mbedtls_ssl_close_notify(&ssl);
 if(socket>=0)(void)io->close(io->context,socket);
 if(ca_init)mbedtls_x509_crt_free(&ca);
 if(config_init)mbedtls_ssl_config_free(&config);
 if(ssl_init)mbedtls_ssl_free(&ssl);
 if(result!=PHOXTRA_HTTPS_OK)*response_length=0;
 return result;
}
