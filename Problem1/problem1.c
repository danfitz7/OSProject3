#include <pthread.h>
#include <stdio.h>
#include <random.h>

/*
When any job leaves a cluster half, we broadcast on the availability channel of the job running on the other cluster*

We assume the pThreads scheduler under the hood is running every thread in pseudo-parallelism, unpredictably, but according to some specified protocol (Round-Robin).
Our "virtual scheduling algorithm" arises from the threads checking when they can run their actual "government computation" (simulated by sleeping)

Every thread waits in what amounts to a TryLock blocking loop until the conditions of the above global state variables allow it to run it's computation:

TS_check_cluster(){
	lock global variables
	check  if either cluster is idle (no job)
	flag success=0
	if one is idle:
		check compatibility with the job running in the other cluster (S, TS, or IDLE)
		if compatible
			set that clusters level to ours - we are going to be running in that cluster
			if (num_TS==3){
				pending_TS = TRUE
			else if (pending_TS == TRUE){
				pending_TS = FALSE
			}
			decrement the num_TS - one less TS process is waiting to run
			success=1
	unlock global variables
	return success;
}

TS_run(){
	while (!TS_check_cluster()

	usleep()
	
	TS_exit()
	
	recycle()
}

TS_exit(){
	lock global vars
	set this cluster's level to IDLE
	unlock global vars
	broadcast on the security level of the job of the other processor*
}
	
	
The other thread levels operate similarly, but with different compatibilities.
The S and U jobs must also check if num_TS>2, to let TS jobs run if there are more than 2 of them.

*
To ensure that TS and S jobs don't just keep swapping in and out we check if the number of U job in the queue is more then some threshold proportion larger than the number of T and TS jobs. 
If it is, and there aren't more than 2 TS jobs waiting to run, then we don't broadcast when exiting, and when the job on the other cluster finishes, it broadcasts on the U level so that some U jobs can run.
The reverse case happens for U jobs to allow T ans ST jobs to run.
We don't have to worry about TS jobs waiting forever, since we've already built in mechanisms that force TS jobs to run when there are more than 2 of them.
*/

// Helps to have booleans
#define boolean unsigned short
#define True 1
#define False 0

#define NUM_THREADS 20
pthread_t threads[NUM_THREADS];
#define MAX_THREAD_RUNTIME_MS 1000 
unsigned int thread_runtimes[NUM_THREADS];

// There are three security levels and corresponding security level numbers:
// 0: No job (IDLE)
// 1: Unclassified job (U)
// 2: Secret job (S)
// 3: Top Secret job (TS)
typedef enum {U, S, TS, IDLE} level_t;
LEVEL thread_levels[NUM_THREADS] = {U,U,U,U,U,U,U,U, TS,TS,TS,TS,TS,TS,TS,TS, S,S,S,S,S,S};
boolean compatible(LEVEL A, LEVEL B){
	return (A == IDLE || B == IDLE)
			|| ((A == S || A == TS) && (B == S || B == TS))
			|| (A == U && B == U);
}

// The security levels of the jobs running in each half of the cluster (half A and half B)
LEVEL cluster_A_level = IDLE; pthread_mutex_t pthread_mutex_cluster_A_level;
LEVEL cluster_B_level = IDLE; pthread_mutex_t pthread_mutex_cluster_B_level;
//LEVEL* cluster_levels[] = {IDLE, IDLE}/*{&clusterA_level, clusterB_level}*/; pthread_mutex_t pthread_mutex_cluster_levels;

// The number of each LEVEL of jobs in the queue
unsigned int num_U = 0; pthread_mutex_t pthread_mutex_num_U;
unsigned int num_S = 0; pthread_mutex_t pthread_mutex_num_S;
unsigned int num_TS = 0; pthread_mutex_t pthread_mutex_num_TS;

// Condition variables used for messaging (broadcasting) to jobs
pthread_cond_t pthread_cond_available_U = PTHREAD_COND_INITIALIZER; // pthread_mutex_t pthread_mutex_available_U;
pthread-cond_t pthread_cond_available_S = PTHREAD_COND_INITIALIZER; // pthread_mutex_t pthread_mutex_available_S;

// Flag for sceduling two TS jobs in a row when there are only 3 in the wueue
boolean pending_TS = False; pthread_mutex_t pthread_mutex_pending_TS;

void run_thread_job(){
	usleep(1);
}

// A generalized security-level job
void* pthread_job(void* id){
	const long thread_id = (long)thread_id;
	const LEVEL thread_level = thread_levels[id];
	const unsigned int thread_runtime = thread_runtimes[thread_id];
	
	LEVEL busy_cluster_level = IDLE;
	LEVEL* idle_cluster = NULL;
	pthread_mutex_lock(pthread_mutex_cluster_levels);
		
		// get the next idle cluster and the security level of the busy cluster
		while (idle_cluster == NULL){
			if (cluster_levels[0] == IDLE){
				idle_cluster = &cluster_A_level;
				idle_cluster_index = cluster_B_level;
			}else if(cluster_levels[1] == IDLE){
				idle_cluster = &cluster_B_level;
				idle_cluster_index = cluster_A_level;
			}
		}
		
		// check security compatibility with the job running on the busy cluster
		if (compatible(thread_level, busy_cluster_level){
			// we are going into this cluster
			*idle_cluster = thread_level;

			// TODO: logic for letting threads of an incompatible type run if there is a disproportionate number of them waiting in the queue
			
			// if there are more than three Top Secret threads waiting...
			if (num_TS > = 3){
				if (thread_level == 3){ // ...and we're one of them, then we should run
					if (num_TS == 3){
						pthread_mutex_lock(&pthread_mutex_pending_TS);
							pthread_mutex_pending_TS = True;
						pthread_mutex_unlock(&pthread_mutex_pending_TS);
					}
				}
			}
			
			pthread_mutex_lock(&pthread_mutex_pending_TS);
				if (thread_level == TS && pthread_mutex_pending_TS == True){
					pthread_mutex_pending_TS = False;
				}
			pthread_mutex_unlock(&pthread_mutex_pending_TS);

			run_thread_job();
		}
		
	pthread_mutex_unlock(&pthread_mutex_cluster_levels);	
}

void setup(){
	// init Mutexes
	pthread_mutex_destroy(&pthread_mutex_cluster_levels, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_U, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_S, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_TS, NULL);
	pthread_mutex_destroy(&pthread_mutex_pending_TS, NULL);
	
	// init Condition Variables
	pthread_cond_destroy(&pthread_cond_available_U, NULL);
	pthread_cond_destroy(&pthread_cond_available_S, NULL);
}

void teardown(){
	// delete Condition Variables
	pthread_cond_destroy(&pthread_cond_available_U);
	pthread_cond_destroy(&pthread_cond_available_S);
	
	// destroy Mutexes
	pthread_mutex_destroy(&pthread_mutex_cluster_levels, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_U, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_S, NULL);
	pthread_mutex_destroy(&pthread_mutex_num_TS, NULL);
	pthread_mutex_destroy(&pthread_mutex_pending_TS, NULL);
}

int main(int argc, const char* argv[]){
	setup();
	
	// Randomize the thread runtimes
	for (int i=0;i<NUM_THREADS;i++{
		thread_runtimes[i]=rand()%MAX_THREAD_RUNTIME_MS;
	}
	
	// Create and run all our pthreads
	// https://computing.llnl.gov/tutorials/pthreads/#Example
	int pthread_create_return_code;
	for (long t=0;t<NUM_THREADS;t++){
		pthread_create_return_code = pthread_create(&threads[t], NULL, thread_procedure[thread_levels[i]], (void*)t)
		if (rc){
			printf(""ERROR; return code from pthread_create is %d\n", pthread_create_return_code);
		}
	}
	
	teardown();

	pthread_exit(NULL);
}