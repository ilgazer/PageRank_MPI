#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
 
int main (int argc, char* argv[])
{
  int   mypid     ;
  int   numprocs  ;
  int   src       ;
  int   dest      ;
  char *sdata     ; 
  char *rdata     ; 
  int   count     ; 
  MPI_Status stat ; 
 
  MPI_Init (&argc, &argv);                        /* starts MPI */

  MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */
  MPI_Comm_size (MPI_COMM_WORLD, &numprocs);     /* get number of processes */

  if (argc < 2) {
      printf("Error: provide count argument\n") ; 
      exit(1) ; 
  }
  count = 1 << 28 ; 
  sdata = (char *) calloc(count,sizeof(char)) ; 
  rdata = (char *) calloc(count,sizeof(char)) ; 

  src  = (mypid - 1 + numprocs) % numprocs  ;
  dest = (mypid + 1) % numprocs  ;

  printf("[%d] sending it dude\n",mypid) ; 

  MPI_Send(sdata,count,MPI_CHAR,dest,1,MPI_COMM_WORLD) ; 
  MPI_Recv(rdata,count,MPI_CHAR,src,MPI_ANY_TAG,MPI_COMM_WORLD,&stat) ; 

  printf("[%d] received from %d with tag %d\n",mypid,stat.MPI_SOURCE,stat.MPI_TAG) ; 
  

  MPI_Finalize();
  return 0;
}

