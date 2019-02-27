/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
unsigned char ciphertext_recv[4194304+400];
int outlen_dec;
int outlen_dec_org;
EVP_CIPHER_CTX *ctx_dec;
unsigned char Recv_IV[1024][16],dec_iv[1024][16], dec_calculator[1024][66560];

int dec_start[1024], dec_end[1024],dec_flag[1024],dec_amount;
int dec_counter[1024];


/* -- Begin Profiling Symbol Block for routine MPI_Recv */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Recv = PMPI_Recv
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_Recv  MPI_Recv
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Recv as PMPI_Recv
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status) __attribute__((weak,alias("PMPI_Recv")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPI_Recv
#define MPI_Recv PMPI_Recv

#endif

#undef FUNCNAME
#define FUNCNAME MPI_Recv

/*@
    MPI_Recv - Blocking receive for a message

Output Parameters:
+ buf - initial address of receive buffer (choice) 
- status - status object (Status) 

Input Parameters:
+ count - maximum number of elements in receive buffer (integer) 
. datatype - datatype of each receive buffer element (handle) 
. source - rank of source (integer) 
. tag - message tag (integer) 
- comm - communicator (handle) 

Notes:
The 'count' argument indicates the maximum length of a message; the actual 
length of the message can be determined with 'MPI_Get_count'.  

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_COMM
.N MPI_ERR_TYPE
.N MPI_ERR_COUNT
.N MPI_ERR_TAG
.N MPI_ERR_RANK

@*/
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
	     MPI_Comm comm, MPI_Status *status)
{
    static const char FCNAME[] = "MPI_Recv";
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm *comm_ptr = NULL;
    MPID_Request * request_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_RECV);

    MPIR_ERRTEST_INITIALIZED_ORDIE();
    
    MPID_THREAD_CS_ENTER(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    MPID_MPI_PT2PT_FUNC_ENTER_BACK(MPID_STATE_MPI_RECV);
    
    /* Validate handle parameters needing to be converted */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_COMM(comm, mpi_errno);
	    /* NOTE: MPI_STATUS_IGNORE != NULL */
	    MPIR_ERRTEST_ARGNULL(status, "status", mpi_errno);
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
	    MPIR_ERRTEST_RECV_RANK(comm_ptr, source, mpi_errno);
	    MPIR_ERRTEST_RECV_TAG(tag, mpi_errno);
	    
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

    /* MT: Note that MPID_Recv may release the SINGLE_CS if it
       decides to block internally.  MPID_Recv in that case will
       re-aquire the SINGLE_CS before returnning */
    mpi_errno = MPID_Recv(buf, count, datatype, source, tag, comm_ptr, 
			  MPID_CONTEXT_INTRA_PT2PT, status, &request_ptr);
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    if (request_ptr == NULL)
    {
	goto fn_exit;
    }
    
    /* If a request was returned, then we need to block until the request is 
       complete */
    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;
	    
	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
	    /* MT: Progress_wait may release the SINGLE_CS while it
	       waits */
	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno != MPI_SUCCESS)
	    { 
		/* --BEGIN ERROR HANDLING-- */
		MPID_Progress_end(&progress_state);
		goto fn_fail;
		/* --END ERROR HANDLING-- */
	    }

            if (unlikely(MPIR_CVAR_ENABLE_FT &&
                        !MPID_Request_is_complete(request_ptr) &&
                        MPID_Request_is_anysource(request_ptr) &&
                        !MPID_Comm_AS_enabled(request_ptr->comm))) {
                /* --BEGIN ERROR HANDLING-- */
                MPID_Cancel_recv(request_ptr);
                MPIR_STATUS_SET_CANCEL_BIT(request_ptr->status, FALSE);
                MPIR_ERR_SET(request_ptr->status.MPI_ERROR, MPIX_ERR_PROC_FAILED, "**proc_failed");
                mpi_errno = request_ptr->status.MPI_ERROR;
                goto fn_fail;
                /* --END ERROR HANDLING-- */
            }
	}
	MPID_Progress_end(&progress_state);
    }

    mpi_errno = request_ptr->status.MPI_ERROR;
    MPIR_Request_extract_status(request_ptr, status);
    MPID_Request_release(request_ptr);

    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    /* ... end of body of routine ... */
    
  fn_exit:
    MPID_MPI_PT2PT_FUNC_EXIT_BACK(MPID_STATE_MPI_RECV);
    MPID_THREAD_CS_EXIT(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#   ifdef HAVE_ERROR_CHECKING
    {
	mpi_errno = MPIR_Err_create_code(
	    mpi_errno, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__, MPI_ERR_OTHER, "**mpi_recv",
	    "**mpi_recv %p %d %D %i %t %C %p", buf, count, datatype, source, tag, comm, status);
    }
#   endif
    mpi_errno = MPIR_Err_return_comm( comm_ptr, FCNAME, mpi_errno );
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}
 #if 0
void openssl_dec_core(unsigned char * ciphertext_recv, unsigned long long src, const void *recvbuf, unsigned long long dest, unsigned long long blocktype_recv){
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0};
	
	static int first_dec=1;
	
	/* if (first_dec) {
		dec_init();
		first_dec=0;
		//printf("\nDecription initialization is done!\n");
	} */
	
	//dec_init();
	
	EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, gcm_iv);		
	EVP_DecryptUpdate(ctx_dec, recvbuf+dest, &outlen_dec, ciphertext_recv+src, (blocktype_recv));		
	EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,(ciphertext_recv+src+blocktype_recv));		
	if (!(EVP_DecryptFinal_ex(ctx_dec, (recvbuf+dest+outlen_dec), &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
}

int MPI_SEC_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	const unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0}; 	
	int i;
	int recvtype_sz;
	MPI_Type_size(datatype, &recvtype_sz);
	unsigned long long blocktype_recv= (unsigned long long) recvtype_sz*count;
	
	unsigned char * ciphertext_recv;
	unsigned long long next, src;
	
	int mpi_errno = MPI_SUCCESS;

	char * ciphertext;
	
	/* if (count > max_pack) {
		
		int temp_count=count/max_pack;		
		
		ciphertext=(char*) MPIU_Malloc(((temp_count*(max_pack+32)) * sizeof(datatype)) );
		
		for (i=0; i<temp_count; i++){
			
			EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, gcm_iv);
														
			mpi_errno=MPI_Recv(ciphertext+i*(max_pack+32), max_pack+16, datatype, source, tag, comm,status);
			
			EVP_DecryptUpdate(ctx_dec, buf+i*max_pack, &outlen_dec, ciphertext+i*(max_pack+32), max_pack);
			EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,ciphertext+i*(max_pack+32)+max_pack);
			if (!(EVP_DecryptFinal_ex(ctx_dec, buf+i*max_pack, &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
		}
		
	}
	else{
		//printf("Ciphertext @ Decrypt:\n");
		//BIO_dump_fp(stdout, ciphertext, count+16);
		
		ciphertext=(char*) MPIU_Malloc(((count+32) * sizeof(datatype)) );
		
		mpi_errno=MPI_Recv(ciphertext, count+16, datatype, source, tag, comm,status);
		
		EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, gcm_iv);
		
		EVP_DecryptUpdate(ctx_dec, buf, &outlen_dec, ciphertext, count);
		
		EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,ciphertext+count);
		
		if (!(EVP_DecryptFinal_ex(ctx_dec, buf, &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
	} */
	 
	
	ciphertext=(char*) MPIU_Malloc(32 + blocktype_recv );
	
	mpi_errno=MPI_Recv(ciphertext, blocktype_recv+16, MPI_CHAR, source, tag, comm,status);
	
	// printf("Ciphertext @ Receiver:\n");
	// BIO_dump_fp(stdout, ciphertext, count+16);
	
	openssl_dec_core(ciphertext,0,buf,0,blocktype_recv);
	
	MPIU_Free(ciphertext);
	
	return mpi_errno;
}
#endif


void openssl_dec_core(unsigned char * ciphertext_recv, unsigned long long src, const void *recvbuf, unsigned long long dest, unsigned long long blocktype_recv){
	
	
	unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//strncpy(gcm_iv, ciphertext_recv, 12);//????
	
	
	EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, ciphertext_recv+src);		
	//EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
	EVP_DecryptUpdate(ctx_dec, recvbuf+dest, &outlen_dec, ciphertext_recv+12+src, (blocktype_recv-12));		
	//EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,(ciphertext_recv+src+blocktype_recv));
	/* EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)*/		
	EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,(ciphertext_recv+src+blocktype_recv));		
	//if (!(EVP_DecryptFinal_ex(ctx_dec, (recvbuf+dest+outlen_dec), &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
	if (!(EVP_DecryptFinal_ex(ctx_dec, (recvbuf+dest+outlen_dec), &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
}

int MPI_SEC_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	
	unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//unsigned char gcm_iv[] = {0,0,0,0,0,0,0,0,0,0,0,0}; 	
	int i;
	int recvtype_sz;
	MPI_Type_size(datatype, &recvtype_sz);
	unsigned long long blocktype_recv= (unsigned long long) recvtype_sz*count;
	
	//unsigned char * ciphertext_recv;
	unsigned long long next, src;
	
	int mpi_errno = MPI_SUCCESS;

	//char * ciphertext;
	
	/* if (count > max_pack) {
		
		int temp_count=count/max_pack;		
		
		ciphertext=(char*) MPIU_Malloc(((temp_count*(max_pack+32)) * sizeof(datatype)) );
		
		for (i=0; i<temp_count; i++){
			
			EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, gcm_iv);
														
			mpi_errno=MPI_Recv(ciphertext+i*(max_pack+32), max_pack+16, datatype, source, tag, comm,status);
			
			EVP_DecryptUpdate(ctx_dec, buf+i*max_pack, &outlen_dec, ciphertext+i*(max_pack+32), max_pack);
			EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,ciphertext+i*(max_pack+32)+max_pack);
			if (!(EVP_DecryptFinal_ex(ctx_dec, buf+i*max_pack, &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
		}
		
	}
	else{
		//printf("Ciphertext @ Decrypt:\n");
		//BIO_dump_fp(stdout, ciphertext, count+16);
		
		ciphertext=(char*) MPIU_Malloc(((count+32) * sizeof(datatype)) );
		
		mpi_errno=MPI_Recv(ciphertext, count+16, datatype, source, tag, comm,status);
		
		EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, gcm_iv);
		
		EVP_DecryptUpdate(ctx_dec, buf, &outlen_dec, ciphertext, count);
		
		EVP_CIPHER_CTX_ctrl(ctx_dec, EVP_CTRL_AEAD_SET_TAG, 16,ciphertext+count);
		
		if (!(EVP_DecryptFinal_ex(ctx_dec, buf, &outlen_dec) > 0)) printf("Tag Verify Failed!\n");
	} */
	 
	
	//ciphertext=(char*) MPIU_Malloc(40 + blocktype_recv );
	
	mpi_errno=MPI_Recv(ciphertext_recv, blocktype_recv+16+12, MPI_CHAR, source, tag, comm,status);
	
	// printf("Ciphertext @ Receiver:\n");
	// BIO_dump_fp(stdout, ciphertext, count+16);
	
	openssl_dec_core(ciphertext_recv,0,buf,0,blocktype_recv+12);
	
	//MPIU_Free(ciphertext_recv);
	
	return mpi_errno;
}


int MPI_CTR_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	int i;
	int recvtype_sz;
	MPI_Type_size(datatype, &recvtype_sz);
	unsigned long long blocktype_recv= (unsigned long long) recvtype_sz*count;
	
	//unsigned char * ciphertext_recv;
	unsigned long long next, src;
	
	int mpi_errno = MPI_SUCCESS;
	
	mpi_errno=MPI_Recv(ciphertext_recv, blocktype_recv+16, MPI_CHAR, source, tag, comm,status);
	
	// printf("Ciphertext @ Receiver:\n");
	// BIO_dump_fp(stdout, ciphertext, count+16);
	
	EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, ciphertext_recv);	
	EVP_DecryptUpdate(ctx_dec, buf, &outlen_dec, ciphertext_recv+16, blocktype_recv);
	
	//MPIU_Free(ciphertext_recv);
	
	return mpi_errno;
}


# if 0
int MPI_PreCtr_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int i;
	int recvtype_sz;
	MPI_Type_size(datatype, &recvtype_sz);
	unsigned long long blocktype_recv= (unsigned long long) recvtype_sz*count;
	
	//unsigned char * ciphertext_recv;
	unsigned long long next, src;
	
	int mpi_errno = MPI_SUCCESS;
	
	mpi_errno=MPI_Recv(ciphertext_recv, blocktype_recv+12, MPI_CHAR, source, tag, comm,status);
	
	// printf("Ciphertext @ Receiver:\n");
	// BIO_dump_fp(stdout, ciphertext, count+16);
	
	
	EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, ciphertext_recv);	
	EVP_DecryptUpdate(ctx_dec, buf, &outlen_dec, ciphertext_recv+12, blocktype_recv);
	
	//MPIU_Free(ciphertext_recv);
	
	return mpi_errno;
}



int MPI_PreCtr_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int i=0,len,segments_no,datasize,next,src;
	int recvtype_sz;
	int mpi_errno = MPI_SUCCESS;
	MPI_Type_size(datatype, &recvtype_sz);
	int blocktype_recv=  recvtype_sz*count;

    segments_no = ( (blocktype_recv-1)/sct_sz ) +1;
	mpi_errno = MPI_Recv(ciphertext_recv, blocktype_recv+(16*segments_no), MPI_CHAR, source, tag, comm, status);

	for(i=0; i < segments_no; i++){
        if( i != segments_no-1 ){
            datasize = sct_sz+16;
        }else{
            datasize = blocktype_recv-((segments_no-1)*sct_sz) + 16;    
        }	

		//sct_sz+16=32 
		next = i*sct_sz;
		src = i*(sct_sz+16);
		EVP_DecryptInit_ex(ctx_dec, NULL, NULL, gcm_key, ciphertext_recv+src);
		//EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
		EVP_DecryptUpdate(ctx_dec, buf+next, &outlen_dec, ciphertext_recv+16+src, datasize);
	}

	return mpi_errno;
}
# endif


int MPI_PreCtr_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status , int max_pack)
{    
	const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int i,len;
	int recvtype_sz,segments,dec_fin,dec_begin;
	unsigned char ch;
	MPI_Type_size(datatype, &recvtype_sz);
	unsigned long long blocktype_recv= (unsigned long long) recvtype_sz*count;
	
	if (blocktype_recv > 8192){
			printf("\nError: message size must < 8K!!!\n");
			return 1;
	}
	int mpi_errno = MPI_SUCCESS;
	
   if (dec_flag[source]==1){
	   mpi_errno=MPI_Recv(ciphertext_recv, blocktype_recv+16, MPI_CHAR, source, tag, comm,status);
	   memcpy(&Recv_IV[source][0],ciphertext_recv,16);

	   if (blocktype_recv <=1024){
		   EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &Recv_IV[source][0]);
	       EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, 1024);
		   dec_counter[source] = 64;
		   dec_start[source] = 0;
		   dec_end [source]= 1024;
		}else{
		   segments =((blocktype_recv-1)/16)*16+16;//upper integer multi of 16
		   EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &Recv_IV[source][0]);
		   EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, segments); 
		   dec_counter[source] = (segments/16); //upper integer
		   dec_end[source]= segments;
		   dec_start[source] = 0;
		}
	  
	  //Decryption
	   for(i=0; i< blocktype_recv; i++){
		   *((char *)(buf+i)) = (char )(dec_calculator[source][i] ^ (ciphertext_recv[i+16]));
		   //memcpy(buf+i, &ch, 1);
	   }
	   dec_flag[source] =0;
	   dec_start[source] = blocktype_recv;

	}else{
		dec_end[source] = (16*dec_counter[source])%66560;
		dec_amount= dec_end[source] - dec_start[source];

		if (blocktype_recv >= 66560-dec_start[source]){
			//Restart from beginning && Make sure dec_end Always= 16*N
			memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
			dec_counter[source] = ((dec_counter[source]-1)/4160)*4160+4160;
		    IV_Count(&dec_iv[source][0],dec_counter[source]);
			segments=((blocktype_recv-1)/16)*16+16;	//upper integer
			EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[source][0]);
			EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, segments);
			
			dec_counter[source] += (segments/16);
			dec_start[source] = 0;
			dec_end[source]= segments;
		}else if(blocktype_recv > dec_amount){
			//Add more dec-ctr blocks
			memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
		    IV_Count(&dec_iv[source][0],dec_counter[source]);
			segments =((blocktype_recv-dec_amount-1)/16)*16+16;
			dec_fin = dec_end[source];
			EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[0][0]);
			EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][dec_fin], &len, p, segments); 
			dec_counter[source] += (segments/16); //upper integer
			dec_end[source] += segments;
		}

	   //Decryption	
	   mpi_errno=MPI_Recv(ciphertext_recv, blocktype_recv, MPI_CHAR, source, tag, comm,status);
	   dec_begin =dec_start[source];
	   for(i=0; i< blocktype_recv; i++){
		   *((char *)(buf+i)) = (char )(dec_calculator[source][dec_begin+i] ^ (ciphertext_recv[i]));
		   //memcpy(buf+i, &ch, 1);
	   }
	   dec_start[source] += blocktype_recv; 
	}

	dec_amount= dec_end[source] - dec_start[source];
	if(dec_amount<128){
		memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
		
		if(dec_end[source]>66432){
			//Restart from beginning
			dec_counter[source] = ((dec_counter[source]-1)/4160)*4160+4160;
			IV_Count(&dec_iv[source][0],dec_counter[source]);
			EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[source][0]);
			EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, 1024);
			
			dec_counter[source] += 64;
			dec_start[source] = 0;
		}else {
			IV_Count(&dec_iv[source][0],dec_counter[source]);
			dec_fin = dec_end[source];
			EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[source][0]);
			EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][dec_fin], &len, p, 128);
			dec_counter[source] += 8;

			dec_end[source] = (16*dec_counter[source])%66560;
		}
	}    

	return mpi_errno;

}