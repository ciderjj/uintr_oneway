#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>

#include "common/common.h"

#define FIFO_PATH "/tmp/ipc_bench_fifo"

void cleanup(FILE* stream) {
	
	fclose(stream);
	if (remove(FIFO_PATH) == -1) {
		throw("Error removing FIFO");
	}
}

void communicate(FILE* stream,
								 struct Arguments* args,
								 struct sigaction* signal_action) {

	int message;
	wait_for_signal(signal_action);

	for (message = 0; message < args->count; ++message) {
		uint64_t timestamp = now();

		if (fwrite( &timestamp, 8, 1, stream) == 0) {
			throw("Error writing buffer");
		}
		// Send off immediately (for small buffers)
		fflush(stream);

		notify_client();
		wait_for_signal(signal_action);

	
	}

	
	cleanup(stream);
}

FILE* open_fifo() {
	FILE* stream;

	// Just in case it already exists
	//(void)remove(FIFO_PATH);

	// Create a FIFO object. Note that a FIFO is
	// really just a special file, which must be
	// opened by one process and to which then
	// both server and client can write using standard
	// c i/o functions. 0666 specifies read+write
	// access permissions for the user, group and world
	if (mkfifo(FIFO_PATH, 0666) > 0) {
		throw("Error creating FIFO");
	}

	// Tell the client the fifo now exists and
	// can be opened from the read end
	notify_client();

	// Because a fifo is really just a file, we can
	// open a normal FILE* stream to it (in write mode)
	// Note that this call will block until the read-end
	// is opened by the client
	if ((stream = fopen(FIFO_PATH, "w")) == NULL) {
		throw("Error opening descriptor to FIFO on server side");
	}

	return stream;
}

int main(int argc, char* argv[]) {
	// The file pointer we will associate with the FIFO
	FILE* stream;
	// For server/client signals
	struct sigaction signal_action;

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	setup_server_signals(&signal_action);
	stream = open_fifo();

	communicate(stream, &args, &signal_action);

	return EXIT_SUCCESS;
}
