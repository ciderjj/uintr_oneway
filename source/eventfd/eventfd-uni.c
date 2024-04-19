#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/fcntl.h>
#include <errno.h>

#include "common/common.h"

void client_communicate(int descriptor, struct Arguments* args) {
    unsigned long cnt = 0;
    struct Benchmarks bench;
    setup_benchmarks(&bench);
    struct timeval startTime, currentTime;
    gettimeofday(&startTime, NULL);
        while (1) {
            gettimeofday(&currentTime, NULL);
    	    if ((currentTime.tv_usec - startTime.tv_usec) > 100000) {
        	break;
    }
            
            if (read(descriptor, &bench.single_start, 8) == -1) {
                    perror("Error reading from eventfd");
                    exit(EXIT_FAILURE);   
            }      
            printf("client=%ld\n",bench.single_start);
        benchmark(&bench);
        cnt++;
        }
        printf("\ncnt=%ld",cnt);
        evaluate(&bench, args);
}

void server_communicate(int descriptor, struct Arguments* args) {
  
    while (1) {
        uint64_t timestamp = now();
        if (write(descriptor, &timestamp, 8) == -1) {
            perror("Error writing to eventfd");
            exit(EXIT_FAILURE);
        }
        printf("server=%ld\n",timestamp);
    }
}

void communicate(int descriptor, struct Arguments* args) {
    pid_t pid;
    if ((pid = fork()) == -1) {
        perror("Error forking process");
        exit(EXIT_FAILURE);
    }

    if (pid == (pid_t)0) {
        client_communicate(descriptor, args);
        close(descriptor);
    } else {
        server_communicate(descriptor, args);
    }
}

int main(int argc, char* argv[]) {
    int descriptor;
    struct Arguments args;
    parse_arguments(&args, argc, argv);

    descriptor = eventfd(0, 0);

    communicate(descriptor, &args);

    return EXIT_SUCCESS;
}