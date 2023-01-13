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
  MPI_Status stat[2] ; 
  MPI_Request req[2] ; 
  int rc ; 
 
  MPI_Init (&argc, &argv);                        /* starts MPI */

  MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */
  MPI_Comm_size (MPI_COMM_WORLD, &numprocs);     /* get number of processes */

  if (argc < 2) {
      printf("Error: provide count argument\n") ; 
      exit(1) ; 
  }
  count = atoi(argv[1]) ; 
  sdata = (char *) calloc(count,sizeof(char)) ; 
  rdata = (char *) calloc(count,sizeof(char)) ; 

  src  = (mypid - 1 + numprocs) % numprocs  ;
  dest = (mypid + 1) % numprocs  ;

  printf("[%d] sending it dude\n",mypid) ; 

  MPI_Isend(sdata,count,MPI_CHAR,dest,1,MPI_COMM_WORLD,&req[0]) ; 
  MPI_Irecv(rdata,count,MPI_CHAR,src,MPI_ANY_TAG,MPI_COMM_WORLD,&req[1]) ; 

  /*  do some work here : overlap computation with communication */

  rc = MPI_Waitall(2,req,stat) ; 
  if (rc != MPI_SUCCESS) {
      printf("Error: message not received\n") ; 
      exit(1) ; 
  }

  printf("[%d] received it from %d with tag %d\n",mypid,stat[1].MPI_SOURCE,stat[1].MPI_TAG) ; 
  

  MPI_Finalize();
  return 0;
}

