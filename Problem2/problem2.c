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

// TURNS
typedef enum {LEFT, RIGHT, STRAIGHT} turn_direction;
turn_direction random_turn(){
	return (turn_direction)(rand()%3);
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
			return "T"; // NOTE: Straight= 'T" to not confuse with 'S' for South
			break;
		default:
			return "?";
			break;
	}
}

// DIRECTIONS
typedef enum {EAST=0, SOUTH=1, WEST=2, NORTH=3} approach_direction;
approach_direction random_direction(){
	return (approach_direction)(rand()%4);
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
//#define MIN_SPEED_TIME 0.25*s_2_ms
//#define MAX_SPEED_TIME 2*s_2_ms
#define INTERSECTION_CROSS_TIME 1*s_2_ms
#define MIN_RECYCLE_TIME 2*s_2_ms
#define MAX_RECYCLE_TIME 5*s_2_ms
time_ms random_time(unsigned int min, unsigned int max){
	return (time_ms)(min + rand()%(max - min));
}
time_ms random_recycle_time(){
	return random_time(MIN_RECYCLE_TIME, MAX_RECYCLE_TIME);
}
/*time_ms random_speed_time(){
	return random_time(MIN_SPEED_TIME, MAX_SPEED_TIME-MIN_SPEED_TIME);
}*/

// QUADRANTS
semutex_t intersection_lock_mutex;
typedef enum {NE=0, SE=1, SW=2, NW=3, UNUSED} quadrant_t;
typedef enum {OCCUPIED, UNOCCUPIED} quadrant_state_t;
quadrant_state_t quadrant_states[4] = {UNOCCUPIED, UNOCCUPIED, UNOCCUPIED, UNOCCUPIED};
char* str_quardant_state(quadrant_state_t s){
	switch(s){
		case OCCUPIED:
			return "O";
			break;
		case UNOCCUPIED:
			return "U";
			break;
		default:
			return "?";
			break;
	}
}
char* str_quadrant(quadrant_t q){
	switch(q){
		case NE:
			return "NE";
			break;
		case SE:
			return "SE";
			break;
		case SW:
			return "SW";
			break;
		case NW:
			return "NW";
			break;
		case UNUSED:
			return "UNUSED";
			break;
		default:
			return "??";
			break;
	}
}
void print_quadrant_statuses(){
	printf(" __\n|%s%s|\n|%s%s|\n ^^", str_quardant_state(quadrant_states[NW]), str_quardant_state(quadrant_states[NE]), str_quardant_state(quadrant_states[SW]), str_quardant_state(quadrant_states[SE]));
}
quadrant_t rotated_quadrant(quadrant_t quad, int n){
	if (n<0 || n>=4){
		printf("WARNING: Rotating by invalid number!\n");
	}
	if (quad==UNUSED){
		return UNUSED;
	}
	for (int i=0;i<n;i++){	// rotate this many times
		quad++;
		while (quad>=4){
			quad-=4;
		}
	}
	return quad;
}
#define N_REQUIRED_QUADRANTS 3
// rotate a three-element array of quadrants this many times (all rotations clockwise)
void rotate_quadrants(quadrant_t quads[N_REQUIRED_QUADRANTS], int n){
	for (int q=0;q<N_REQUIRED_QUADRANTS;q++){		// loop through each required quad
		quads[q] = rotated_quadrant(quads[q], n);
	}
}
// fill a three-element array with the quadrants this car needs to cross to get through the intersection
void get_quadrant_list(quadrant_t quads[N_REQUIRED_QUADRANTS], approach_direction dir, turn_direction turn){
	// Assume approaching form the EAST
	quads[0] = NE;							// enter on the NorthEast Quad
	quads[1] = (turn == RIGHT)? UNUSED :NW; // if we're turning riht, we never need another quad. Otherwise, we'll always use the NorthWest Quad
	quads[2] = (turn == LEFT)? SW : UNUSED;	// if we're going straight (East to West), then we only needed the NorthEast and NorthWest Quads. If we're turning left to go North, we'll also need the SouthWest quad
	
	rotate_quadrants(quads, dir);
}
void print_quadrant_list(quadrant_t quads[N_REQUIRED_QUADRANTS]){
	printf("Q{");
	for (int i=0;i<N_REQUIRED_QUADRANTS;i++){
		if (quads[i]!=UNUSED){
			printf("%s,", str_quadrant(quads[i]));
		}else{
			break;
		}
	}
	printf("}");
}

// CARS
#define NUM_CARS 20
typedef int car_id;
typedef enum {REGULAR, EMERGENCY, MOTORCADE} car_type_t;
typedef struct{
	approach_direction direction;
	turn_direction turn;
	time_ms recycle_time;
//	time_ms speed_time;
	car_type_t type;
	quadrant_t required_quadrants[N_REQUIRED_QUADRANTS];
	pthread_t thread;
	car_id id;
	semutex_t mutex;
//	pthread_cond_t greenlight;
} car;
car cars[NUM_CARS];	// master array of cars
void print_car(car_id car){
	printf("'%s%2d coming from %s turning %s'", ((cars[car].type==REGULAR)?"C":"E"), car, str_direction(cars[car].direction), str_turn(cars[car].turn));
}
void recycle(car_id id){
	printf("\tCar %d is recycling to: ", id);
	
	cars[id].turn = random_turn();
	cars[id].direction = random_direction();
	cars[id].recycle_time = random_recycle_time();
	cars[id].type = REGULAR; // TODO: make emergency vehcles
	//cars[id].speed_time = random_speed_time();
	get_quadrant_list(cars[id].required_quadrants, cars[id].direction, cars[id].turn);
	
	print_car(id);
	printf(" occupying quadrants ");
	print_quadrant_list(cars[id].required_quadrants);
	printf("\n");
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
	q->car_ids[q->tail] = id;
	q->tail++;
	normalize_queue_index(&(q->tail));
	
	if ((q->count == QUEUE_CAPACITY)){
		printf("WARNING: queue full: %d!\n", q->count);
	}
	if (q->tail == q->head){
		printf("WARNING: queue full or empty!");
	}
	
	q->count++;
	if (q->count > QUEUE_CAPACITY){
		printf("ERROR: queue count %d exceeds capacity!\n", q->count);
		exit(1);
	}
}
boolean queue_empty(car_queue* q){
	return (q->count == 0 || q->head == q->tail);
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
	
	if (queue_empty(q)){
		printf("WARNING: queue empty: %d!\n", q->count);
	}
	
	if (q->tail == q->head){
		printf("WARNING: queue full or empty!");
	}
	
	return result;
}
car_id peek_queue(car_queue* q){
	if (queue_empty(q)){
		return -1;
	}else{
		return q->car_ids[q->head];
	}
}

/*void print_car_queue(car_queue* q){
	printf("Car queue (head:%d, tail: %d):\n", q->head, q->tail);
	queue_index max = ((q->tail >= q->head)? q->tail-1 : QUEUE_CAPACITY-1);
	for (int i=q->head;i<=max;i++){
		printf("\tQueue index: %d\t", i);
		print_car(q->car_ids[i]);
		printf("\n");
		// if the queue circles around, have this loop also circle around
		if (i == max && max != q->tail-1){
			i=0;
			max = q->tail;
			printf("Circling to front of queue array.");
		}
	}
}*/
void print_car_queue(car_queue* q){
	printf("Q(%2d){",q->count);
	
	boolean queue_loops_around = (q->tail < q->head);
	queue_index max = (queue_loops_around? QUEUE_CAPACITY : q->tail);
	for (int i=q->head;i<max;i++){
		printf("[%d|%s|%s], ", cars[q->car_ids[i]].id, str_direction(cars[q->car_ids[i]].direction), str_turn(cars[q->car_ids[i]].turn));
		
		
		if (i == max && queue_loops_around){
			i=0;
			max=q->tail;
		}
	}
	printf("}");
}

// INTERSECTION QUEUES
car_queue approaching_queues[4]; // Cars line up on the roads to our intersection from the cardinal directions
car_queue emergency_vehicle_queues[4];	 // emergency vehicles line up in these special queues (representative of bypassing every car in the corresponding regular car queue)
semutex_t cars_queues_lock_mutex[4]; // controls access to the respective car queues

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

boolean can_cross(car_id id){
	for (int i=0;i<3;i++){
		if (cars[id].required_quadrants[i]==UNUSED){
			break;
		}
		if (quadrant_states[cars[id].required_quadrants[i]]==OCCUPIED){ // If a car would need this quadrant to cross but the quadrant is occupied, then it cannot cross
			return False;
		}
	}
	return True;
}

/*void set_quadrant(quadrant_t quadrant, quadrant_state_t quad_state){
	semutex_lock(&intersection_lock_mutex);
	quadrant_states[quadrant] = quad_state;
	semutex_unlock(&intersection_lock_mutex);
}
void enter_quadrant(quadrant_t quad){
	set_quadrant(quad, True);
}
void leave_quadrant(quadrant_t quad){
	set_quadrant(quad, False);
}*/

// CAR LOGIC
#define this cars[id]
void enqueue(car_id id){
	approach_direction dir = cars[id].direction;
	car_queue* queue = &(approaching_queues[dir]);
	
	printf("\tEn-queueing ");
	print_car(id);
	printf(" in %s: ", str_direction(dir));
		
	semutex_lock(&cars_queues_lock_mutex[dir]);
	if (cars[id].type == REGULAR){
		push_queue(queue, id);
	}else{
		push_queue(queue, id);
	}
	semutex_unlock(&cars_queues_lock_mutex[dir]);
	
	print_car_queue(queue);
	printf("\n");
}

void go(car_id id){
	printf("\t\t\t\t");
	print_car(id);
	printf(" is going to occupy quadrants ");
	print_quadrant_list(cars[id].required_quadrants);
	printf("\n");
	
	for (int i=0;i<3;i++){ // loop through the car's required quads
		if (cars[id].required_quadrants[i] != UNUSED){		// if we actually need this quadrant
			quadrant_states[cars[id].required_quadrants[i]] = OCCUPIED;
			
			/*enter_quadrant(c->required_quadrants[i]);	// enter
			usleep(c->speed_time);						// spend some time there, enjoy the sun
			leave_quadrant(c->required_quadrants[i]);	// move along
			*/
		}else{
			break;
		}
	}

}

// main car thread code
void* car_procedure(void* void_id){
	printf("\n\tCar %d created!\n", (int)void_id);
	const car_id id = (car_id)void_id;
	
	/*
	for (int i=0;i<N_REQUIRED_QUADRANTS;i++){
		this.required_quadrants[i]=UNUSED;
	}*/
	
	while(1){
		enqueue(id);			 // enqueue ourselves
		semutex_lock(&(this.mutex));				// wait to go
		//pthread_cond_wait(&(this.greenlight), &(this.mutex)); // wait to go
		usleep(INTERSECTION_CROSS_TIME);	//go(&this);			// go
		recycle(this.id);		// do it all again!
		usleep(this.recycle_time); // sleep a bit
	}
}

approach_direction abs_quad(approach_direction base, int offset){
	base+=offset;
	while (base>=4){
		base-=4;
	}
	return base;
}

void crossguard(){
	//car* first_cars_in_line[4];
	
	approach_direction cur_priority = EAST;
	unsigned long loop = 0;
	while(1){
		printf("\n\nCROSSGUARD %lu prioritizing cars approaching from the %s\n", loop, str_direction(cur_priority));
		loop++;

		for (int d = 0;d<4;d++){
			approach_direction abs_cur_dir = abs_quad(cur_priority, d);
			printf("\tChecking relative apprach direction #%d which is absolute direction %s\n", d,str_direction(abs_cur_dir));
			car_queue* queue = &(approaching_queues[abs_cur_dir]);
			
			printf("\tChecking approach direction queue %s: ", str_direction(abs_cur_dir));
			print_car_queue(queue);
			printf("\n");
			
			if (!queue_empty(queue)){
				car_id car = peek_queue(queue);
				
				printf("\t\tExamining car: ");
				print_car(car);
				printf("\n");
				
				if (can_cross(car)){
					car = pop_queue(queue);
					printf("\t\t\t...which can turn!\n");//, car, str_turn(cars[car].turn), str_direction(abs_cur_dir));
					go(car);
					semutex_unlock(&(cars[car].mutex)); // tell the car to go!
				}else{
					printf("\t\t\t... which can't cross because it requires quadrants:");
					print_quadrant_list(cars[car].required_quadrants);
					printf("\n");
				}
			}else{
				printf("\t\tNo car in queue %s\n", str_direction(abs_cur_dir));
			}
			print_quadrant_statuses();
		}
		
		// circularly loop through directions
		cur_priority++;
		if (cur_priority >=4){
			cur_priority=0;
		}
		
		usleep(INTERSECTION_CROSS_TIME); // let cars go
		// clear the intersection for next time
		for (int i=0;i<4;i++){
			quadrant_states[i]=UNOCCUPIED;
		}
	}
}

void setup(){
	//init Queues
	for (int i=0;i<4;i++){
		approaching_queues[i].head=0;
		approaching_queues[i].tail=0;
		approaching_queues[i].count=0;
		emergency_vehicle_queues[i].head=0;
		emergency_vehicle_queues[i].tail=0;
		emergency_vehicle_queues[i].count=0;
	}
	
	
	for (int i=0;i<4;i++){
		semutex_init(&cars_queues_lock_mutex[i], False);
	}
	semutex_init(&intersection_lock_mutex, False);
	
	for (int i=0;i<NUM_CARS;i++){
		semutex_init(&(cars[i].mutex), True);
		//semutex_lock(&(cars[i].mutex));
		//pthread_cond_init(&(cars[i].greenlight), NULL);
	}
}

void teardown(){
	for (int i=0;i<4;i++){
		semutex_destroy(&cars_queues_lock_mutex[i]);
	}
	semutex_destroy(&intersection_lock_mutex);

	for (int i=0;i<NUM_CARS;i++){
		semutex_destroy(&(cars[i].mutex));
		//pthread_cond_destroy(&(cars[i].greenlight));
	}
}

int main(int argc, const char* argv[]){
	setup();
	
	// Randomize the car directions
	int pthread_create_return_code;
	for (car_id c=0;c<NUM_CARS;c++){
		printf("\nMaking car %d\n", c);
		cars[c].id=c;
		recycle(c); 	// init the car
		pthread_create_return_code = pthread_create(&(cars[c].thread), NULL, car_procedure, (void*)(unsigned long)c);
		if (pthread_create_return_code){
			printf("ERROR; Return code from pthread_create is %d\n", pthread_create_return_code);
		}
	}
	
	sleep(1);
	
	crossguard();
	
	teardown();

	pthread_exit(NULL);
}