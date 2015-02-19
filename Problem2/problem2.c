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

// TURNS
typedef enum {LEFT, RIGHT, STRAIGHT} turn_direction;
turn_direction random_turn(){
	return (turn_direction)(rand()%2);
}
char* str_turn(turn_direction t){
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
typedef enum {EAST=0, SOUTH=1, WEST=2, NORTH=3} approach_direction;
approach_direction random_direction(){
	return (approach_direction)(rand()%3);
}
char* str_direction(approach_direction d){
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

// TIME
typedef unsigned int time_ms;
#define s_2_ms 1000000
#define MIN_SPEED_TIME 0.25*s_2_ms
#define MAX_SPEED_TIME 2*s_2_ms
#define MIN_RECYCLE_TIME 2*s_2_ms
#define MAX_RECYCLE_TIME 5*s_2_ms
time_ms random_time(unsigned int min, unsigned int max){
	return (time_ms)(min + rand()%(max - min));
}
time_ms random_recycle_time(){
	return random_time(MIN_RECYCLE_TIME, MAX_RECYCLE_TIME);
}
time_ms random_speed_time(){
	return random_time(MIN_SPEED_TIME, MAX_SPEED_TIME-MIN_SPEED_TIME);
}

// QUADRANTS
pthread_mutex_t intersection_lock_mutex;
typedef enum {NE=0, SE=1, SW=2, NW=3, UNUSED} quadrant_t;
typedef enum {OCCUPIED, UNOCCUPIED} quadrant_state_t;
quadrant_state_t quadrant_states[4];

// rotate a three-element array of quadrants this many times (all rotations clockwise)
void rotate_quadrants(quadrant_t quads[], int n){
	for (int i=0;i<n;i++){			// rotate this many times
		for (int q=0;q<3;q++){		// loop through each required quad
			if (quads[q] != UNUSED){	// if the quad is NULL (not required), we don't care
				// circularly rotate
				quads[q]++;
				if (quads[q] >= 4){
					quads[q] = 0;
				}
			}
		}
	}
}

// fill a three-element array with the quadrants this car needs to cross to get through the intersection
void get_quadrant_list(quadrant_t quads[4], approach_direction dir, turn_direction turn){
	quads[0] = NW;
	quads[1] = (turn == RIGHT)? UNUSED :SW;
	quads[3] = (turn == LEFT)? SE : UNUSED;
	
	rotate_quadrants(quads, dir);
}


// CARS
#define NUM_CARS 20
typedef unsigned int car_id;
typedef struct{
	approach_direction direction;
	turn_direction turn;
	time_ms recycle_time;
	time_ms speed_time;

	quadrant_t required_quadrants[3];
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t greenlight;
} car;
car cars[NUM_CARS];	// master array of cars
void recycle(car* c){
	c->turn = random_turn();
	c->direction = random_direction();
	c->recycle_time = random_recycle_time();
	c->speed_time = random_speed_time();
	get_quadrant_list(c->required_quadrants, c->direction, c->turn);
}
void print_car(car_id car){
	printf("C%2d coming from %s turning %s\n", car, str_direction(cars[car].direction), str_turn(cars[car].turn));
}

// QUEUES
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

// INTERSECTION QUEUES
car_queue approaching_queues[4]; // Cars line up on the roads to our intersection from the cardinal directions
//emergency_vehicle_queues[4];	 // emergency vehicles line up in these special queues (representative of bypassing every car in the corresponding regular car queue)
pthread_mutex_t cars_queues_lock_mutex[4]; // controls access to the respective car queues

/*void circularShift(boolean[4] a, int n){
	for (int s=0;s<n;s++){
		boolean end = a[3];
		for (int i=3;i>0;i--){
			a[i]-a[i-1];
		}
		a[0]=end;
	}
}
get_required_quadrants(car* c){
	// for NORTH
	quadrants[NW] = True;
	quadrants[NE] = False;
	quadrants[SE] = (car.turn == LEFT || car.turn == STRAIGHT);
	quadrants[SW] = (car.turn == STRAIGHT);
	
	// rotate by approach direction
	circularShift(*car.required_quadrants, (int)(car.direction-NORTH));
}*/

boolean can_cross(car* c){
	for (int i=0;i<3;i++){
		if (c->required_quadrants[i] && quadrant_states[c->required_quadrants[i]]){ // If a car would need this quadrant to cross but the quadrant is occupied, then it cannot cross
			return False;
		}
	}
	return True;
}
void set_quadrant(quadrant_t quadrant, quadrant_state_t quad_state){
	pthread_mutex_lock(&intersection_lock_mutex);
	quadrant_states[quadrant] = quad_state;
	pthread_mutex_unlock(&intersection_lock_mutex);
}
void enter_quadrant(quadrant_t quad){
	set_quadrant(quad, True);
}
void leave_quadrant(quadrant_t quad){
	set_quadrant(quad, False);
}

// CAR LOGIC
#define this cars[id]
void enqueue(car_id id){
	pthread_mutex_lock(&cars_queues_lock_mutex[cars[id].direction]);
	push_queue(&(approaching_queues[cars[id].direction]), id);
	pthread_mutex_unlock(&cars_queues_lock_mutex[cars[id].direction]);
}

void go(car* c){
	for (int i=0;i<3;i++){ // loop through the car's required quads
		if (c->required_quadrants[i] != UNUSED){		// if we actually need this quadrant
			enter_quadrant(c->required_quadrants[i]);	// enter
			usleep(c->speed_time);						// spend some time there, enjoy the sun
			leave_quadrant(c->required_quadrants[i]);	// move along
		}else{
			break;
		}
	}
}

// main car thread code
void* car_procedure(void* void_id){
	const car_id id = (car_id)void_id;
	
	while(1){
		usleep(this.recycle_time); // sleep a bit
		enqueue(id);			 // enqueue ourselves
		
		pthread_mutex_lock(&(this.mutex));				// lock our mutex
		pthread_cond_wait(&(this.greenlight), &(this.mutex)); // wait to go
		go(&this);			// go
		recycle(&this);		// do it all again!
	}
}

pthread_mutex_t waiting_car_mutexes[4];
void crossguard(){
	//car* first_cars_in_line[4];
	
	while(1){
		
	}
}

void setup(){
	for (int i=0;i<4;i++){
		pthread_mutex_init(&cars_queues_lock_mutex[i], NULL);
	}
	pthread_mutex_init(&intersection_lock_mutex, NULL);
	
	for (int i=0;i<NUM_CARS;i++){
		pthread_mutex_init(&(cars[i].mutex), NULL);
		pthread_mutex_lock(&(cars[i].mutex));
		pthread_cond_init(&(cars[i].greenlight), NULL);
	}
}

void teardown(){
	for (int i=0;i<4;i++){
		pthread_mutex_destroy(&cars_queues_lock_mutex[i]);
	}
	pthread_mutex_destroy(&intersection_lock_mutex);

	for (int i=0;i<NUM_CARS;i++){
		pthread_mutex_destroy(&(cars[i].mutex));
		pthread_cond_destroy(&(cars[i].greenlight));
	}
}

int main(int argc, const char* argv[]){
	setup();
	
	// Randomize the car directions
	int pthread_create_return_code;
	for (car_id c=0;c<NUM_CARS;c++){
		printf("\nMaking car %d\n", c);
		recycle(&(cars[c])); 	// init the car
		pthread_create_return_code = pthread_create(&(cars[c].thread), NULL, car_procedure, (void*)(unsigned long)c);
		if (pthread_create_return_code){
			printf("ERROR; Return code from pthread_create is %d\n", pthread_create_return_code);
		}
	}
	
	crossguard();
	
	teardown();

	pthread_exit(NULL);
}