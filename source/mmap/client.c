#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common/common.h"
int get_file_descriptor() {
	// Open a new file descriptor, creating the file if it does not exist
	// 0666 = read + write access for user, group and world
	int file_descriptor = open("/tmp/mmap", O_RDWR | O_CREAT, 0666);

	if (file_descriptor < 0) {
		throw("Error 1opening file!\n");
	}

	return file_descriptor;
}

void mmap_wait(atomic_char* guard) {
	while (atomic_load(guard) != 'c');
}

void mmap_notify(atomic_char* guard) {
	atomic_store(guard, 's');
}


void msg_wait(atomic_char*guard,char*file_memory,Benchmarks* bench) {
	    mmap_wait(guard);
		// Read
		memcpy(&bench->single_start, file_memory + 1, 8);

		benchmark(bench);
		// Write back
		

}



int main(int argc, char* argv[]) {
	// The memory region to which the file will be mapped
	unsigned long cnt=0;

	void* file_memory;
	// The file descriptor of the file we will
	// map into our process's memory
	int file_descriptor;
	// Fetch command-line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);
	file_descriptor = get_file_descriptor();
	// clang-format off
  file_memory = mmap(
		NULL,
		args.size+1,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		file_descriptor,
	  0
	 );
	// clang-format on

	if (file_memory < 0) {
		throw("Error mapping file!");
	}



	// Don't need the file descriptor anymore at this point
	if (close(file_descriptor) < 0) {
		throw("Error closing file!");
	}
	atomic_char* guard = (atomic_char*)file_memory;
	struct Benchmarks bench;
	setup_benchmarks(&bench);
    //初始化完成
	struct timeval startTime, currentTime;

    gettimeofday(&startTime, NULL);
    while (1) 
	{
	
    	gettimeofday(&currentTime, NULL);
    	if ((currentTime.tv_usec - startTime.tv_usec) > 100000) {
        	break;
    }
        mmap_notify(guard);
   		msg_wait(guard,file_memory,&bench);
    	cnt++;
    }
	printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);

	if (munmap(file_memory, args.size) < 0) {
		throw("Error unmapping file!");
	}

	remove("/tmp/mmap");
	close(file_descriptor);

	return EXIT_SUCCESS;
}
