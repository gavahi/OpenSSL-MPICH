/*
This program will send 10
int, double and char to
process 1, from process 0.

*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#define sz 10000

int main(int argc, char** argv) {
  
  MPI_Init(NULL, NULL);
  
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  int world_size;
  MPI_Request request;
  MPI_Status status;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  

  // We are assuming at least 2 processes for this task
  //if (world_size < 2) {
    //fprintf(stderr, "World size must be greater than 1 for %s\n", argv[0]);
    //MPI_Abort(MPI_COMM_WORLD, 1);
  //}
 
  int send_buf[sz];
  int recv_buf[sz];
  int i,j, size;
  
  if(world_rank == 0){
    printf("int value test\n");
     for(i=0;i<500;i++)
       send_buf[i]=105;

     for(i=500;i<1400;i++)
       send_buf[i]= -99;  
  }
  
  
 init_prectr_128();

#if 0
  
  if (world_rank == 0) {   
    
    MPI_PreCtr_Isend(send_buf, 1200, MPI_INT, 1, 0, MPI_COMM_WORLD,&request);
    MPI_Psend_Wait(&request, &status,1);
  } else if (world_rank == 1) {
    MPI_PreCtr_Irecv(recv_buf, 1200, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
    MPI_Precv_Wait(&request, &status,0);
   // printf("Process 1 received: %d %d %d %d %d %d %d %d %d %d\n",
    //recv_buf[0], recv_buf[41], recv_buf[42], recv_buf[43], recv_buf[44], recv_buf[45],
    //recv_buf[46], recv_buf[47], recv_buf[8], recv_buf[9]);
    for(i=0;i<600;i++)
     printf("\nrecv_buf[%d]=%d \n",i, recv_buf[i]);
   printf("\n");
  }

  MPI_Barrier(MPI_COMM_WORLD);
 

  double d_send_buf[sz];
  double d_recv_buf[sz];

  if(world_rank == 0){
     printf("double value test\n"); 
     for(i=0;i<400;i++)
       d_send_buf[i]=2.33;

     for(i=400;i<1200;i++)
       d_send_buf[i]= -4.56;  
  }

  if (world_rank == 0) { 
    
    MPI_PreCtr_Isend(d_send_buf, 500, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD,&request);
    MPI_Psend_Wait(&request, &status,1);
  } else if (world_rank == 1) {
    MPI_PreCtr_Irecv(d_recv_buf, 500, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,&request);
    MPI_Precv_Wait(&request, &status,0);
    //printf("Process 1 received: %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n",
    //d_recv_buf[0], d_recv_buf[1], d_recv_buf[2], d_recv_buf[3], d_recv_buf[4], d_recv_buf[5],
    //d_recv_buf[6], d_recv_buf[7], d_recv_buf[98], d_recv_buf[9]);
    for(i=0;i<500;i++){
      printf("\nd_recv_buf[%d]=%lf\n ",i,d_recv_buf[i]);
    }
    printf("\n");
  }
 #endif

  MPI_Barrier(MPI_COMM_WORLD);

  char c_send_buf[sz];
  char c_recv_buf[sz];

  if(world_rank == 0){
     printf("char value test\n"); 
     for(i=0;i<90;i++)
       c_send_buf[i]='a';

     for(i= 90;i<8000;i++)
       c_send_buf[i]= 'A';  
  }



  for (int j=0;j<500;j++){
    if (world_rank == 0) {   

      MPI_PreCtr_Isend(c_send_buf,24, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &request);
      MPI_Psend_Wait(&request, &status,1);
    //MPI_PreCtr_Send(c_send_buf, 24, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
    }else if (world_rank == 1) {
      //MPI_PreCtr_Recv(c_recv_buf, 24, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
     MPI_PreCtr_Irecv(c_recv_buf, 24, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &request);
     MPI_Precv_Wait(&request, &status,0);

    
      for(i=0;i<5;i++){
        printf("[TIME: %d] c_recv_buf[%d]=%c ",j+1, i,c_recv_buf[i]);
      }
      printf("\n");
    }
  }


  	
  MPI_Finalize();
}
