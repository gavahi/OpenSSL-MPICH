/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/* Copyright (c) 2001-2018, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH2 software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#include "mpiimpl.h"

unsigned int outlen_enc;
// int outlen_enc_org;
EVP_CIPHER_CTX *ctx_enc;
int nonceCounter=0;
unsigned char send_ciphertext[4194304+400];


/* -- Begin Profiling Symbol Block for routine MPI_Send */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Send = PMPI_Send
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_Send  MPI_Send
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Send as PMPI_Send
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) __attribute__((weak,alias("PMPI_Send")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPI_Send
#define MPI_Send PMPI_Send

#endif

#undef FUNCNAME
#define FUNCNAME MPI_Send

/*@
    MPI_Send - Performs a blocking send

Input Parameters:
+ buf - initial address of send buffer (choice) 
. count - number of elements in send buffer (nonnegative integer) 
. datatype - datatype of each send buffer element (handle) 
. dest - rank of destination (integer) 
. tag - message tag (integer) 
- comm - communicator (handle) 

Notes:
This routine may block until the message is received by the destination 
process.

.N ThreadSafe

.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_COUNT
.N MPI_ERR_TYPE
.N MPI_ERR_TAG
.N MPI_ERR_RANK

.seealso: MPI_Isend, MPI_Bsend
@*/
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
	     MPI_Comm comm)
{
    static const char FCNAME[] = "MPI_Send";
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm *comm_ptr = NULL;
    MPID_Request * request_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_SEND);

    MPIR_ERRTEST_INITIALIZED_ORDIE();
    
    MPID_THREAD_CS_ENTER(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    MPID_MPI_PT2PT_FUNC_ENTER_FRONT(MPID_STATE_MPI_SEND);
    
    /* Validate handle parameters needing to be converted */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_COMM(comm, mpi_errno);
	}
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */
    
    /* Convert MPI object handles to object pointers */
    MPID_Comm_get_ptr( comm, comm_ptr );

    /* Validate parameters if error checking is enabled */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            MPID_Comm_valid_ptr( comm_ptr, mpi_errno, FALSE );
            if (mpi_errno) goto fn_fail;
	    
	    MPIR_ERRTEST_COUNT(count, mpi_errno);
	    MPIR_ERRTEST_SEND_RANK(comm_ptr, dest, mpi_errno);
	    MPIR_ERRTEST_SEND_TAG(tag, mpi_errno);
	    
	    /* Validate datatype handle */
	    MPIR_ERRTEST_DATATYPE(datatype, "datatype", mpi_errno);
	    
	    /* Validate datatype object */
	    if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
	    {
		MPID_Datatype *datatype_ptr = NULL;

		MPID_Datatype_get_ptr(datatype, datatype_ptr);
		MPID_Datatype_valid_ptr(datatype_ptr, mpi_errno);
		if (mpi_errno) goto fn_fail;
		MPID_Datatype_committed_ptr(datatype_ptr, mpi_errno);
		if (mpi_errno) goto fn_fail;
	    }
	    
	    /* Validate buffer */
	    MPIR_ERRTEST_USERBUFFER(buf,count,datatype,mpi_errno);
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ...  */
    
    mpi_errno = MPID_Send(buf, count, datatype, dest, tag, comm_ptr, 
			  MPID_CONTEXT_INTRA_PT2PT, &request_ptr);
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    if (request_ptr == NULL)
    {
#if defined(CHANNEL_MRAIL)
        mpi_errno = MPID_Progress_test();
        if (mpi_errno != MPI_SUCCESS)
        {
            goto fn_fail;
        }
#endif /* defined(CHANNEL_MRAIL) */
	goto fn_exit;
    }

    /* If a request was returned, then we need to block until the request 
       is complete */
    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;
	    
	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno != MPI_SUCCESS)
	    {
		/* --BEGIN ERROR HANDLING-- */
		MPID_Progress_end(&progress_state);
		goto fn_fail;
		/* --END ERROR HANDLING-- */
	    }
	}
	MPID_Progress_end(&progress_state);
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    MPID_Request_release(request_ptr);
    
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    /* ... end of body of routine ... */
    
  fn_exit:
    MPID_MPI_PT2PT_FUNC_EXIT(MPID_STATE_MPI_SEND);
    MPID_THREAD_CS_EXIT(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#   ifdef HAVE_ERROR_CHECKING
    {
	mpi_errno = MPIR_Err_create_code(
	    mpi_errno, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__, MPI_ERR_OTHER, "**mpi_send", 
	    "**mpi_send %p %d %D %i %t %C", buf, count, datatype, dest, tag, comm);
    }
#   endif
    mpi_errno = MPIR_Err_return_comm( comm_ptr, FCNAME, mpi_errno );
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

#if 0
void init_openssl_128(){
   // unsigned char key_boringssl_16 [16] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p'};
    //ctx = EVP_AEAD_CTX_new(EVP_aead_aes_128_gcm(),key,16, 0);
	ctx_enc = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(ctx_enc, EVP_aes_128_gcm(), NULL, NULL, NULL);
	ctx_dec = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx_dec, EVP_aes_128_gcm(), NULL, NULL, NULL);
	enc_init();
	dec_init();
    return;                        
}

void init_openssl_256(){
   // unsigned char key_boringssl_16 [16] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p'};
    //ctx = EVP_AEAD_CTX_new(EVP_aead_aes_128_gcm(),key,16, 0);
	ctx_enc = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(ctx_enc, EVP_aes_256_gcm(), NULL, NULL, NULL);
	ctx_dec = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx_dec, EVP_aes_256_gcm(), NULL, NULL, NULL);
	enc_init();
	dec_init();
    return;                        
}


void enc_init(){
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0}; 

	//const unsigned char gcm_aad[] = {'/0'};
	//const unsigned char gcm_aad[] = {0x4d, 0x23, 0xc3, 0xce, 0xc3, 0x34, 0xb4, 0x9b, 0xdb, 0x37, 0x0c, 0x43,0x7f, 0xec, 0x78, 0xde};
	
	//ctx_enc = EVP_CIPHER_CTX_new();
	//EVP_EncryptInit_ex(ctx_enc, EVP_aes_128_gcm(), NULL, NULL, NULL);
	EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_SET_IVLEN, sizeof(gcm_iv), NULL);
	EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, gcm_iv);
	//EVP_EncryptUpdate(ctx_enc, NULL, &outlen_enc, gcm_aad, sizeof(gcm_aad));	
}


void openssl_enc_core(unsigned char * ciphertext_send, unsigned long long src,const void *sendbuf, unsigned long long dest, unsigned long long blocktype_send){
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0};
	int world_rank;
	static int first_enc=1;
	
	//if (first_enc) {
	//	enc_init();
	//	first_enc=0;
		//printf("\nEncription initialization is done!\n");
	//}
	//enc_init();
	
	EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, gcm_iv);
	EVP_EncryptUpdate(ctx_enc, ciphertext_send+src, &outlen_enc, sendbuf+dest, blocktype_send);
	EVP_EncryptFinal_ex(ctx_enc, ciphertext_send+src+outlen_enc, &outlen_enc);
	EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, ciphertext_send+src+blocktype_send);
}

int MPI_SEC_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm , int max_pack)
{
    int mpi_errno = MPI_SUCCESS;
	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	int i=0;
	
	//const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0}; 
		
	MPI_Request request[10000];
	MPI_Status status_local[10000];

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
			
	char * ciphertext;
	int sendtype_sz;
	unsigned long long next, src;
	MPI_Type_size(datatype, &sendtype_sz);
    	
	unsigned long long blocktype_send= (unsigned long long) sendtype_sz*count;
	
	// if (count > max_pack) {
		
		// int temp_count=count/max_pack;
		
		// ciphertext=(char*) MPIU_Malloc(((temp_count*(max_pack+32)) * sizeof(datatype)) );
				
		// for (i=0; i<temp_count; i++){
			
			// EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, gcm_iv);			
			// EVP_EncryptUpdate(ctx_enc, ciphertext+i*(max_pack+32), &outlen_enc, buf+i*max_pack, max_pack);
			// EVP_EncryptFinal_ex(ctx_enc, ciphertext+i*(max_pack+32), &outlen_enc);
			// EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, (ciphertext+i*(max_pack+32)+max_pack));
			
			// mpi_errno=MPI_Isend(ciphertext+i*(max_pack+32),max_pack+16, datatype, dest, tag, comm, &request[i]);
			
		// }
		
		// MPI_Waitall(temp_count,request, status_local);
	// }
	// else {
		
		// ciphertext=(char*) MPIU_Malloc(((count+32) * sizeof(datatype)) );
		
		// EVP_EncryptUpdate(ctx_enc, ciphertext, &outlen_enc, buf, count);
		// EVP_EncryptFinal_ex(ctx_enc, ciphertext, &outlen_enc);
		// EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, (ciphertext+count));
		
		// mpi_errno=MPI_Send(ciphertext,count+16, datatype, dest, tag, comm);
	// }
	
	
	ciphertext=(char*) MPIU_Malloc((32) + blocktype_send );
	openssl_enc_core(ciphertext,0,buf,0,blocktype_send);
	
	// printf("\nciphertext @ sender in Process rank %d:\n",world_rank);
    // BIO_dump_fp(stdout, ciphertext, count+16);
	
	mpi_errno=MPI_Send(ciphertext,blocktype_send+16, MPI_CHAR, dest, tag, comm);

	
	
	//printf("SEND3 count=%d \n",count);
	
	MPIU_Free(ciphertext);

	return mpi_errno;
}
#endif


void init_openssl_128(){
   // unsigned char key_boringssl_16 [16] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p'};
    //ctx = EVP_AEAD_CTX_new(EVP_aead_aes_128_gcm(),key,16, 0);
	ctx_enc = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(ctx_enc, EVP_aes_128_gcm(), NULL, NULL, NULL);
	ctx_dec = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx_dec, EVP_aes_128_gcm(), NULL, NULL, NULL);
	//enc_init();
	//dec_init();
    return;                        
}

void init_openssl_256(){unsigned char send_ciphertext[4194304+18];
   // unsigned char key_boringssl_16 [16] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p'};
    //ctx = EVP_AEAD_CTX_new(EVP_aead_aes_128_gcm(),key,16, 0);
	ctx_enc = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(ctx_enc, EVP_aes_256_gcm(), NULL, NULL, NULL);
	ctx_dec = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx_dec, EVP_aes_256_gcm(), NULL, NULL, NULL);
	//enc_init();
	//dec_init();
    return;                        
}


//void openssl_enc_core(unsigned char * ciphertext_send, unsigned long long src,const void *sendbuf, unsigned long long dest, unsigned long long blocktype_send){
void openssl_enc_core(unsigned char *send_ciphertext , unsigned long long src,const void *sendbuf, unsigned long long dest, unsigned long long blocktype_send){	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0};

	nonceCounter++;
	memset(send_ciphertext, 0, 8);
	send_ciphertext[8] = (nonceCounter >> 24) & 0xFF;
	send_ciphertext[9] = (nonceCounter >> 16) & 0xFF;
	send_ciphertext[10] = (nonceCounter >> 8) & 0xFF;
	send_ciphertext[11] = nonceCounter & 0xFF;
	//strncpy(gcm_iv, send_ciphertext, 12);

	int world_rank;
	static int first_enc=1;
	
	
	EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, send_ciphertext);
	//EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
	
	//EVP_EncryptUpdate(ctx_enc, ciphertext_send+src, &outlen_enc, sendbuf+dest, blocktype_send);
	EVP_EncryptUpdate(ctx_enc, send_ciphertext+12+src, &outlen_enc, sendbuf+dest, blocktype_send);

	//EVP_EncryptFinal_ex(ctx_enc, ciphertext_send+src+outlen_enc, &outlen_enc);
	EVP_EncryptFinal_ex(ctx_enc, send_ciphertext+12+src+outlen_enc, &outlen_enc);
	//EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, ciphertext_send+src+blocktype_send);
	/* (ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) */
	EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, send_ciphertext+12+src+blocktype_send);



	//return outlen_enc;
}

int MPI_SEC_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm , int max_pack)
{
    int mpi_errno = MPI_SUCCESS;
	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	int i=0;
	
	//const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0}; 
		
	//MPI_Request request[10000];
	//MPI_Status status_local[10000];

	//OpenSSL_add_all_algorithms();
	//ERR_load_crypto_strings();
			
	char * ciphertext;
	int sendtype_sz;
	unsigned long long next, src;
	MPI_Type_size(datatype, &sendtype_sz);
    	
	unsigned long long blocktype_send= (unsigned long long) sendtype_sz*count;
	
	// if (count > max_pack) {
		
		// int temp_count=count/max_pack;
		
		// ciphertext=(char*) MPIU_Malloc(((temp_count*(max_pack+32)) * sizeof(datatype)) );
				
		// for (i=0; i<temp_count; i++){
			
			// EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, gcm_iv);			
			// EVP_EncryptUpdate(ctx_enc, ciphertext+i*(max_pack+32), &outlen_enc, buf+i*max_pack, max_pack);
			// EVP_EncryptFinal_ex(ctx_enc, ciphertext+i*(max_pack+32), &outlen_enc);
			// EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, (ciphertext+i*(max_pack+32)+max_pack));
			
			// mpi_errno=MPI_Isend(ciphertext+i*(max_pack+32),max_pack+16, datatype, dest, tag, comm, &request[i]);
			
		// }
		
		// MPI_Waitall(temp_count,request, status_local);
	// }
	// else {
		
		// ciphertext=(char*) MPIU_Malloc(((count+32) * sizeof(datatype)) );
		
		// EVP_EncryptUpdate(ctx_enc, ciphertext, &outlen_enc, buf, count);
		// EVP_EncryptFinal_ex(ctx_enc, ciphertext, &outlen_enc);
		// EVP_CIPHER_CTX_ctrl(ctx_enc, EVP_CTRL_AEAD_GET_TAG, 16, (ciphertext+count));
		
		// mpi_errno=MPI_Send(ciphertext,count+16, datatype, dest, tag, comm);
	// }
	
	
	//ciphertext=(char*) MPIU_Malloc((32) + blocktype_send );
	openssl_enc_core(send_ciphertext,0,buf,0,blocktype_send);
	
	// printf("\nciphertext @ sender in Process rank %d:\n",world_rank);
    // BIO_dump_fp(stdout, ciphertext, count+16);
	
	//with IV size 12, tag size 16
	mpi_errno=MPI_Send(send_ciphertext,blocktype_send+16+12, MPI_CHAR, dest, tag, comm);
	

	
	
	//printf("SEND3 count=%d \n",count);
	
	//MPIU_Free(ciphertext);

	return mpi_errno;
}
