#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>
#include <math.h>

// generated by gui apps
#define PARTITION_WIDTH 3
#define PARTITION_HEIGHT 6
#define BOUNDARY_LEFT 10
#define BOUNDARY_RIGHT -10
#define BOUNDARY_TOP 30
#define BOUNDARY_BOTTOM 200
#define ERROR_MARGIN 1e-9
#define STEP_MAX 1e6

double **element_local, **element_global, temp; // the whole array, kept at node_rank==0
double *buffer_recv, *buffer_send; // line buffer
unsigned int step=0, divergence_local=1, divergence_global=1;
int node_count, node_rank, node_namelen;
char node_name[MPI_MAX_PROCESSOR_NAME];
struct timeval start_time, stop_time;
struct timeval process_start_time, process_stop_time;
double total_processing_time; // sec
int i, j, k;
MPI_Status status;

int main(int argc, char *argv[]){
	buffer_recv = malloc(sizeof(double)*(PARTITION_WIDTH+1));
	buffer_send = malloc(sizeof(double)*(PARTITION_WIDTH+1));
	
	element_local = malloc(sizeof(double*)*(PARTITION_HEIGHT));
	for (j=0; j<PARTITION_HEIGHT; j++){
		element_local[j] = malloc(sizeof(double)*(PARTITION_WIDTH+1));
		}
	
	element_global = malloc(sizeof(double*)*PARTITION_HEIGHT);
	for (j=0; j<PARTITION_HEIGHT; j++){
		element_global[j] = malloc(sizeof(double)*PARTITION_WIDTH*2);
		}
	
	// assigning boundary values
	for (j=0; j<PARTITION_HEIGHT; j++){
		element_global[j][0] = BOUNDARY_LEFT;
		element_global[j][2*PARTITION_WIDTH-1] = BOUNDARY_RIGHT;
		}
	for (i=0; i<PARTITION_WIDTH*2; i++){
		element_global[0][i] = BOUNDARY_TOP;
		element_global[PARTITION_HEIGHT-1][i] = BOUNDARY_BOTTOM;
		}
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &node_count);
	MPI_Comm_rank(MPI_COMM_WORLD, &node_rank);
	MPI_Get_processor_name(node_name, &node_namelen);

// start 
if (!node_rank){
	gettimeofday(&start_time,NULL);
	//printf("bagiempat: start = %d.%d\n",start_time.tv_sec,start_time.tv_usec);
	}

// do the actual work here
while(divergence_global && (step<STEP_MAX)){
	step++;
	divergence_global=0;
	// assigment to element_local from master node
	for (k=0; k<=1; k++){
		for (j=0; j<PARTITION_HEIGHT; j++){
			if (node_rank==0){
				// left section
				if (k==0) memcpy(buffer_send,element_global[j],sizeof(double)*(PARTITION_WIDTH+1));
				// right section
				else if (k==1) memcpy(buffer_send,&(element_global[j][PARTITION_WIDTH-1]),sizeof(double)*(PARTITION_WIDTH+1));
				MPI_Send(buffer_send,PARTITION_WIDTH+1,MPI_DOUBLE,k,1,MPI_COMM_WORLD);
				}
			if (node_rank==k){
				MPI_Recv(buffer_recv,PARTITION_WIDTH+1,MPI_DOUBLE,0,1,MPI_COMM_WORLD,&status);
				memcpy(element_local[j],buffer_recv,sizeof(double)*(PARTITION_WIDTH+1));
				}
			}
		}
	
										
	// computing element_local: 4-way mean for elements within each partition
	// divergence check
	gettimeofday(&process_start_time,NULL);	
	divergence_local=0;
	for (j=1; j<PARTITION_HEIGHT-1; j++){
		for (i=1; i<PARTITION_WIDTH; i++){
			temp = 0.25*(element_local[j-1][i]+element_local[j+1][i]+\
						 element_local[j][i-1]+element_local[j][i+1]);
				if (fabs(element_local[j][i]-temp)>ERROR_MARGIN){
					
					divergence_local=1;
					element_local[j][i]=temp;
					}
			}
		}
	gettimeofday(&process_stop_time,NULL);
	total_processing_time += (process_stop_time.tv_sec-process_start_time.tv_sec+\
		(double)(process_stop_time.tv_usec-process_start_time.tv_usec)/1000000);
	
	// summing divergence values
	for (k=0; k<node_count; k++){
		MPI_Reduce(&divergence_local,&divergence_global,1,MPI_UNSIGNED,MPI_SUM,k,MPI_COMM_WORLD);
		}
	
	// update to element_global
	for (k=0; k<node_count; k++){
		for (j=1; j<PARTITION_HEIGHT-1; j++){
			// send
			if (node_rank==k){
				if (node_rank==0) memcpy(buffer_send,element_local[j],sizeof(double)*PARTITION_WIDTH);
				else if (node_rank==1) memcpy(buffer_send,&(element_local[j][1]),sizeof(double)*PARTITION_WIDTH);
				MPI_Send(buffer_send,PARTITION_WIDTH,MPI_DOUBLE,0,2,MPI_COMM_WORLD);
				}
			// recv
			if (node_rank==0){
				MPI_Recv(buffer_recv,PARTITION_WIDTH,MPI_DOUBLE,k,2,MPI_COMM_WORLD,&status);
				memcpy(&(element_global[j][k*PARTITION_WIDTH]),buffer_recv,sizeof(double)*PARTITION_WIDTH);
				}	
			}
		}
	}

// finish, outputs
printf("bagiempat: node[%d]: computing time= %.6f sec\n",node_rank,total_processing_time);

if (node_rank==0){
	gettimeofday(&stop_time,NULL);
	printf("bagiempat: total steps= %d\n",step);
	//printf("bagiempat: finish = %d.%d\n",stop_time.tv_sec,stop_time.tv_usec);
	printf("bagiempat: elapsed= %.6f sec\n",stop_time.tv_sec-start_time.tv_sec+\
		(double)(stop_time.tv_usec-start_time.tv_usec)/1000000);
	
	//~ for (j=0; j<PARTITION_HEIGHT; j++){
		//~ for (i=0; i<PARTITION_WIDTH*2; i++){
			//~ printf("%f\t",element_global[j][i]);
			//~ }
			//~ printf("\n");
		//~ }
	}

	
MPI_globalize();
return(0);
}
