#include "../include/phoxtra/net.h"
#include <psa/crypto.h>
#include <psa/crypto_platform.h>
#include <stddef.h>
#include <stdint.h>
psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t* context,uint8_t* output,size_t output_size,size_t* output_length){
 (void)context;
 if(!output_length||(!output&&output_size))return PSA_ERROR_INVALID_ARGUMENT;
 *output_length=0;size_t offset=0;
 while(offset<output_size){
  size_t amount=output_size-offset;if(amount>256u)amount=256u;
  if(phoxtra_random_bytes(output+offset,amount)!=0){for(size_t i=0;i<output_size;++i)output[i]=0;return PSA_ERROR_INSUFFICIENT_ENTROPY;}
  offset+=amount;
 }
 *output_length=output_size;return PSA_SUCCESS;
}
