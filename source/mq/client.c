#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "mq/mq-common.h"

void msg_wait(int mq,struct Message* message,Benchmarks* bench) {

		if (msgrcv(mq, message, 8, SERVER_MESSAGE, 0) < 8) {
			throw("Error receiving on client-side");
		}
		memcpy(&bench->single_start, &message->buffer, 8);
        benchmark(bench);
	
    
}

int create_mq() {
	int mq;
	key_t key;

	client_once(WAIT);

	// Generate a key for the message-queue
	key = generate_key("mq");

	// msgget is the call to create or retrieve a message queue.
	// It is identified by its unique key, which we generated above
	// and which we pass as first argument. 0666 are the
	// read+write permissions for user, group and world.
	if ((mq = msgget(key, 0666)) == -1) {
		throw("Error retrieving message-queue on client-side");
	}

	return mq;
}

int main(int argc, char* argv[]) {
	unsigned long cnt = 0;
	// A message-queue is simply identified
	// by a numeric ID
	int mq;
    struct Message* message;
	
	// For command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);
   
	if (args.size > MAXIMUM_MESSAGE_SIZE) {
		args.size = MAXIMUM_MESSAGE_SIZE;
	}
    message = create_message(&args);
	mq = create_mq();
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
        message->type = CLIENT_MESSAGE;
		memset(message->buffer, '1', args.size);

		
		if (msgsnd(mq, message, args.size, 0) == -1) {
			throw("Error sending on client-side");
		}

   		msg_wait(mq,message,&bench);
    	cnt++;
    }
	printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);
	free(message);
	return EXIT_SUCCESS;
}
