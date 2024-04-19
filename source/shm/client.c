#include <assert.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdint.h>

#include "common/common.h"




void cleanup(char* shared_memory) {
	// Detach the shared memory from this process' address space.
	// If this is the last process using this shared memory, it is removed.
	shmdt(shared_memory);
}

void shm_wait(atomic_char* guard) {
        while (atomic_load(guard) != 'c');

}

void shm_notify(atomic_char* guard) {
	atomic_store(guard, 's');
}


void msg_wait(atomic_char*guard,char*shared_memory,Benchmarks* bench) {
	    shm_wait(guard);
		// Read
		memcpy(&bench->single_start, shared_memory + 1, 8);

		benchmark(bench);
		// Write back
		

}

int main(int argc, char* argv[]) {
	// The identifier for the shared memory segment
	int segment_id;

    unsigned long cnt=0;
	// The *actual* shared memory, that this and other
	// processes can read and write to as if it were
	// any other plain old memory
	char* shared_memory;

	// Key for the memory segment
	key_t segment_key;

	// Fetch command-line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);

	segment_key = generate_key("shm");

	segment_id = shmget(segment_key, 1 + args.size, IPC_CREAT | 0666);

	if (segment_id < 0) {
		throw("Could not get segment");
	}

	shared_memory = (char*)shmat(segment_id, NULL, 0);

	if (shared_memory < (char*)0) {
		throw("Could not attach segment");
	}

	//communicate(shared_memory, &args);
    atomic_char* guard = (atomic_char*)shared_memory;
	struct Benchmarks bench;
	setup_benchmarks(&bench);
	//shm_notify(guard);
	//初始化完成
	struct timeval startTime, currentTime;

    gettimeofday(&startTime, NULL);
    
    while (1) 
	{
	
    	gettimeofday(&currentTime, NULL);
    	if ((currentTime.tv_usec - startTime.tv_usec) > 100000) {
        	break;
    }
        shm_notify(guard);
   		msg_wait(guard,shared_memory,&bench);
    	cnt++;
    }
	printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);
	cleanup(shared_memory);

	return EXIT_SUCCESS;
}

