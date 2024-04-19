#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "common/common.h"
#include "common/sockets.h"

#define PORT "6969"
#define HOST "localhost"


int get_address(struct addrinfo *server_info) {
	struct addrinfo *iterator;
	int socket_descriptor;

	// For system call return values
	int return_code;

	// Iterate through the address linked-list until
	// we find one we can get a socket for
	for (iterator = server_info; iterator != NULL; iterator = iterator->ai_next) {
		// The call to socket() is the first step in establishing a socket
		// based communication. It takes the following arguments:
		// 1. The address family (PF_INET or PF_INET6)
		// 2. The socket type (SOCK_STREAM or SOCK_DGRAM)
		// 3. The protocol (TCP or UDP)
		// Note that all of these fields will have been already populated
		// by getaddrinfo. If the call succeeds, it returns a valid file descriptor.
		// clang-format off
		socket_descriptor = socket(
			iterator->ai_family,
			iterator->ai_socktype,
			iterator->ai_protocol
		 );
		// clang-format on

		if (socket_descriptor == -1) continue;

		// Once we have a socket, we can connect it to the server's socket.
		// Again, this information we get from the addrinfo struct
		// that was populated by getaddrinfo(). The arguments are:
		// 1. The socket file_descriptor from which to connect.
		// 2. The address to connect to (sockaddr_in struct)
		// 3. The size of this address structure.
		// clang-format off
		return_code = connect(
			socket_descriptor,
			iterator->ai_addr,
			iterator->ai_addrlen
		);
		// clang-format on

		if (return_code == 1) {
			close(socket_descriptor);
		}

		break;
	}

	// If we didn't actually find a valid address
	if (iterator == NULL) {
		throw("Error finding valid address!");
	}

	// Return the valid address info
	return socket_descriptor;
}

void cleanup(int descriptor, void *buffer) {
	close(descriptor);
	free(buffer);
}








void get_server_information(struct addrinfo **server_info) {
	// For system call return values
	int return_code;

	// We can supply some hints to the call to getaddrinfo
	// as to what socket family (domain) or what socket type
	// we want for the server address.
	struct addrinfo hints;

	// Fill the hints with zeros first
	memset(&hints, 0, sizeof hints);

	// We set to AF_UNSPEC so that we can work
	// with either IPv6 or IPv4
	hints.ai_family = AF_UNSPEC;
	// Stream socket (TCP) as opposed to datagram sockets (UDP)
	hints.ai_socktype = SOCK_STREAM;

	return_code = getaddrinfo(HOST, PORT, &hints, server_info);

	if (return_code != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(return_code));
		exit(EXIT_FAILURE);
	}
}

void setup_socket(int socket_descriptor, int busy_waiting) {
	set_socket_both_buffer_sizes(socket_descriptor);

	if (busy_waiting) {
		// adjust_socket_blocking_timeout(socket_descriptor, 0, 10);
		if (set_io_flag(socket_descriptor, O_NONBLOCK) == -1) {
			throw("Error setting socket to non-blocking on client-side");
		}
	}
}

int create_socket(int busy_waiting) {

	struct addrinfo *server_info = NULL;

	// The file-descriptor of the socket we will open
	int socket_descriptor;

	get_server_information(&server_info);
	socket_descriptor = get_address(server_info);

	setup_socket(socket_descriptor, busy_waiting);

	// Don't need this anymore
	freeaddrinfo(server_info);

	return socket_descriptor;
}





int main(int argc, char *argv[]) {
	// Sockets are returned by the OS as standard file descriptors.
	// It will be used for all communication with the server.
	int socket_descriptor;
    unsigned long cnt = 0;
	// Flag to determine whether or not to do busy waiting
	// and not block the socket, or use normal blocking sockets.
	int busy_waiting;

	// Command-line arguments
	struct Arguments args;

	busy_waiting = check_flag("busy", argc, argv);
	parse_arguments(&args, argc, argv);

	socket_descriptor = create_socket(busy_waiting);
	struct Benchmarks bench;
	setup_benchmarks(&bench);
	void *buffer;
	buffer = malloc(args.size);
	//初始化完成
	struct timeval startTime, currentTime;
    gettimeofday(&startTime, NULL);
	//communicate(socket_descriptor, &args, busy_waiting);
	while (1) 
	{
	    
    	gettimeofday(&currentTime, NULL);
    	if ((currentTime.tv_usec - startTime.tv_usec) > 100000) {
        	break;
    }
        if (send(socket_descriptor, buffer, args.size, 0) == -1) {
			throw("Error sending data on client-side");
		}
   		if (receive(socket_descriptor, &bench.single_start,8, busy_waiting) == -1) {
			throw("Error receiving data on client-side");
		}

		benchmark(&bench);
    	cnt++;
    }
	printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);
	cleanup(socket_descriptor, buffer);

	return EXIT_SUCCESS;
}

