#include <mpi.h>
#include <stdio.h>
 
int main (int argc, char* argv[])
{
  int mypid, numprocs;
 
  MPI_Init (&argc, &argv);                        /* starts MPI */

  MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */
  MPI_Comm_size (MPI_COMM_WORLD, &numprocs);     /* get number of processes */

  printf( "Hello world from process %d of %d\n", mypid, numprocs );

  MPI_Finalize();
  return 0;
}

