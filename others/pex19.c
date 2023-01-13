#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
 
int main (int argc, char* argv[])
{
  int   mypid      ;
  int   all_sum[2]    ;
  int   prefix_sum[2] ; 
  int   target_sum[2] ; 
  int   data[2] ; 
  int target_proc ; 
  int rc ; 
 
  MPI_Init (&argc, &argv);                        /* starts MPI */

  MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */

  data[0] = mypid ; 
  data[1] = mypid+1 ; 
  target_proc = 1 ; 

   MPI_Reduce(data,target_sum,2,MPI_INT,MPI_SUM,target_proc,MPI_COMM_WORLD) ; 

   MPI_Allreduce(data,all_sum,2,MPI_INT,MPI_SUM,MPI_COMM_WORLD) ; 

   MPI_Scan(data,prefix_sum,2,MPI_INT,MPI_SUM,MPI_COMM_WORLD) ; 

   if (mypid == target_proc) {
    printf("[%d][0] A=%d P=%d T=%d\n",mypid,all_sum[0],prefix_sum[0],target_sum[0])  ; 
    printf("[%d][1] A=%d P=%d T=%d\n",mypid,all_sum[1],prefix_sum[1],target_sum[1])  ; 
   }
   else {
    printf("[%d][0] A=%d P=%d\n",mypid,all_sum[0],prefix_sum[0])  ; 
    printf("[%d][1] A=%d P=%d\n",mypid,all_sum[1],prefix_sum[1])  ; 
   }

   MPI_Finalize();
   return 0;
}
