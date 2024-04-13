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

void communicate(FILE *stream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	struct Benchmarks bench;
    int count=args->count;
	setup_benchmarks(&bench);
	// Server can go
	notify_server();

	for (; count > 0; --count) {
		wait_for_signal(signal_action);

		if (fread(&bench.single_start, 8, 1, stream) == 0) {
			throw("Error reading buffer");
		}
        benchmark(&bench);
		notify_server();
	}
    evaluate(&bench, args);
	cleanup(stream); 
}

FILE *open_fifo(struct sigaction *signal_action) {
	FILE *stream;

	// Wait for the server to create the FIFO
	wait_for_signal(signal_action);

	// Because a FIFO is really just a file, we can
	// open a normal FILE* stream to it (in read mode)
	// Note that this call will block until the write-end
	// is opened by the server
	if ((stream = fopen(FIFO_PATH, "r")) == NULL) {
		throw("Error opening stream to FIFO on client-side");
	}

	return stream;
}

int main(int argc, char *argv[]) {
	// The file pointer we will associate with the FIFO
	FILE *stream;
	// For server/client signals
	struct sigaction signal_action;

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	setup_client_signals(&signal_action);
	stream = open_fifo(&signal_action);

	communicate(stream, &args, &signal_action);

	return EXIT_SUCCESS;
}
