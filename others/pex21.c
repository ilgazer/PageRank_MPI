#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
  int *a;
  MPI_Win win;
  MPI_Init(&argc, &argv);

  /* collectively create remote accessible memory in a window */
  MPI_Win_allocate(1000*sizeof(int), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &a, &win);

  /*******
  Array ‘a’ is now accessible from all processes in MPI_COMM_WORLD 
  e.g use MPI_Put etc.   
  *******/    


  MPI_Win_free(&win);

  MPI_Finalize(); 
  return 0;
}
