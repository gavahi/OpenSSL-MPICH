#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>



int main(int argc, char** argv) {


  MPI_Init(NULL, NULL);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  char local_char[300] = {'y','y','y'};
  char local_array[1000] = {'x','x','x','x'};
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  
  

 // printf("rank %d will send %x %c\n",world_rank, local_char, local_char);fflush(stdout);

  
  //MPI_Allgather(&local_char, 1, MPI_CHAR, local_array, 1, MPI_CHAR, MPI_COMM_WORLD); 

  //printf("rank %d received %x, %c %c\n",world_rank, local_array[0], local_array[0], local_array[1]);
  //fflush(stdout);

  printf("Try encrypted part............\n");fflush(stdout);
   init_crypto();
  local_array[0]='x';
  local_array[1]='x';
  local_array[2]='x';

   local_array[3]='x';
  local_array[4]='x';
  local_array[5]='x';

  if (world_rank == 0){
      local_char[0] = 'c';
      local_char[1] = 'd';
      local_char[2] = 'e';
  }
  else if (world_rank == 1){
      local_char[0] = 'F';
      local_char[1] = 'G';
      local_char[2] = 'H';
  }
    
  

  //printf("\nrank %d will send %x %c\n",world_rank, local_char, local_char);fflush(stdout);
  //MPI_SEC_Allgather(local_char, 3, MPI_CHAR, local_array, 3, MPI_CHAR, MPI_COMM_WORLD); 
  //printf("\nrank %d received  local_array[0]=%c,   local_array[1]=%c, local_array[2]=%c  local_array[3]=%c  local_array[4]=%c  local_array[4]=%c\n",
  //world_rank, local_array[0],local_array[1], local_array[2], local_array[3],local_array[4], local_array[5]);
 // fflush(stdout);
  //MPI_Barrier(MPI_COMM_WORLD);

  if(world_rank == 0){
      for(int i=0;i<10;i++)
         local_char[i]='9';
  }

  if(world_rank == 1){
      for(int i=0;i<10;i++)
         local_char[i]='8';
  }

  if(world_rank == 2){
      for(int i=0;i<10;i++)
         local_char[i]='7';
  }
  if(world_rank == 3){
      for(int i=0;i<9;i++)
         local_char[i]='1';
  local_char[9]='\0';
  }
  MPI_SEC_Allgather(local_char, 10, MPI_CHAR, local_array, 10, MPI_CHAR, MPI_COMM_WORLD);
  printf("\nrank %d received %s\n",world_rank, local_array); fflush(stdout);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
}
