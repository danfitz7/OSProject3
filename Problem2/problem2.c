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

// TURNS
typedef enum {LEFT, RIGHT, STRAIGHT} turn_t;
turn_t random_turn(){
	return (turn_t)(rand()%2);
}
char* str_turn(turn_t t){
	switch(t){
		case LEFT:
			return "L";
			break;
		case RIGHT:
			return "R";
			break;
		case STRAIGHT:
			return "S";
			break;
		default:
			return "?";
			break;
	}
}

// DIRECTIONS
typedef enum {NORTH, SOUTH, EAST, WEST} direction_t;
direction_t random_direction(){
	return (direction_t)(rand()%3);
}
char* str_direction(direction_t d){
	switch(d){
		case NORTH:
			return "N";
			break;
		case SOUTH:
			return "S";
			break;
		case EAST:
			return "E";
			break;
		case WEST:
			return "W";
			break;
		default:
			return "?";
			break;
	}
}

// CARS
#define NUM_CARS 20
typedef unsigned short car_id;
typedef struct{
	direction_t direction;
	turn_t turn;
} car;
car cars[NUM_CARS];	// master array of cars
void print_car(car_id car){
	printf("C%2d coming from %s turning %s\n", car, str_direction(cars[car].direction), str_turn(cars[car].turn));
}

// ROAD LOGIC



// DEFINE QUEUES
typedef unsigned int queue_index;
#define QUEUE_CAPACITY NUM_CARS // should never need more than NUM_CARS
typedef struct {
	car_id car_ids[QUEUE_CAPACITY];
	queue_index head; 	// index of the element that is the head of the queue
	queue_index tail; 	// always the index circularly-after the last element in the queue. If this is equal to the head index, the queue is either full or empty
	unsigned int count;	// number of items in the queue
} car_queue;
// helper function to wrap around indexes in circular queue
void normalize_queue_index(unsigned int* i){
	while (*i >= QUEUE_CAPACITY){
		*i = *i-QUEUE_CAPACITY;
	}
	while (*i < 0){
		*i = *i+QUEUE_CAPACITY;
	}
}
void push_queue(car_queue* q, car_id id){
	queue_index last_index = (q->tail > 0)? q->tail : QUEUE_CAPACITY-1; // if the tail is at index 0, then it's wrapping around from the end of the array. Fill the last elt in the array.
	q->car_ids[last_index] = id;
	
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
car_id pop_queue(car_queue* q){
	car_id result = q->car_ids[q->head];
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
boolean queue_empty(car_queue* q){
	return (q->count == 0);
}
void print_car_queue(car_queue* q){
	printf("Car queue (head:%d, tail: %d):\n", q->head, q->tail);
	queue_index max = ((q->tail >= q->head)? q->tail-1 : QUEUE_CAPACITY-1);
	for (int i=q->head;i<=max;i++){
		printf("\tQueue index: %d\t", i);
		print_car(q->car_ids[i]);
		
		// if the queue circles around, have this loop also circle around
		if (i == max && max != q->tail-1){
			i=0;
			max = q->tail;
			printf("Circling to front of queue array.");
		}
	}
}

void setup(){
	/*// init Mutexes
	pthread_mutex_init(&pthread_mutex_clusters_lock, NULL);
	for (int i=0;i<3;i++){
		pthread_mutex_init(&(pthread_mutex_level_queues[i]), NULL);
	}
	for (int i=0;i<NUM_THREADS;i++){
		pthread_mutex_init(&(thread_mutexes[i]), NULL);
		pthread_mutex_lock(&(thread_mutexes[i]));
	}
	
	// init Condition Variables
	//pthread_cond_init(&pthread_cond_cluster_available, NULL);
	*/
}

void teardown(){
	/*
	// delete Condition Variables
	//pthread_cond_destroy(&pthread_cond_cluster_available);
	
	// destroy Mutexes
	pthread_mutex_destroy(&pthread_mutex_clusters_lock);
	for (int i=0;i<3;i++){
		pthread_mutex_destroy(&(pthread_mutex_level_queues[i]));
	}
	for (int i=0;i<NUM_THREADS;i++){
		pthread_mutex_destroy(&(thread_mutexes[i]));
	}
	*/
}

int main(int argc, const char* argv[]){
	setup();
	
	// Randomize the car directions
	for (int i=0;i<NUM_CARS;i++){
		cars[i].turn=random_turn();
		cars[i].direction=random_direction();
	}
	
	
	
	teardown();

	//pthread_exit(NULL);
}