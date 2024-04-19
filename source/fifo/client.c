#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/common.h"

#define FIFO_PATH "/tmp/ipc_bench_fifo"

void cleanup(FILE *stream) {
	fclose(stream);
}


FILE *open_fifo(struct sigaction *signal_action) {
	FILE *stream;

	// Wait for the server to create the FIFO
	//wait_for_signal(signal_action);
	// Because a FIFO is really just a file, we can
	// open a normal FILE* stream to it (in read mode)
	// Note that this call will block until the write-end
	// is opened by the server
	if ((stream = fopen(FIFO_PATH, "r")) == NULL) {
		throw("Error opening stream to FIFO on client-side");
	}

	return stream;
}


void msg_wait(FILE* stream,struct sigaction* signal_action,Benchmarks* bench) {
	    wait_for_signal(signal_action);
		// Read
		if (fread(&bench->single_start, 8, 1, stream) == 0) {
			throw("Error reading buffer");
		}

		benchmark(bench);
		// Write back
		

}



int main(int argc, char *argv[]) {
	// The file pointer we will associate with the FIFO
	FILE *stream;
	unsigned long cnt = 0;
	// For server/client signals
	struct sigaction signal_action;

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	setup_client_signals(&signal_action);
    
	stream = open_fifo(&signal_action);

    
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
        notify_server();
   		msg_wait(stream,&signal_action,&bench);
    	cnt++;
    }
	printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);
	cleanup(stream);

	return EXIT_SUCCESS;
}
