#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

struct exstruct {
   int   i ; 
   char  c ; 
} ;
 
int main (int argc, char* argv[])
{
   int   mypid     ;
   int   numprocs  ;
   int rc ; 
   struct exstruct  s ;
   MPI_Datatype   datatype                ; 
   MPI_Datatype   types[2]                ;
   MPI_Aint       displs[2]               ;
   int            blockcounts[2]          ; 
 
   MPI_Init (&argc, &argv);                        /* starts MPI */

   MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */
   MPI_Comm_size (MPI_COMM_WORLD, &numprocs);     /* get number of processes */

   // construct user defined datatype for struct 
   blockcounts[0] =  1 ;
   blockcounts[1] =  1 ; 
   MPI_Address(&(s.i),&(displs[0])) ;
   MPI_Address(&(s.c),&(displs[1])) ;
   displs[1] = displs[1] - displs[0] ;
   displs[0] = (MPI_Aint) 0 ;
   types[0] = MPI_INT ;
   types[1] = MPI_CHAR ;
   MPI_Type_struct(2,blockcounts,displs,types,&datatype) ; 
   MPI_Type_commit(&datatype) ;  



   /* use the datatype */ 
   
   
   /* print some information about the datatype  */ 
   MPI_Aint extent ; 
   int size ; 

   MPI_Type_extent(datatype,&extent) ; 
   MPI_Type_size(datatype,&size) ; 
   printf("extent=%d, size=%d\n",extent,size) ;  
   

   /* in case you will never use the datatype again */ 
   MPI_Type_free(&datatype) ; 

   MPI_Finalize();
   return 0;
}

