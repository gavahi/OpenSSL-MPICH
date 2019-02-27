#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define FLOAT_PRECISION 5
#define FIELD_WIDTH 20
#define sz 4*1024*1024
#define MG 1048576.00
#define low 1*1024//4*1024//512*1024 
#define high 4*1024*1024//(4*1024*1024)*1.1
#define times 3000
#define normal 0
#define max_pack 2*1024
#define SIZE_NUM 11
#define key_size 16
#define GCM 0
#define OCB 0
#define CTR 0
#define PreCtr 1

//static int lengths[SIZE_NUM] = { 1 , 16 , 256 , 1024, 4*1024 , 16*1024 , 64*1024 , 256*1024 , 1024*1024 , 2*1024*1024 , 4* 1024*1024};
static int lengths[SIZE_NUM] =   { 1 , 16 , 32,    48,    128,     256,       512,      1*1024,    2*1024,    3*1024,   4*1024};


int main(int argc, char** argv) {

  MPI_Init(NULL, NULL);
  int world_rank,world_size,j,datasz,iteration,datasz1;
  char * sendbuf, *recvbuf;
  double t_start, t_end, t;
	MPI_Request request;
	MPI_Status status;
  //struct timeval  tv1, tv2;

  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  sendbuf = (char *)malloc(sz * sizeof(char));
  recvbuf = (char *)malloc(sz * sizeof(char));

  memset(sendbuf,'a',4194304);
  memset(recvbuf,'b',4194304);


  // We are assuming at least 2 processes for this 
  if (world_size != 2) {
    fprintf(stderr, "World size must be two for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

   if (key_size==16 && GCM==1) {
		if(world_rank == 0) printf("\n\t\t****** Secure Run with OpenSSL  128 ********\n");
		init_openssl_128();
	}
	else if(key_size==32 && GCM==1) {
		if(world_rank == 0)  printf("\n\t\t****** Secure Run with OpenSSL  256 ********\n");
		init_openssl_256();
	}
	else if (key_size==16 && OCB==1) {
		if(world_rank == 0) printf("\n\t\t****** Secure Run with OpenSSL OCB 128 ********\n");
		init_ocb_128();
	}
	else if(key_size==32 && OCB==1) {
		if(world_rank == 0)  printf("\n\t\t****** Secure Run with OpenSSL OCB 256 ********\n");
		init_ocb_256();
	}
	else if (key_size==16 && CTR==1) {
		if(world_rank == 0) printf("\n\t\t****** Secure Run with OpenSSL CTR 128 ********\n");
		init_ctr_128();
	}
	else if(key_size==32 && CTR==1) {
		if(world_rank == 0)  printf("\n\t\t****** Secure Run with OpenSSL CTR 256 ********\n");
		init_ctr_256();
	}
	else if (key_size==16 && PreCtr==1) {
		if(world_rank == 0) printf("\n\t\t****** Secure Run with OpenSSL PreCtr 128 ********\n");
		init_prectr_128();
	}
	else if(key_size==32 && PreCtr==1) {
		if(world_rank == 0)  printf("\n\t\t****** Secure Run with OpenSSL PreCtr 256 ********\n");
		init_prectr_256(1024);
	}
	
	

   iteration = times;
   //init_openssl_128();
   //init_openssl_256();
   
  //if (world_rank == 0) {printf("\n\t\t****** Secure Run with OpenSSL  ********\n");} 
	if(world_rank == 0) printf("\n# Size           Bandwidth (MB/s)\n");
	
	iteration = times;
	int indx;
	
	if (normal)
	   for(indx=0; indx<SIZE_NUM; indx++){    
			datasz=lengths[indx];
			if(datasz > 1024*1024)       iteration = 1000;

		   for(j=1;j<=iteration+20;j++){
					if(j == 20){
					  t_start = MPI_Wtime();
					}
					if (world_rank == 0) {
						  MPI_Send(sendbuf, datasz, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
						  MPI_Recv(recvbuf, datasz, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

					}
					else if(world_rank == 1){
						  MPI_Recv(recvbuf, datasz, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						  MPI_Send(sendbuf, datasz, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
					}
				}
			t_end = MPI_Wtime();
			if(world_rank == 0) {
				t = t_end - t_start;
				 double tmp = (datasz * 2) / MG * iteration;

				// printf("** %d total data sent in MB = %f time took %f\n", datasz,tmp,t);fflush(stdout);
			   fprintf(stdout, "%-*d%*.*f\n", 10, datasz, FIELD_WIDTH,
							FLOAT_PRECISION, tmp / t);

				fflush(stdout);
				}
	  }

  //for(datasz=1; datasz<=4194304; datasz*=2){
	//for(datasz=low; datasz<=high; datasz*=2){
	if (PreCtr==1){
		for(indx=0; indx<SIZE_NUM; indx++){    
		datasz=lengths[indx];
		if(datasz > 1024*1024)       iteration = 1000;
		for(j=1;j<=iteration+20;j++){
				if(j == 20){
				  t_start = MPI_Wtime();
				  //gettimeofday(&tv1, NULL);
				}
				//openssl_init();
				if (world_rank == 0) {
            MPI_PreCtr_Isend(sendbuf, datasz, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &request);
            MPI_Psend_Wait(&request, &status,1);
					  
					  MPI_PreCtr_Irecv(recvbuf, datasz, MPI_CHAR, 1, 1, MPI_COMM_WORLD,  &request);
						MPI_Precv_Wait(&request, &status,1);
					//printf("%d sent %d amount of data to 1\n", world_rank, datasz);
				}
				else if(world_rank == 1){
					//if(datasz > 16)
					  //MPI_SEC_Recv(recvbuf, datasz-16, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					//else
					  MPI_PreCtr_Irecv(recvbuf, datasz, MPI_CHAR, 0, 0, MPI_COMM_WORLD,  &request);
						MPI_Precv_Wait(&request, &status,0);
					  MPI_PreCtr_Isend(sendbuf, datasz, MPI_CHAR, 0, 1, MPI_COMM_WORLD,&request);
						MPI_Psend_Wait(&request, &status,0);
					//printf("%d received %d amount from process 0\n", world_rank, datasz);
			}
		}
		t_end = MPI_Wtime();
	   // gettimeofday(&tv2, NULL);
		if(world_rank == 0) {
			// t_end = MPI_Wtime();
			t = t_end - t_start;
			//printf("datasz =%d t = %lf\n", datasz,t);
			// tmp = Total data sent in MegaByte
			//double tmp = datasz / 1e6 * 1000;
			 double tmp = (datasz * 2) / MG * iteration;
			//printf("datasz =%d tmp = %lf\n", datasz,tmp);
			//printf("** %d total data sent in MB = %f time took %f\n", datasz,tmp,t);fflush(stdout);
			fprintf(stdout, "%-*d%*.*f\n", 10, datasz, FIELD_WIDTH,	FLOAT_PRECISION, tmp / t);

			fflush(stdout);
        }
	}
	}
	else {
	for(indx=0; indx<SIZE_NUM; indx++){    
		datasz=lengths[indx];
		if(datasz > 1024*1024)       iteration = 1000;
		for(j=1;j<=iteration+20;j++){
				if(j == 20){
				  t_start = MPI_Wtime();
				  //gettimeofday(&tv1, NULL);
				}
				//openssl_init();
				if (world_rank == 0) {
					  MPI_SEC_Send(sendbuf, datasz, MPI_CHAR, 1, 0, MPI_COMM_WORLD,max_pack);
					  MPI_SEC_Recv(recvbuf, datasz, MPI_CHAR, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE,max_pack);
					//printf("%d sent %d amount of data to 1\n", world_rank, datasz);
				}
				else if(world_rank == 1){
					//if(datasz > 16)
					  //MPI_SEC_Recv(recvbuf, datasz-16, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					//else
					  MPI_SEC_Recv(recvbuf, datasz, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE,max_pack);
					  MPI_SEC_Send(sendbuf, datasz, MPI_CHAR, 0, 1, MPI_COMM_WORLD,max_pack);
					//printf("%d received %d amount from process 0\n", world_rank, datasz);
			}
		}
		t_end = MPI_Wtime();
	   // gettimeofday(&tv2, NULL);
		if(world_rank == 0) {
			// t_end = MPI_Wtime();
			t = t_end - t_start;
			//printf("datasz =%d t = %lf\n", datasz,t);
			// tmp = Total data sent in MegaByte
			//double tmp = datasz / 1e6 * 1000;
			 double tmp = (datasz * 2) / MG * iteration;
			//printf("datasz =%d tmp = %lf\n", datasz,tmp);
			//printf("** %d total data sent in MB = %f time took %f\n", datasz,tmp,t);fflush(stdout);
			fprintf(stdout, "%-*d%*.*f\n", 10, datasz, FIELD_WIDTH,	FLOAT_PRECISION, tmp / t);

			fflush(stdout);
        }
	}	
	}
  free(sendbuf);
  free(recvbuf);

  MPI_Finalize();
}
