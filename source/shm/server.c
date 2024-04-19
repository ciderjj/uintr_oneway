#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdint.h>

#include "common/common.h"


void cleanup(int segment_id, char* shared_memory) {
	// Detach the shared memory from this process' address space.
	// If this is the last process using this shared memory, it is removed.
	shmdt(shared_memory);

	/*
		Deallocate manually for security. We pass:
			1. The shared memory ID returned by shmget.
			2. The IPC_RMID flag to schedule removal/deallocation
				 of the shared memory.
			3. NULL to the last struct parameter, as it is not relevant
				 for deletion (it is populated with certain fields for other
				 calls, notably IPC_STAT, where you would pass a struct shmid_ds*).
	*/
	shmctl(segment_id, IPC_RMID, NULL);
}

void shm_wait(atomic_char* guard) {
	while (atomic_load(guard) != 's');
}

void shm_notify(atomic_char* guard) {
	atomic_store(guard, 'c');
}

void communicate(char* shared_memory, struct Arguments* args) {


	atomic_char* guard = (atomic_char*)shared_memory;
    
	// Wait for signal from client
	
	while(1){

        shm_wait(guard);

		uint64_t timestamp = now();

		// Write
		memcpy(shared_memory + 1, &timestamp, sizeof(uint64_t));
        

		shm_notify(guard);



		
	

	}
}

int main(int argc, char* argv[]) {
	// The identifier for the shared memory segment
	int segment_id;

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
		throw("Error allocating segment");
	}

	shared_memory = (char*)shmat(segment_id, NULL, 0);

	if (shared_memory == (char*)-1) {
		throw("Error attaching segment");
	}

	communicate(shared_memory, &args);

	cleanup(segment_id, shared_memory);

	return EXIT_SUCCESS;
}
