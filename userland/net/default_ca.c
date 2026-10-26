#include "https_client.h"
#include "phoxtra_default_ca.h"
const unsigned char* phoxtra_default_ca_bundle(size_t* length_out){
 if(length_out)*length_out=PHOXTRA_DEFAULT_CA_PEM_LENGTH;
 return phoxtra_default_ca_pem;
}
int phoxtra_https_get_default(const char* hostname,const char* path,unsigned char* response_buffer,size_t response_capacity,size_t* response_length){
 size_t length=0;const unsigned char* bundle=phoxtra_default_ca_bundle(&length);
 return phoxtra_https_get(hostname,path,bundle,length,response_buffer,response_capacity,response_length);
}
