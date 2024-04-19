#include <assert.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <x86gprintrin.h>
#include <stddef.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "common/common.h"


#ifndef __NR_uintr_register_handler
#define __NR_uintr_register_handler	471
#define __NR_uintr_unregister_handler	472
#define __NR_uintr_create_fd		473
#define __NR_uintr_register_sender	474
#define __NR_uintr_unregister_sender	475
#define __NR_uintr_wait			476
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(fd, flags)	syscall(__NR_uintr_unregister_sender, fd, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)

#define SERVER_TOKEN 0
#define CLIENT_TOKEN 1

struct __kfifo* fifo;
unsigned long cnt=0;
volatile unsigned long uintr_received;   //用于存储客户端和服务器的uintrfd
int uintrfd_client;

int uipi_index;  //客户端和服务器的uipi索引
struct Benchmarks bench;
char* shared_memory;  //共享内存

void __attribute__ ((interrupt))  //声明一个中断处理函数
     __attribute__((target("general-regs-only", "inline-all-stringops")))
     ui_handler(struct __uintr_frame *ui_frame,
		unsigned long long vector) {

	fifo->data=shared_memory + sizeof(struct __kfifo);
    __kfifo_out(fifo, &bench.single_start, kfifo_len(fifo));

}

void cleanup(char* shared_memory) {  //清理共享内存
	// Detach the shared memory from this process' address space.
	// If this is the last process using this shared memory, it is removed.
	shmdt(shared_memory);
}


void setup_client(char* shared_memory) { //客户端的初始化以及注册中断接收方、中断发送方

	int mfd = socket(AF_UNIX, SOCK_DGRAM, 0); // 创建UNIX域数据报套接字
    struct sockaddr_un ad; 
    ad.sun_family = AF_UNIX;
    strcpy(ad.sun_path, "process_b");

    struct iovec f = {NULL, 0};
    char dmsg[CMSG_SPACE(sizeof(int))];
    struct msghdr o = {(void*)&ad, sizeof(ad), &f, 1, dmsg, sizeof(dmsg), 0};
    struct cmsghdr *d = CMSG_FIRSTHDR(&o);
    d->cmsg_level = SOL_SOCKET;
    d->cmsg_type = SCM_RIGHTS;
    d->cmsg_len = CMSG_LEN(sizeof(int));

    *(int*)CMSG_DATA(d) = uintrfd_client; // 设置文件描述符

    int ret = sendmsg(mfd, &o, 0);//发送文件描述符

    if (ret == -1) {
        perror("Send message failed");
        exit(EXIT_FAILURE);
    }
	_stui();  //开启中断
}

void destroy_client() {   //销毁客户端
	close(uintrfd_client);

	uintr_unregister_handler(0);

}


int main(int argc, char* argv[]) {
	unsigned long cnt=0;

    int segment_id;   //共享内存的标识符

	

	key_t segment_key;  //共享内存的key

	struct Arguments args;

	parse_arguments(&args, argc, argv);

	segment_key = ftok("uintr-shm",'X');  //生成共享内存的key

	segment_id = shmget(segment_key, sizeof(struct __kfifo) + DEFAULT_MESSAGE_SIZE, IPC_CREAT | 0666);  //创建共享内存
	if (segment_id < 0) {
		printf("Could not get segment");
	}

	shared_memory = (char*)shmat(segment_id, NULL, 0);  //将共享内存附加到进程的地址空间
	if (shared_memory < (char*)0) {   //附加共享内存失败
		printf("Could not attach segment");
	}

    fifo = (struct __kfifo*) shared_memory;
    
    setup_benchmarks(&bench);
    //初始化完成
    setup_client(shared_memory);  //客户端的初始化
    struct timeval startTime, currentTime;
    gettimeofday(&startTime, NULL);
    while (1) 
	{
	
    	gettimeofday(&currentTime, NULL);
    	if ((currentTime.tv_usec - startTime.tv_usec) > 100000) {
        	break;
    }
    	cnt++;
    }
    printf("\ncnt=%ld",cnt);
    evaluate(&bench, &args);
	destroy_client();
	cleanup(shared_memory);

	return EXIT_SUCCESS;
}


