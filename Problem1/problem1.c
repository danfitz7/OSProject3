#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>
#include <time.h>

// Helps to have booleans
#define boolean unsigned short
#define True 1
#define False 0

// also random numbers
#define random_range(min, max) min+rand()%(max-min)

// THREADS
#define NUM_THREADS 20
pthread_t threads[NUM_THREADS];					// global master array of threads
pthread_cond_t thread_conditions[NUM_THREADS]; 	// a condition variable (mutex) for every boy and girl and thread
pthread_mutex_t thread_mutexes[NUM_THREADS]; 	// a mutex for every thread

// TIMING
#define s_to_ms 1000000
#define MIN_THREAD_RUNTIME_MS (unsigned long)(0.25f*s_to_ms)
#define MAX_THREAD_RUNTIME_MS (unsigned long)(2*s_to_ms)
#define MIN_THREAD_DELAYTIME_MS (unsigned long)(1*s_to_ms)
#define MAX_THREAD_DELAYTIME_MS (unsigned long)(3*s_to_ms)
unsigned long thread_runTimes[NUM_THREADS];
unsigned long thread_delayTimes[NUM_THREADS];

// THREAD SECURITY LEVELS
// There are three security levels and corresponding security level numbers:
// 0: No job (IDLE)
// 1: Unclassified job (U)
// 2: Secret job (S)
// 3: Top Secret job (TS)
typedef enum {U, S, TS, IDLE} level;
char* str_level(level l){
	switch(l){
		case U:
			return "U";
			break;
		case S:
			return "S";
			break;
		case TS:
			return "T";
			break;
		case IDLE:
			return "I";
			break;
		default:
			return "N";
			break;
	}
}
level thread_levels[NUM_THREADS] = {U,U,U,U,U,U,U,U,TS,TS,TS,TS,TS,TS,S,S,S,S,S,S};
boolean compatible(level A, level B){
	return (A == IDLE || B == IDLE)
			|| ((A == S || A == TS) && (B == S || B == TS))
			|| (A == U && B == U);
}

// DEFINE QUEUES
typedef unsigned long thread_id;
#define QUEUE_CAPACITY NUM_THREADS // should never need more than NUM_THREADS
typedef struct {
	thread_id thread_ids[QUEUE_CAPACITY];//pthread_t* threads[NUM_THREADS];
	unsigned int head; // index of the element that is the head of the queue
	unsigned int tail; // always the index circularly-after the last element in the queue. If this is equal to the head index, the queue is either full or empty
	unsigned int count;// number of items in the queue
} pthread_queue;
// helper function to wrap around indexes in circular queue
void normalize_queue_index(unsigned int* i){
	while (*i >= QUEUE_CAPACITY){
		*i = *i-QUEUE_CAPACITY;
	}
	while (*i < 0){
		*i = *i+QUEUE_CAPACITY;
	}
}
void push_queue(pthread_queue* q, thread_id id){
	unsigned int last_index = (q->tail > 0)? q->tail : QUEUE_CAPACITY-1; // if the tail is at index 0, then it's wrapping around from the end of the array. Fill the last elt in the array.
	q->thread_ids[last_index] = id;
	
	q->tail++;
	normalize_queue_index(&(q->tail));
	if (q->tail == q->head){
		printf("WARNING: queue full: %d!\n", q->count);
	}
	
	q->count++;
	if (q->count > QUEUE_CAPACITY){
		printf("ERROR: queue count exceeds number of threads!\n");
		exit(1);
	}
}
thread_id pop_queue(pthread_queue* q){
	thread_id result = q->thread_ids[q->head];
	q->head++;
	normalize_queue_index(&(q->head));
	if (q->count == 0){
		printf("ERROR: popping from empty queue!\n");
		exit(1);
	}
	q->count--;
	if (q->tail == q->head){
		printf("\tWARNING: queue empty: %d!\n", q->count);
	}
	return result;
}
boolean queue_empty(pthread_queue* q){
	return (q->count == 0);
}
void print_pthread_queue(pthread_queue* q){
	printf("Thread queue (head:%d, tail: %d):\n", q->head, q->tail);
	unsigned int max = ((q->tail >= q->head)? q->tail-1 : QUEUE_CAPACITY-1);
	for (int i=q->head;i<=max;i++){
		printf("\tQueue index: %d\tThread ID:%lu\tLevel:%s\trunTime: %lu\tdelayTime: %lu\n", i, q->thread_ids[i], str_level(thread_levels[q->thread_ids[i]]), thread_runTimes[q->thread_ids[i]], thread_delayTimes[q->thread_ids[i]]);
		
		// if the queue circles around, have this loop also circle around
		if (i == max && max != q->tail-1){
			i=0;
			max = q->tail;
			printf("Circling to front of queue array.");
		}
	}
}

// THREAD QUEUES
pthread_queue S_queue = {.head=0, .tail=0, .count=0}; // queue of Secret jobs
pthread_queue TS_queue = {.head=0, .tail=0, .count=0};// queue of Top-Secret jobs
pthread_queue U_queue = {.head=0, .tail=0, .count=0}; // queue of Unclassified jobs
pthread_queue* level_queues[3] = {&U_queue, &S_queue, &TS_queue};
pthread_mutex_t pthread_mutex_level_queues;

// CLUSTER STATE
pthread_mutex_t pthread_mutex_clusters_lock; // locks the state of the clusters
level A_level = IDLE;
level B_level = IDLE;
pthread_cond_t pthread_cond_cluster_available; // signalled when one of the clusters becomes available
void get_next_available_cluster(level* idle_cluster){
	pthread_cond_wait(&pthread_cond_cluster_available, &pthread_mutex_clusters_lock); // wait for some thread signal that a cluster is open
	idle_cluster = NULL;
	pthread_mutex_lock(&pthread_mutex_clusters_lock);
	if (A_level == IDLE){
		idle_cluster = &A_level;
		if (B_level==IDLE){
			printf("huh, both idle.");
		}
	}
	if (B_level == IDLE){
		idle_cluster = &B_level;
	}
	if (idle_cluster == NULL){
		printf("ERROR: both clusters busy but idle signal sent!");
	}
	pthread_mutex_unlock(&pthread_mutex_clusters_lock);
}

// A generalized security-level job
#define thread_level thread_levels[this_id]
#define thread_runTime thread_runTimes[this_id]
#define thread_delayTime thread_delayTimes[this_id]
void push_thyself(thread_id this_id){
	printf("T%lu\tpushing to queue...\n", this_id);
	pthread_mutex_trylock(&pthread_mutex_level_queues);
	push_queue(level_queues[thread_level], this_id);
	pthread_mutex_unlock(&pthread_mutex_level_queues);
}
void wait_for_cluster(thread_id this_id){
	printf("T%lu\twaiting for cluster...\n", this_id);
	pthread_mutex_lock(&(thread_mutexes[this_id]));		// block while waiting for the cluster
	printf("T%lu\trunning in cluster...\n", this_id);
}
void exit_cluster(thread_id this_id){
	printf("T%lu\texiting cluster...\n", this_id);
	pthread_mutex_unlock(&(thread_mutexes[this_id]));
}
void* pthread_job(void* id){
	const unsigned long this_id = (long)id; // our thread id was given to us directly disguised as a void* argument
	
	printf("Thread %lu here!\n", this_id);
	
	while(1){ // main job loop
		wait_for_cluster(this_id);  // wait for the scheduler to release our mutex, indicating that we can (and are) running in one of the clusters
		usleep(thread_runTime); 	// do our job as much as we can
		exit_cluster(this_id);		// unlock our mutex to indicate to the scheduler that we've finished
		
		usleep(thread_delayTime);   // wait to start before en-queueing ourselves
		push_thyself(this_id);		// push ourselves again
	}
	
	/*// just for fun  - stress test queues
	for (int i=0;i<3;i++){
		printf("\tT%lu popped %d from queue.\n", this_id, pop_queue(&A_queue));
	
		usleep(thread_delayTime);
	
		push_queue(&A_queue, (thread_id)this_id);
		printf("\tT%lu pushed itself again.\n", this_id);
	
		usleep(thread_delayTime);
	}
	printf("\tT%lu popped %d from queue.\n", this_id, pop_queue(&A_queue));
	*/

	return (void*)0; // stop complaining
}

void* (*thread_procedures[3])(void*) = {pthread_job, pthread_job, pthread_job}; // array of thread procedures corresponding to security levels

void setup(){
	// init Mutexes
	pthread_mutex_init(&pthread_mutex_clusters_lock, NULL);
	pthread_mutex_init(&pthread_mutex_level_queues, NULL);
	for (int i=0;i<NUM_THREADS;i++){
		pthread_mutex_init(&(thread_mutexes[i]), NULL);
		pthread_mutex_lock(&(thread_mutexes[i]));
	}
	
	// init Condition Variables
	pthread_cond_init(&pthread_cond_cluster_available, NULL);
}

void teardown(){
	// delete Condition Variables
	pthread_cond_destroy(&pthread_cond_cluster_available);
	
	// destroy Mutexes
	pthread_mutex_destroy(&pthread_mutex_clusters_lock);
	pthread_mutex_destroy(&pthread_mutex_level_queues);
	for (int i=0;i<NUM_THREADS;i++){
		pthread_mutex_destroy(&(thread_mutexes[i]));
	}
}

void scheduler(){
	level idle_cluster = IDLE;
	while(1){
		// wait for a job to finish in a cluster
		get_next_available_cluster(&idle_cluster);
		
		// get the next job to run according to security logic
		unsigned long next_thread_to_run_id = 0;//get_next_thread_to_run();
		
		// put the new job in the cluster
		idle_cluster = thread_levels[next_thread_to_run_id];
	}
}

int main(int argc, const char* argv[]){
	setup();
	
	// Randomize the thread runtimes
	for (int i=0;i<NUM_THREADS;i++){
		thread_runTimes[i]=random_range(MIN_THREAD_RUNTIME_MS, MAX_THREAD_RUNTIME_MS);
		thread_delayTimes[i]=random_range(MIN_THREAD_DELAYTIME_MS, MAX_THREAD_DELAYTIME_MS);
	}
	
	// Create and run all our pthreads
	// https://computing.llnl.gov/tutorials/pthreads/#Example
	int pthread_create_return_code;
	for (long t=0;t<NUM_THREADS;t++){
		printf("\nMaking thread %lu\n", t);
		pthread_create_return_code = pthread_create(&threads[t], NULL, thread_procedures[thread_levels[t]], (void*)t);
		if (pthread_create_return_code){
			printf("ERROR; return code from pthread_create is %d\n", pthread_create_return_code);
		}
		push_thyself(t); // push the newly created thread on the queue
		//print_pthread_queue(&A_queue);
	}
	
	
	scheduler();
	
	
	teardown();

	pthread_exit(NULL);
}