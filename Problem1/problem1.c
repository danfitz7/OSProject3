#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

// Helps to have booleans
#define boolean unsigned short
#define True 1
#define False 0

// Semaphores as mutex-like locks (binary semaphores).
// "semutex" is a stand-in replacement everywhere for "pthread_mutex"
typedef sem_t semutex_t;
void semutex_init(semutex_t* semutex, boolean locked){
	sem_init(semutex, 0, (locked?0:1)); // 1 = locked, 0 = unlocked. see FIGURE: A SEMAPHORE AS A LOCK in http://pages.cs.wisc.edu/~remzi/Classes/537/Fall2008/Notes/threads-semaphores.txt
}
void semutex_destroy(semutex_t* semutex){
	sem_destroy(semutex);
}
void semutex_lock(semutex_t* semutex){
	sem_wait(semutex);
}
void semutex_unlock(semutex_t* semutex){
	sem_post(semutex);
}
int semutex_trylock(semutex_t* semutex){
	return sem_trywait(semutex);
}

// also random numbers
#define random_range(min, max) min+rand()%(max-min)

// THREADS
#define NUM_THREADS 20
pthread_t threads[NUM_THREADS];					// global master array of threads
//pthread_cond_t thread_conditions[NUM_THREADS];// a condition variable (mutex) for every boy and girl and thread
semutex_t thread_mutexes[NUM_THREADS]; 	// a mutex for every thread

// TIMING
#define s_to_ms 1000000
#define MIN_THREAD_RUNTIME_MS (unsigned long)(0.25f*s_to_ms)
#define MAX_THREAD_RUNTIME_MS (unsigned long)(2*s_to_ms)
#define MIN_THREAD_DELAYTIME_MS (unsigned long)(2*s_to_ms)
#define MAX_THREAD_DELAYTIME_MS (unsigned long)(4*s_to_ms)
unsigned long thread_runTimes[NUM_THREADS];
unsigned long thread_delayTimes[NUM_THREADS];

// THREAD SECURITY LEVELS
// There are three security levels and corresponding security level numbers:
// 0: No job (IDLE)
// 1: Unclassified job (U)
// 2: Secret job (S)
// 3: Top Secret job (TS)
typedef enum {U=0, S=1, TS=2, IDLE} level;
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
level thread_levels[NUM_THREADS] = {U,U,U,U,U,U,U,U,S,S,S,S,S,S,TS,TS,TS,TS,TS,TS};
/*boolean compatible(level A, level B){
	return (A == IDLE || B == IDLE)
			|| ((A == S || A == TS) && (B == S || B == TS))
			|| (A == U && B == U);
}*/

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
void print_queue_rows(pthread_queue* q){
	unsigned int max = ((q->tail >= q->head)? q->tail-1 : QUEUE_CAPACITY-1);
	for (int i=q->head;i<=max;i++){
		printf("\t%2lu", q->thread_ids[i]);
		
		// if the queue circles around, have this loop also circle around
		if (i == max && max != q->tail-1){
			i=0;
			max = q->tail;
			printf("Circling to front of queue array.");
		}
	}
	printf("\n");
}


// THREAD QUEUES
pthread_queue S_queue = {.head=0, .tail=0, .count=0}; // queue of Secret jobs
pthread_queue TS_queue = {.head=0, .tail=0, .count=0};// queue of Top-Secret jobs
pthread_queue U_queue = {.head=0, .tail=0, .count=0}; // queue of Unclassified jobs
pthread_queue* level_queues[3] = {&U_queue, &S_queue, &TS_queue};
semutex_t level_queues_mutex[3];
// flags
boolean pending_TS = False;
level pending_switchType = IDLE;

// CLUSTER STATE
typedef short cluster_index;
typedef enum {A=0, B=1, AB = 2, NONE = 3} cluster_activity;
//#define A (cluster_index)0
//#define B (cluster_index)1
semutex_t cluster_lock_mutex; 	// locks the state of cluster_jobs
thread_id cluster_jobs[2];						// the ids of the threads currently in each cluster
//cluster_activity current_clusters = 0;			// code for the status of both clusters (4 states), locked by cluster_lock_mutex
//pthread_cond_t pthread_cond_cluster_available; 	// signalled when one of the clusters becomes available
void print_clusters(){
	//semutex_lock(&cluster_lock_mutex);
	printf("\nA[%2lu](%s) B[%2lu](%s)\n", cluster_jobs[A], str_level(thread_levels[cluster_jobs[A]]),cluster_jobs[B], str_level(thread_levels[cluster_jobs[B]]));
	//semutex_unlock(&cluster_lock_mutex);
	
	print_queue_rows(&U_queue);
	print_queue_rows(&S_queue);
	print_queue_rows(&TS_queue);
}

// A generalized security-level job
#define thread_level thread_levels[this_id]
#define thread_runTime thread_runTimes[this_id]
#define thread_delayTime thread_delayTimes[this_id]
void push_thyself(thread_id this_id){
	printf("\tT%2lu(%s)\tPushing to queue for level %d...\n", this_id, str_level(thread_level), thread_level);
	semutex_lock(&level_queues_mutex[thread_level]);
	push_queue(level_queues[thread_level], this_id);
	semutex_unlock(&level_queues_mutex[thread_level]);
}
void wait_for_cluster(thread_id this_id){
	printf("\tT%2lu(%s)\tWaiting for cluster...\n", this_id, str_level(thread_level));
	semutex_lock(&(thread_mutexes[this_id]));
		//pthread_cond_wait(&(thread_conditions[this_id]), &(thread_mutexes[this_id]));		// block while waiting for a cluster. thread_mutexes[this_id] is unlocked while we block.
		// NOTE: thread_mutexes[this_id] is now locked.
	semutex_trylock(&cluster_lock_mutex);	// make sure someone has blocked this so the scheduler can detect it's unblocking
	printf("\tT%2lu(%s)\tRunning in cluster...\n", this_id, str_level(thread_level));
}
void exit_cluster(thread_id this_id){
	printf("\tT%2lu(%s)\tExiting cluster...\n", this_id, str_level(thread_level));
	semutex_unlock(&(thread_mutexes[this_id]));		// unlock our own mutex
	semutex_unlock(&cluster_lock_mutex);		// unlock the cluster lock which the scheduler is blocking on to see when a cluster becomes free
	//pthread_cond_signal(&pthread_cond_cluster_available);	// signal the scheduler that we've finished running
}
void* pthread_job(void* id){
	const unsigned long this_id = (long)id; // our thread id was given to us directly disguised as a void* argument
	push_thyself(this_id); 					// push ourselves on the appropriate queue

	printf("\tThread %lu ready and waiting!\n", this_id);
	
	while(1){ // main job loop
		wait_for_cluster(this_id);  // wait for the scheduler to release our mutex, indicating that we can (and are) running in one of the clusters
		usleep(thread_runTime); 	// do our job as much as we can
		exit_cluster(this_id);		// unlock our mutex to indicate to the scheduler that we've finished
		
		usleep(thread_delayTime);   // wait to start before en-queueing ourselves
		push_thyself(this_id);		// push ourselves again
	}
	
	return (void*)0; // stop complaining
}

void* (*thread_procedures[3])(void*) = {pthread_job, pthread_job, pthread_job}; // array of thread procedures corresponding to security levels

void setup(){
	// init Mutexes
	semutex_init(&cluster_lock_mutex, False);
	for (int i=0;i<3;i++){
		semutex_init(&(level_queues_mutex[i]), False);
	}
	for (int i=0;i<NUM_THREADS;i++){
		semutex_init(&(thread_mutexes[i]), True);	// lock all threads as they're made so they immediately block on their own mutex
		//semutex_lock(&(thread_mutexes[i]));			
	}
	
	// init Condition Variables
	//pthread_cond_init(&pthread_cond_cluster_available, NULL);
}

void teardown(){
	// delete Condition Variables
	//pthread_cond_destroy(&pthread_cond_cluster_available);
	
	// destroy Mutexes
	semutex_destroy(&cluster_lock_mutex);
	for (int i=0;i<3;i++){
		semutex_destroy(&(level_queues_mutex[i]));
	}
	for (int i=0;i<NUM_THREADS;i++){
		semutex_destroy(&(thread_mutexes[i]));
	}
}

cluster_index get_next_available_cluster_index(){
	printf("\tWaiting for next idle cluster...\n");
	semutex_lock(&cluster_lock_mutex); // block waiting for one of the two running processes to unlock this mutex
	//pthread_cond_wait(&pthread_cond_cluster_available, &cluster_lock_mutex); // wait for some thread signal that a cluster is open
	
	int A_trylock = semutex_trylock(&(thread_mutexes[cluster_jobs[A]]));
	int B_trylock = semutex_trylock(&(thread_mutexes[cluster_jobs[B]]));
	semutex_unlock(&cluster_lock_mutex);							// unlock so next running thread can lock	
	
	// which one unlocked?
	if (A_trylock == 0){
		return A;
	}
	if (B_trylock == 0){
		return B;
	}
	printf("ERROR: A cluster was freed but neither process is unlocked!\n");
	return -1;
}
void set_cluster(unsigned int i, thread_id id){
	semutex_lock(&cluster_lock_mutex);
	cluster_jobs[i] = id;					// remember that this thread is running in this thread
	semutex_unlock(&cluster_lock_mutex);
}
void run_thread_in_cluster(thread_id thread, cluster_index cluster){
		printf("\tRunning thread %lu (%s) in cluster %d...\n", thread, str_level(thread_levels[thread]), cluster);
		set_cluster(cluster, thread);
		semutex_unlock(&(thread_mutexes[thread]));// signal the thread to run by unlocking the mutex its waiting on
}

int cur_queue_index = U;				// index of the queue being popped to get the next thread to run
cluster_index next_free_cluster = -1;	// index of the cluster to put the next thread in
void wait_for_enqueue(){
	printf("WARNING: no jobs in any queue!\n");
	while(TS_queue.count<=0 && S_queue.count<=0 && U_queue.count<=0); // wait for some job to enter some queue
}
thread_id get_next_thread_to_run(){
	
	// if there are more then 3 TS jobs in the TS queue, we must run at-least two of them immediately.
	// Run one now because there are 3 or more 3 TS jobs, and set the pending_TS flag so we run one next time as well.
	if (TS_queue.count >=3 || pending_TS==True){
		printf("\t\tMore than 3 TS jobs or pending TS\n");
		if (pending_TS){		// if we're here because the flag was set
			pending_TS = False; // unset the flag
		}else if (TS_queue.count == 3){ // if the count was exactly three (it will be two after we pop this thread)
			pending_TS = True;			// set the flag so we pop again next time, even though it's not >=3
		}
		cur_queue_index = TS;
		return pop_queue(&TS_queue);
	}
	
	// normal compatibility logic
	if (cur_queue_index == S || cur_queue_index == TS){ 	// if we're running S or TS 
		if (S_queue.count>0){								// and there is at-least one S job in the S queue
			cur_queue_index = S;							// prepare to run S
		}else if (TS_queue.count>0){						// if we're out of S, but there's still some TS,
			cur_queue_index = TS;							// prepare tor un TS
		}else{							// if there's no more TS or S
			if (U_queue.count>0){		// but there is U
				pending_switchType = U;	// then we must switch security types.
			}else{
				wait_for_enqueue();
				return get_next_thread_to_run();
			}
		}
	}else{	// if we're running U
		if (U_queue.count>0){
			cur_queue_index = U;	// there's no more S jobs
		}else{
			if (S_queue.count>0){
				pending_switchType = S;
			}else if (TS_queue.count>0){
				pending_switchType = TS;
			}else{
				wait_for_enqueue();
				return get_next_thread_to_run();
			}
		}
	}
	
	// job type ballancing logic
	// Compare the number f jobs in each security level queue. If there are too many of one compatibility type, switch to that type. 
	if (pending_switchType != IDLE){ // if we're not already planning on switching security type
		if ((cur_queue_index == S || cur_queue_index == TS) && (U_queue.count > 2*(TS_queue.count + S_queue.count))){ // to many unclassified
			pending_switchType = U;
		}else if (cur_queue_index == U && ((TS_queue.count + S_queue.count) > U_queue.count)){ // too many secret
			pending_switchType = S;
		}
	}
	
	// job type switching logic
	// If we are trying to switch type because too many of one compatibility type have piled up in their queue(s)...
	if (pending_switchType != IDLE){
		cur_queue_index = pending_switchType;   // switch the currently running security type
		pending_switchType=IDLE;				// reset flag
		
		// wait for jobs in both clusters to finish, don't start any new ones until they do.
		//get_next_available_cluster_index();
		cluster_index remaining_cluster_index = 1-next_free_cluster; // get the index of the clustert that's still busy (not idle)
		thread_id other_cluster_tread_id = cluster_jobs[remaining_cluster_index];
		printf("Switching type when job %lu in cluster %d finishes...\n", cluster_jobs[remaining_cluster_index], remaining_cluster_index);
		semutex_lock(&(thread_mutexes[other_cluster_tread_id]));								// wait for that cluster to become idle
		run_thread_in_cluster(pop_queue(level_queues[cur_queue_index]), remaining_cluster_index); 	// start a job in the most recently idle cluster. When get_next_thread_to_run returns, the main scheduler will start another job of the same type in the original idle cluster.
	}
	
	return pop_queue(level_queues[cur_queue_index]);
}
void scheduler(){
	printf("\nStarting Scheduler...\n");
	
	while(1){
		// wait for a job to finish in a cluster
		next_free_cluster = get_next_available_cluster_index();
		
		// get the next job to run according to security logic
		thread_id next_thread_to_run = get_next_thread_to_run();
		
		// put the new job in the cluster
		run_thread_in_cluster(next_thread_to_run, next_free_cluster);
		
		print_clusters();
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
	}
	
	// run the first two threads
	semutex_lock(&cluster_lock_mutex);
	run_thread_in_cluster(get_next_thread_to_run(), A);	
	run_thread_in_cluster(get_next_thread_to_run(), B);
	
	// main scheduling loop
	scheduler();
	
	teardown();

	pthread_exit(NULL);
}