#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/fcntl.h>
#include <errno.h>

#include "common/common.h"

void client_communicate(int descriptor, struct Arguments* args) {
    struct Benchmarks bench;
    setup_benchmarks(&bench);
    int count = args->count;
    for (; count > 0; --count) {
        // Set the file descriptor to non-blocking mode
        int flags = fcntl(descriptor, F_GETFL, 0);
        fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);


        while (1) {
            if (read(descriptor, &bench.single_start, sizeof(uint64_t)) == -1) {
                if (errno == EAGAIN) {
                    // No data available, continue to try reading
                    continue;
                } else {
                    perror("Error reading from eventfd");
                    exit(EXIT_FAILURE);
                }
            }
            if (errno != EAGAIN) {
                // Data read successfully
                break;
            }
        }
        benchmark(&bench);
    }
    evaluate(&bench, args);
}

void server_communicate(int descriptor, struct Arguments* args) {
    int message;
    for (message = 0; message < args->count; ++message) {
        uint64_t timestamp = now();
        if (write(descriptor, &timestamp, sizeof(uint64_t)) == -1) {
            perror("Error writing to eventfd");
            exit(EXIT_FAILURE);
        }
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