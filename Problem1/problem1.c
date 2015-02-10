/*

There are three security levels and corresponding security level numbers:
0: No job (IDLE)
1: Unclassified job (U)
2: Secret job (S)
3: Top Secret job (TS)

We have several mutex-locked global variables to keep track of the state of the clusters and the jobs waiting to run on them:
2 state variables representing the security level of the jobs running in each half of the cluser: cluster_A_level and cluster_B_level 
3 variables for number of TS,S,U processes waiting to run, num_U, num_S, and num_TS
2 condition variables used for broadcasting the availability of a cluster for a certain security level job: U_available, S_available (for S and TS availability)
1 flag indicating if there were 3 TS jobs to run, and 1 was run, but another should still be run (even though num_TS is no longer >3) so that "if at least 3 TS jobs are ready to run, the cluster must ensure the first two TS jobs will be the next to run in the cluster": pending_TS

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

#define num_threads 20


int main(int argc, const char* argv[]){
		
}