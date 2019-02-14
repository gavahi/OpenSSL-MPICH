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
     for(i=0;i<1000;i++)
       send_buf[i]=105;

     for(i=1000;i<2100;i++)
       send_buf[i]= -99;  
  }
  
  //init_crypto();
 init_prectr_128();
  
  if (world_rank == 0) {   
    MPI_PreCtr_Send(send_buf, 1960, MPI_INT, 1, 0, MPI_COMM_WORLD);
    //MPI_Wait(&request, &status);
  } else if (world_rank == 1) {
    MPI_PreCtr_Recv(recv_buf, 1960, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    //MPI_Wait(&request, &status);
   // printf("Process 1 received: %d %d %d %d %d %d %d %d %d %d\n",
    //recv_buf[0], recv_buf[41], recv_buf[42], recv_buf[43], recv_buf[44], recv_buf[45],
    //recv_buf[46], recv_buf[47], recv_buf[8], recv_buf[9]);
    for(i=1900;i<1960;i++)
     printf("recv_buf[%d]=%d, ",i, recv_buf[i]);
   printf("\n");
  }

  MPI_Barrier(MPI_COMM_WORLD);

  double d_send_buf[sz];
  double d_recv_buf[sz];

  if(world_rank == 0){
     printf("double value test\n"); 
     for(i=0;i<50;i++)
       d_send_buf[i]=2.33;

     for(i=50;i<1200;i++)
       d_send_buf[i]= -4.56;  
  }

  if (world_rank == 0) {   
    MPI_PreCtr_Send(d_send_buf, 980, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
    //MPI_Wait(&request, &status);
  } else if (world_rank == 1) {
    MPI_PreCtr_Recv(d_recv_buf, 980, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &status);
    //MPI_Wait(&request, &status);
    //printf("Process 1 received: %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n",
    //d_recv_buf[0], d_recv_buf[1], d_recv_buf[2], d_recv_buf[3], d_recv_buf[4], d_recv_buf[5],
    //d_recv_buf[6], d_recv_buf[7], d_recv_buf[98], d_recv_buf[9]);
    for(i=960;i<980;i++){
      printf("d_recv_buf[%d]=%lf ",i,d_recv_buf[i]);
    }
    printf("\n");
  }


  MPI_Barrier(MPI_COMM_WORLD);
  

  char c_send_buf[sz];
  char c_recv_buf[sz];

  if(world_rank == 0){
     printf("char value test\n"); 
     for(i=0;i<6990;i++)
       c_send_buf[i]='a';

     for(i=6990;i<8000;i++)
       c_send_buf[i]= 'A';  
  }


#if 0
  for (int j=0;j<10000;j++){
    if (world_rank == 0) {   
      MPI_PreCtr_Send(c_send_buf, 7872, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
      //MPI_Wait(&request, &status);
    }else if (world_rank == 1) {
      MPI_PreCtr_Recv(c_recv_buf, 7872, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

      //for(i=6980;i<7000;i++){
        //printf("c_recv_buf[%d]=%c ",i,c_recv_buf[i]);
      //}
      //printf("\n");
    }
  }
  #endif

  if (world_rank == 0) {   
      MPI_PreCtr_Send(c_send_buf, 8000, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
      //MPI_Wait(&request, &status);
    }else if (world_rank == 1) {
      MPI_PreCtr_Recv(c_recv_buf, 8000, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

      for(i=7980;i<8000;i++){
        printf("c_recv_buf[%d]=%c ",i,c_recv_buf[i]);
      }
      printf("\n");
    }
      
  
  	
  MPI_Finalize();
}
