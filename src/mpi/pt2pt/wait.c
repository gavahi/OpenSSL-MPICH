/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"

/* added by Abu Naser */
int waitCounter = 0;
/* end of add by Abu Naser*/
int psend_wait_counter, precv_wait_counter;
psend_wait_counter=0;
precv_wait_counter=0;

/* -- Begin Profiling Symbol Block for routine MPI_Wait */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Wait = PMPI_Wait
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_Wait  MPI_Wait
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Wait as PMPI_Wait
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPI_Wait(MPI_Request *request, MPI_Status *status) __attribute__((weak,alias("PMPI_Wait")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPI_Wait
#define MPI_Wait PMPI_Wait
#undef FUNCNAME
#define FUNCNAME MPIR_Wait_impl
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPIR_Wait_impl(MPI_Request *request, MPI_Status *status)
{
    int mpi_errno = MPI_SUCCESS;
    int active_flag;
    MPID_Request *request_ptr = NULL;

    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }

    MPID_Request_get_ptr(*request, request_ptr);

    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;

        /* If this is an anysource request including a communicator with
         * anysource disabled, convert the call to an MPI_Test instead so we
         * don't get stuck in the progress engine. */
        if (unlikely(MPIR_CVAR_ENABLE_FT &&
                    MPID_Request_is_anysource(request_ptr) &&
                    !MPID_Comm_AS_enabled(request_ptr->comm))) {
            mpi_errno = MPIR_Test_impl(request, &active_flag, status);
            goto fn_exit;
        }

	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
	    mpi_errno = MPIR_Grequest_progress_poke(1, &request_ptr, status);
	    if (request_ptr->kind == MPID_UREQUEST &&
                request_ptr->greq_fns->wait_fn != NULL)
	    {
		if (mpi_errno) {
		    /* --BEGIN ERROR HANDLING-- */
		    MPID_Progress_end(&progress_state);
                    MPIR_ERR_POP(mpi_errno);
                    /* --END ERROR HANDLING-- */
		}
		continue; /* treating UREQUEST like normal request means we'll
			     poll indefinitely. skip over progress_wait */
	    }

	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno) {
		/* --BEGIN ERROR HANDLING-- */
		MPID_Progress_end(&progress_state);
                MPIR_ERR_POP(mpi_errno);
		/* --END ERROR HANDLING-- */
	    }

            if (unlikely(
                        MPIR_CVAR_ENABLE_FT &&
                        MPID_Request_is_anysource(request_ptr) &&
                        !MPID_Request_is_complete(request_ptr) &&
                        !MPID_Comm_AS_enabled(request_ptr->comm))) {
                MPID_Progress_end(&progress_state);
                MPIR_ERR_SET(mpi_errno, MPIX_ERR_PROC_FAILED_PENDING, "**failure_pending");
                if (status != MPI_STATUS_IGNORE) status->MPI_ERROR = mpi_errno;
                goto fn_fail;
            }
	}
	MPID_Progress_end(&progress_state);
    }

    mpi_errno = MPIR_Request_complete(request, request_ptr, status, &active_flag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    
 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}

#endif

#undef FUNCNAME
#define FUNCNAME MPI_Wait
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
/*@
    MPI_Wait - Waits for an MPI request to complete

Input Parameters:
. request - request (handle) 

Output Parameters:
. status - status object (Status).  May be 'MPI_STATUS_IGNORE'.

.N waitstatus

.N ThreadSafe

.N Fortran

.N FortranStatus

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_REQUEST
.N MPI_ERR_ARG
@*/
int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
    MPID_Request * request_ptr = NULL;
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm * comm_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_WAIT);

    MPIR_ERRTEST_INITIALIZED_ORDIE();
    
    MPID_THREAD_CS_ENTER(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    MPID_MPI_PT2PT_FUNC_ENTER(MPID_STATE_MPI_WAIT);

    /* Check the arguments */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_ARGNULL(request, "request", mpi_errno);
	    /* NOTE: MPI_STATUS_IGNORE != NULL */
	    MPIR_ERRTEST_ARGNULL(status, "status", mpi_errno);
	    MPIR_ERRTEST_REQUEST_OR_NULL(*request, mpi_errno);
	}
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */
    
    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }
    
    /* Convert MPI request handle to a request object pointer */
    MPID_Request_get_ptr(*request, request_ptr);
    
    /* Validate object pointers if error checking is enabled */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            MPID_Request_valid_ptr( request_ptr, mpi_errno );
            if (mpi_errno) goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ... */

    /* save copy of comm because request will be freed */
    if (request_ptr)
        comm_ptr = request_ptr->comm;
    mpi_errno = MPIR_Wait_impl(request, status);
    if (mpi_errno) goto fn_fail;

    /* ... end of body of routine ... */
    
  fn_exit:
    MPID_MPI_PT2PT_FUNC_EXIT(MPID_STATE_MPI_WAIT);
    MPID_THREAD_CS_EXIT(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    return mpi_errno;
	
  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#ifdef HAVE_ERROR_CHECKING
    mpi_errno = MPIR_Err_create_code(mpi_errno, MPIR_ERR_RECOVERABLE, 
				     FCNAME, __LINE__, MPI_ERR_OTHER,
				     "**mpi_wait", "**mpi_wait %p %p", 
				     request, status);
#endif
    mpi_errno = MPIR_Err_return_comm(comm_ptr, FCNAME, mpi_errno);
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

/* This implementation is for variable nonce */
int MPI_SEC_Wait(MPI_Request *request, MPI_Status *status){
    
    int mpi_errno = MPI_SUCCESS;
    int  recv_sz=0; 
    //int var;
    unsigned long count;          
       
    mpi_errno=MPI_Wait(request, status);
    MPI_Datatype datatype = MPI_CHAR;
    MPI_Get_count(status, datatype, &recv_sz);
    //var = openssl_dec_core(ciphertext,0,buf,0,blocktype_recv);
    openssl_dec_core(&Ideciphertext[waitCounter][0],0,bufptr[waitCounter],0,recv_sz-16);
     /*var = crypto_aead_aes256gcm_decrypt_afternm(bufptr[waitCounter], &count,
                                  NULL,
                                  &Ideciphertext[waitCounter][12], (unsigned long)(recv_sz-12),
                                  NULL,
                                  0,
                                  &Ideciphertext[waitCounter][0],(const crypto_aead_aes256gcm_state *) &ctx);
    if(var != 0){
        printf("Decryption failed\n");
        fflush(stdout);
    } 
*/
    waitCounter++;
    if(waitCounter == (500-1))
        waitCounter=0;
    return mpi_errno;
}
/* end of add by abu naser */



/***********************************************************************************/

                   /*Add Pre-Ctr Isend Mode implementation*/

/***********************************************************************************/

int MPIR_Psendwait_impl(MPI_Request *request, MPI_Status *status)
{
    int mpi_errno = MPI_SUCCESS;
    int active_flag,pre_fin,len,x,y;
    MPID_Request *request_ptr = NULL;
    const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int dest;

    dest=enc_dest[psend_wait_counter];
    psend_wait_counter++;
    if(psend_wait_counter==1024){
        psend_wait_counter=0;
    }   

    psend_counter++;
    if(psend_counter==1024){
        psend_counter=0;
    }



    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }

    MPID_Request_get_ptr(*request, request_ptr);

    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;

        /* If this is an anysource request including a communicator with
         * anysource disabled, convert the call to an MPI_Test instead so we
         * don't get stuck in the progress engine. */
        if (unlikely(MPIR_CVAR_ENABLE_FT &&
                    MPID_Request_is_anysource(request_ptr) &&
                    !MPID_Comm_AS_enabled(request_ptr->comm))) {
            mpi_errno = MPIR_Test_impl(request, &active_flag, status);
            goto fn_exit;
        }

	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
        /*add for Pre-CTR Isend*/
/***********************************************************************************/
        amount= pre_end[dest] - pre_start[dest];
        
   	    if(amount<128){
		    if(pre_end[dest]<66560){
                memcpy(&enc_iv[dest][0],&IV[dest][0],16);
                IV_Count(&enc_iv[dest][0],iv_counter[dest]);
			    EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &enc_iv[dest][0]);
			    pre_fin = pre_end[dest];
                x=66560-pre_end[dest];
                y= ((x) < (128)) ? (x) : (128);
			    EVP_EncryptUpdate(ctx_enc, &pre_calculator[dest][pre_fin], &len, p, y);
			    iv_counter[dest] += (y/16);
                pre_end[dest] += y;
            
			//printf("\n Enc! pre_calculator: %c %c %c %c %c %c %c %c  \n",
            //pre_calculator[0][pre_end], pre_calculator[0][pre_end+1], pre_calculator[0][pre_end+2], pre_calculator[0][pre_end+4],
            //pre_calculator[0][pre_end+5], pre_calculator[0][pre_end+6], pre_calculator[0][pre_end+7], pre_calculator[0][pre_end+8]);

		    }
	    }	
        /*end of add*/
/***********************************************************************************/
	    mpi_errno = MPIR_Grequest_progress_poke(1, &request_ptr, status);
	    if (request_ptr->kind == MPID_UREQUEST &&
                request_ptr->greq_fns->wait_fn != NULL)
	    {
		if (mpi_errno) {
		    /* --BEGIN ERROR HANDLING-- */
		    MPID_Progress_end(&progress_state);
                    MPIR_ERR_POP(mpi_errno);
                    /* --END ERROR HANDLING-- */
		}
		continue; /* treating UREQUEST like normal request means we'll
			     poll indefinitely. skip over progress_wait */
	    }

	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno) {
		/* --BEGIN ERROR HANDLING-- */
		MPID_Progress_end(&progress_state);
                MPIR_ERR_POP(mpi_errno);
		/* --END ERROR HANDLING-- */
	    }



            if (unlikely(
                        MPIR_CVAR_ENABLE_FT &&
                        MPID_Request_is_anysource(request_ptr) &&
                        !MPID_Request_is_complete(request_ptr) &&
                        !MPID_Comm_AS_enabled(request_ptr->comm))) {
                MPID_Progress_end(&progress_state);
                MPIR_ERR_SET(mpi_errno, MPIX_ERR_PROC_FAILED_PENDING, "**failure_pending");
                if (status != MPI_STATUS_IGNORE) status->MPI_ERROR = mpi_errno;
                goto fn_fail;
            }
	}
	MPID_Progress_end(&progress_state);
    }

    mpi_errno = MPIR_Request_complete(request, request_ptr, status, &active_flag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    
 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
/***********************************************************************************/
/***********************************************************************************/

int MPI_Psend_Wait(MPI_Request *request, MPI_Status *status)
{
    MPID_Request * request_ptr = NULL;
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm * comm_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_WAIT);

    MPIR_ERRTEST_INITIALIZED_ORDIE();
    
    MPID_THREAD_CS_ENTER(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    MPID_MPI_PT2PT_FUNC_ENTER(MPID_STATE_MPI_WAIT);

    /* Check the arguments */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_ARGNULL(request, "request", mpi_errno);
	    /* NOTE: MPI_STATUS_IGNORE != NULL */
	    MPIR_ERRTEST_ARGNULL(status, "status", mpi_errno);
	    MPIR_ERRTEST_REQUEST_OR_NULL(*request, mpi_errno);
	}
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */
    
    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }
    
    /* Convert MPI request handle to a request object pointer */
    MPID_Request_get_ptr(*request, request_ptr);
    
    /* Validate object pointers if error checking is enabled */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            MPID_Request_valid_ptr( request_ptr, mpi_errno );
            if (mpi_errno) goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ... */

    /* save copy of comm because request will be freed */
    if (request_ptr)
        comm_ptr = request_ptr->comm;


    /*MODIFY FOR PRE-CTR MODE*/    
    mpi_errno = MPIR_Psendwait_impl(request, status);    
    if (mpi_errno) goto fn_fail;

    /* ... end of body of routine ... */
    
  fn_exit:
    MPID_MPI_PT2PT_FUNC_EXIT(MPID_STATE_MPI_WAIT);
    MPID_THREAD_CS_EXIT(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    return mpi_errno;
	
  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#ifdef HAVE_ERROR_CHECKING
    mpi_errno = MPIR_Err_create_code(mpi_errno, MPIR_ERR_RECOVERABLE, 
				     FCNAME, __LINE__, MPI_ERR_OTHER,
				     "**mpi_wait", "**mpi_wait %p %p", 
				     request, status);
#endif
    mpi_errno = MPIR_Err_return_comm(comm_ptr, FCNAME, mpi_errno);
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}


/***********************************************************************************/
/*                         Pre-CTR Wait for Irecv                                  */
/***********************************************************************************/
int MPIR_Precvwait_impl(MPI_Request *request, MPI_Status *status)
{
    int mpi_errno = MPI_SUCCESS;
    int active_flag,dec_fin,len,recve_sz,segments,x,y, source;
    MPID_Request *request_ptr = NULL;
    const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    MPI_Datatype datatype = MPI_CHAR;
    MPI_Get_count(status, datatype, &recve_sz);

    source= dec_source[precv_wait_counter];

    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }
    
    MPID_Request_get_ptr(*request, request_ptr);

    if (!MPID_Request_is_complete(request_ptr))
    {
	MPID_Progress_state progress_state;

        /* If this is an anysource request including a communicator with
         * anysource disabled, convert the call to an MPI_Test instead so we
         * don't get stuck in the progress engine. */
        if (unlikely(MPIR_CVAR_ENABLE_FT &&
                    MPID_Request_is_anysource(request_ptr) &&
                    !MPID_Comm_AS_enabled(request_ptr->comm))) {
            mpi_errno = MPIR_Test_impl(request, &active_flag, status);
            goto fn_exit;
        }

	MPID_Progress_start(&progress_state);
        while (!MPID_Request_is_complete(request_ptr))
	{
/***********************************************************************************/
        /*add for Pre-CTR Irecv*/
        if (dec_flag[source] !=1){
            dec_amount= dec_end[source] - dec_start[source];
	        if(dec_amount<128){ 
		        if(dec_end[source]<66560){
                    x=66560-dec_end[source];
                    y= ((x) < (128)) ? (x) : (128);
                    memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);  
                    //printf("\n [RECEIVE] dec_end[source] = %d \n",dec_end[source]);
                    IV_Count(&dec_iv[source][0],dec_counter[source]);
			        dec_fin = dec_end[source];
			        EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[source][0]);
			        EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][dec_fin], &len, p, y);
			        dec_counter[source] += (y/16);
			        dec_end[source] += y;
		        }
	        }  
        }
        /*end of add*/
/***********************************************************************************/

	    mpi_errno = MPIR_Grequest_progress_poke(1, &request_ptr, status);
	    if (request_ptr->kind == MPID_UREQUEST &&
                request_ptr->greq_fns->wait_fn != NULL)
	    {
		if (mpi_errno) {
		    /* --BEGIN ERROR HANDLING-- */
		    MPID_Progress_end(&progress_state);
                    MPIR_ERR_POP(mpi_errno);
                    /* --END ERROR HANDLING-- */
		}
		continue; /* treating UREQUEST like normal request means we'll
			     poll indefinitely. skip over progress_wait */
	    }

	    mpi_errno = MPID_Progress_wait(&progress_state);
	    if (mpi_errno) {
		/* --BEGIN ERROR HANDLING-- */
		MPID_Progress_end(&progress_state);
                MPIR_ERR_POP(mpi_errno);
		/* --END ERROR HANDLING-- */
	    }

            if (unlikely(
                        MPIR_CVAR_ENABLE_FT &&
                        MPID_Request_is_anysource(request_ptr) &&
                        !MPID_Request_is_complete(request_ptr) &&
                        !MPID_Comm_AS_enabled(request_ptr->comm))) {
                MPID_Progress_end(&progress_state);
                MPIR_ERR_SET(mpi_errno, MPIX_ERR_PROC_FAILED_PENDING, "**failure_pending");
                if (status != MPI_STATUS_IGNORE) status->MPI_ERROR = mpi_errno;
                goto fn_fail;
            }
	}
	MPID_Progress_end(&progress_state);
    }

    mpi_errno = MPIR_Request_complete(request, request_ptr, status, &active_flag);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    
 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}
/***********************************************************************************/
/***********************************************************************************/

int Prectr_Irecv_Wait(MPI_Request *request, MPI_Status *status)
{
    MPID_Request * request_ptr = NULL;
    int mpi_errno = MPI_SUCCESS;
    MPID_Comm * comm_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_WAIT);

    MPIR_ERRTEST_INITIALIZED_ORDIE();
    
    MPID_THREAD_CS_ENTER(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    MPID_MPI_PT2PT_FUNC_ENTER(MPID_STATE_MPI_WAIT);

    /* Check the arguments */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_ARGNULL(request, "request", mpi_errno);
	    /* NOTE: MPI_STATUS_IGNORE != NULL */
	    MPIR_ERRTEST_ARGNULL(status, "status", mpi_errno);
	    MPIR_ERRTEST_REQUEST_OR_NULL(*request, mpi_errno);
	}
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */
    
    /* If this is a null request handle, then return an empty status */
    if (*request == MPI_REQUEST_NULL)
    {
	MPIR_Status_set_empty(status);
	goto fn_exit;
    }
    
    /* Convert MPI request handle to a request object pointer */
    MPID_Request_get_ptr(*request, request_ptr);
    
    /* Validate object pointers if error checking is enabled */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            MPID_Request_valid_ptr( request_ptr, mpi_errno );
            if (mpi_errno) goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ... */

    /* save copy of comm because request will be freed */
    if (request_ptr)
        comm_ptr = request_ptr->comm;
    /* Add for Pre-CTR Mode */
    mpi_errno = MPIR_Precvwait_impl(request, status); 
    if (mpi_errno) goto fn_fail;

    /* ... end of body of routine ... */
    
  fn_exit:
    MPID_MPI_PT2PT_FUNC_EXIT(MPID_STATE_MPI_WAIT);
    MPID_THREAD_CS_EXIT(GLOBAL, MPIR_THREAD_GLOBAL_ALLFUNC_MUTEX);
    return mpi_errno;
	
  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#ifdef HAVE_ERROR_CHECKING
    mpi_errno = MPIR_Err_create_code(mpi_errno, MPIR_ERR_RECOVERABLE, 
				     FCNAME, __LINE__, MPI_ERR_OTHER,
				     "**mpi_wait", "**mpi_wait %p %p", 
				     request, status);
#endif
    mpi_errno = MPIR_Err_return_comm(comm_ptr, FCNAME, mpi_errno);
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

/***********************************************************************************/

/*                   This implementation is for Pre-CTR Irecv                      */
/***********************************************************************************/
int MPI_Precv_Wait(MPI_Request *request, MPI_Status *status){
    
    int mpi_errno = MPI_SUCCESS;
    int  recv_sz=0; 
    int i,len,segments,dec_begin,dec_fin,rmd,source;
    const unsigned char gcm_key[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned long count;          
       
    mpi_errno=Prectr_Irecv_Wait(request, status);
    MPI_Datatype datatype = MPI_CHAR;
    MPI_Get_count(status, datatype, &recv_sz);
    source =dec_source[precv_wait_counter];

    //printf("\n waitCounter= %d !! recv_sz = %d !!dec_start[source]==%d\n",waitCounter,recv_sz,dec_start[source]);
    dec_end[source] = (16*dec_counter[source])%66560;
	dec_amount= dec_end[source] - dec_start[source];

    if (dec_flag[source]==1){
        memcpy(&Recv_IV[source][0],&Ideciphertext[waitCounter][0],16);
        if (recv_sz <=1040){
		   EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &Recv_IV[source][0]);
	       EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, 1024);
		   dec_counter[source] = 64;
		   dec_start[source] = 0;
		   dec_end [source]= 1024;
		}else{
		   segments =((recv_sz-17)/16)*16+16;//upper integer multi of 16
		   EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &Recv_IV[source][0]);
		   EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, segments); 
		   dec_counter[source] = (segments/16); //upper integer
		   dec_end[source]= segments;
		   dec_start[source] = 0;
		}	  
	  //Decryption
      printf("\n[IRECV]:Dec_Start=%d ,Dec_End=%d Dec_Counter=%d\n",dec_start[source], dec_end[source],dec_counter[source]);
	   for(i=0; i< recv_sz-16; i++){
		   *((char *)(bufptr[waitCounter]+i)) = (char )(dec_calculator[source][i] ^ (Ideciphertext[waitCounter][i+16]));
		   //memcpy(buf+i, &ch, 1);
	   }
	   dec_flag[source] =0;
	   dec_start[source] = recv_sz-16;

	}else{
		if (recv_sz >= 66560-dec_start[source]){
            //Calculate with the rest of the Array
            segments = 66560-dec_end[source];
            if(segments !=0){
                memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
                IV_Count(&dec_iv[source][0],dec_counter[source]);
            
                dec_fin = dec_end[source];
                EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[0][0]);
			    EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][dec_fin], &len, p, segments); 
                dec_counter[source] += (segments-1)/16 +1;
            }
            rmd = 66560-dec_start[source];
	        dec_begin =dec_start[source];
            printf("\n[IRECV]:Dec_Start=%d ,Dec_End=%d Dec_Counter=%d\n",dec_start[source], dec_end[source],dec_counter[source]);
	        for(i=0; i< rmd; i++){
		        *((char *)(bufptr[waitCounter]+i)) = (char )(dec_calculator[source][dec_begin+i] ^ (Ideciphertext[waitCounter][i]));   
            }
            //Cycle start
            printf("\n[3333-Irecv-3333]: dec_counter=%d, rmd=%d\n",dec_counter[source],rmd);
            memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
            IV_Count(&dec_iv[source][0],dec_counter[source]);
            segments =((recv_sz-rmd-1)/16)*16+16;
            EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[source][0]);
            EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][0], &len, p, segments);
            dec_counter[source] += (segments-1)/16+1;
            dec_end[source] = segments;
            printf("\n[IRECV]:Dec_Start=%d ,Dec_End=%d Dec_Counter=%d\n",dec_start[source], dec_end[source],dec_counter[source]);
            for(i=0; i< recv_sz-rmd; i++){
		        *((char *)(bufptr[waitCounter]+i+rmd)) = (char )(dec_calculator[source][i] ^ (Ideciphertext[waitCounter][rmd+i]));
	        }
            dec_start[source]=recv_sz-rmd;

		}else if(recv_sz > dec_amount){
			//Add more dec-ctr blocks
			memcpy(&dec_iv[source][0],&Recv_IV[source][0],16);
		    IV_Count(&dec_iv[source][0],dec_counter[source]);
			segments =((recv_sz-dec_amount-1)/16)*16+16;
			dec_fin = dec_end[source];
			EVP_EncryptInit_ex(ctx_enc, NULL, NULL, gcm_key, &dec_iv[0][0]);
			EVP_EncryptUpdate(ctx_enc, &dec_calculator[source][dec_fin], &len, p, segments); 
			dec_counter[source] += (segments/16); //upper integer
			dec_end[source] += segments;

            //Decryption	
	        dec_begin =dec_start[source];
            printf("\n[IRECV]:Dec_Start=%d ,Dec_End=%d Dec_Counter=%d\n",dec_start[source], dec_end[source],dec_counter[source]);
	        for(i=0; i< recv_sz; i++){
		            *((char *)(bufptr[waitCounter]+i)) = (char )(dec_calculator[source][dec_begin+i] ^ (Ideciphertext[waitCounter][i]));
	        }
	        dec_start[source] += recv_sz;

		}else{
            //Decryption	
	        dec_begin =dec_start[source];
            printf("\n[IRECV]:Dec_Start=%d ,Dec_End=%d Dec_Counter=%d\n",dec_start[source], dec_end[source],dec_counter[source]);
	        for(i=0; i< recv_sz; i++){
		            *((char *)(bufptr[waitCounter]+i)) = (char )(dec_calculator[source][dec_begin+i] ^ (Ideciphertext[waitCounter][i]));
	        }
	        dec_start[source] += recv_sz;
        }
	  
	}

    precv_wait_counter++;
    if(precv_wait_counter==1024){
        precv_wait_counter=0;
    }

    waitCounter++;
    if(waitCounter == (500-1))
        waitCounter=0;
    return mpi_errno;
}