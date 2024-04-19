#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>

#include "common/common.h"
#include "mq/mq-common.h"

void cleanup(int mq, struct Message* message) {
	// Destroy the message queue.
	// Takes the message-queue ID and an operation,
	// in this case IPC_RMID. The last parameter, for
	// options and other information, can be set to
	// NULL for the purpose of removing the queue.
	if (msgctl(mq, IPC_RMID, NULL) == -1) {
		throw("Error removing message queue");
	}

	free(message);
}


void communicate(int mq, struct Arguments* args) {

	struct Message* message;
	
	message = create_message(args);

	while(1) {
		if (msgrcv(mq, message, args->size, CLIENT_MESSAGE, 0) < args->size) {
			throw("Error receiving on server-side");
		}
		uint64_t timestamp = now();

		message->type = SERVER_MESSAGE;
		memcpy(message->buffer, &timestamp, 8);

		// Same parameters as msgrcv, but no message-type
		// (because it is determined by the message's member)
		if (msgsnd(mq, message, 8, IPC_NOWAIT) == -1) {
			throw("Error sending on server-side");
		}

		

	
	}

	// Since the buffer size must be fixed
	// args->size = MESSAGE_SIZE;


	cleanup(mq, message);
}

int create_mq() {
	int mq;

	// Generate a key for the message-queue
	key_t key = generate_key("mq");

	// msgget is the call to create or retrieve a message queue.
	// It is identified by its unique key, which we generated above
	// and which we pass as first argument. The second argument is
	// the set of flags for the message queue. By passing IPC_CREAT,
	// the message-queue will be created if it does not yet exist.
	// 0666 are the read+write permissions for user,
	// group and world.
	if ((mq = msgget(key, IPC_CREAT | 0666)) == -1) {
		throw("Error creating message-queue on server-side");
	}

	// Tell the client it can now get the queue
	server_once(NOTIFY);

	return mq;
}

void limit_message_size(struct Arguments* args) {
	if (args->size > MAXIMUM_MESSAGE_SIZE) {
		args->size = MAXIMUM_MESSAGE_SIZE;
		// clang-format off
		fprintf(
			stderr,
			"Reduced the message size to %d bytes!\n",
			MAXIMUM_MESSAGE_SIZE
		);
		// clang-format on
	}
}

int main(int argc, char* argv[]) {
	// A message-queue is simply identified
	// by a numeric ID
	int mq;

	// For command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);

	limit_message_size(&args);

	mq = create_mq();
	communicate(mq, &args);

	return EXIT_SUCCESS;
}
