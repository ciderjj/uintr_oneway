#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "common/common.h"

void client_communicate(int descriptor, struct Arguments* args) {
    struct Benchmarks bench;
    setup_benchmarks(&bench);
	int count=args->count;
	for (; count > 0; --count) {
	

		// A write *adds* the value stored in the
		// eventfd to the value in the 8-byte buffer passed.
		// Here we send the current timestamp to the server
		if (read(descriptor, &bench.single_start, 8) == -1) {
			throw("Error reading to eventfd");
		}
		benchmark(&bench);
	}
	evaluate(&bench, args);
}


void server_communicate(int descriptor, struct Arguments* args) {
	int message;




	for (message = 0; message < args->count; ++message) {
		uint64_t timestamp = now();

		if (write(descriptor, &timestamp, 8) == -1) {
			throw("Error writing from eventfd");
		}

		
	}

	// The message size is always one (it's just a signal)
}

void communicate(int descriptor, struct Arguments* args) {
	// File descriptors can only be shared bewteen related processes,
	// therefore we will need to fork this process
	pid_t pid;

	// Fork a child process
	if ((pid = fork()) == -1) {
		throw("Error forking process");
	}

	// fork() returns 0 for the child process
	if (pid == (pid_t)0) {
		client_communicate(descriptor, args);
		close(descriptor);
	} else {
		server_communicate(descriptor, args);
	}
}

int main(int argc, char* argv[]) {
	// An eventfd object is simply a file in the file-system, that can
	// be used as a wait/notify-mechanism similar to a semaphore. It
	// is associated with a standard file descriptor and thus the
	// usual read() and write() functions can be used, albeit their
	// behaviour is different than for standard files.
	// Stored in the eventfd itself is a simple 64-bit/8-Byte integer.
	int descriptor; 

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	// Create a new eventfd object and get the corresponding
	// file descriptor. The first argument is the initial value,
	// which we just set to 0 here and the second argument are
	// any flags (a bitwise-OR combination thereof), such as:
	// EFD_NONBLOCK: Causes read() and write() to return immdediately
	//               where the calls would normally block. In that case,
	//               the return code is EAGAIN.
	// EFD_SEMAPHORE: Causes reads to always return a value of 1 and
	//                decrement the value stored in the eventfd by 1.
	descriptor = eventfd(0, 0);

	communicate(descriptor, &args);

	return EXIT_SUCCESS;
}
