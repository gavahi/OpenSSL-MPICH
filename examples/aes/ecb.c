#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>



int AES(char* key, char* S, char* subkey)
{

	
	int key_len;
	int len;

	EVP_CIPHER_CTX *ctx_enc;
	ctx_enc = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx_enc, EVP_aes_128_ecb(), NULL, key, NULL);
	EVP_EncryptInit_ex(ctx_enc, NULL, NULL, key, NULL);
	
  	if(1 != EVP_EncryptUpdate(ctx_enc, subkey, &len, S, 16))
    		printf("1");
  	key_len = len;

	if(1 != EVP_EncryptFinal_ex(ctx_enc, subkey + len, &len)) 
		printf("2");
        

	  /* Clean up */
  	EVP_CIPHER_CTX_free(ctx_enc);

 	 return key_len;



}

int main()
{
	unsigned char subkey[128];
	const unsigned char key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	unsigned char S[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int len = AES(key, S, subkey);
	printf("The %d length of subkey is:\n", len);
        printf("####################################\n");
	int i;
	for (i=0;i<len;i++){
		printf("%02x ", subkey[i]);
	}
        printf("\n####################################\n");

       unsigned char S1[] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	 len = AES(key, S1, subkey);
	printf("The %d length of subkey is:\n", len);
        printf("####################################\n");
	
	for (i=0;i<len;i++){
		printf("%02x ", subkey[i]);
	}
        printf("\n####################################\n");

        len = AES(key, S, subkey);
	printf("The %d length of subkey is:\n", len);
        printf("####################################\n");
	 i;
	for (i=0;i<len;i++){
		printf("%02x ", subkey[i]);
	}
        printf("\n####################################\n");
	
	return 0;
}
