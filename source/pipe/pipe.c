#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/common.h"

FILE *open_stream(int file_descriptor[2], int to_open) {
	FILE *stream;
	char mode[1];
	mode[0] = to_open ? 'w' : 'r';

	// Don't need this end anymore
	close(file_descriptor[1 - to_open]);

	// Open a stream to the other end
	stream = fdopen(file_descriptor[to_open], mode);

	if (stream == NULL) {
		throw("Could not open stream for reading");
	}

	return stream;
}

void client_communicate(int file_descriptors[2], struct Arguments *args) {
	struct sigaction signal_action;
	FILE *stream;

    struct Benchmarks bench;
	stream = open_stream(file_descriptors, 0);
	setup_client_signals(&signal_action);

    setup_benchmarks(&bench);
	// Set things in motion
	int count=args->count;
	notify_server();
    
	for (; count > 0; --count) {
		wait_for_signal(&signal_action);

		if (fread(&bench.single_start, 8, 1, stream) == -1) {
			throw("Error reading from pipe");
		}
        benchmark(&bench);
		notify_server();
	}

	// Now close the write end too
	evaluate(&bench, args);
	close(file_descriptors[1]);
}

void server_communicate(int file_descriptors[2], struct Arguments *args) {
	struct sigaction signal_action;
	
	FILE *stream;

	int message;

	stream = open_stream(file_descriptors, 1);
	setup_server_signals(&signal_action);


	wait_for_signal(&signal_action);

	for (message = 0; message < args->count; ++message) {
		uint64_t timestamp = now();

		if (fwrite(&timestamp, 8, 1, stream) == -1) {
			throw("Error writing to pipe");
		}
		// Send immediately
		fflush(stream);

		notify_client();
		wait_for_signal(&signal_action);
		
	}

	// Now close the write end too
	close(file_descriptors[1]);

}

void communicate(int file_descriptors[2], struct Arguments *args) {
	// For the process ID of the spawned child process
	pid_t pid;

	// Fork a child process
	if ((pid = fork()) == -1) {
		throw("Error forking process");
	}

	// fork() returns 0 for the child process
	if (pid == (pid_t)0) {
		client_communicate(file_descriptors, args);
	}

	else {
		server_communicate(file_descriptors, args);
	}
}

int main(int argc, char *argv[]) {
	// The call to pipe will return two file descriptors
	// for the read and write end of the pipe, respectively
	int file_descriptors[2];

	struct Arguments args;

	// check_flag("help", argc, argv);
	parse_arguments(&args, argc, argv);

	// The call that creates a new pipe object and places two
	// valid file descriptors in the array we pass it. The first
	// entry at [0] is the read end (from which you read) and
	// second entry at [1] is the write end (to which you write)
	// Note that these file descriptors will only be visible to
	// the current process and any children it spawns. This is
	// mainly what distinguishes pipes from FIFOs (named pipes)
	if (pipe(file_descriptors) < 0) {
		throw("Error opening pipe!\n");
	}

	communicate(file_descriptors, &args);

	return EXIT_SUCCESS;
}
