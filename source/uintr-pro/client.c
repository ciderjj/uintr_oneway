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

volatile unsigned long uintr_received[2];   //用于存储客户端和服务器的uintrfd
int uintrfd_client;
int uintrfd_server;
int uipi_index[2];  //客户端和服务器的uipi索引

void __attribute__ ((interrupt))  //声明一个中断处理函数
     __attribute__((target("general-regs-only", "inline-all-stringops")))
     ui_handler(struct __uintr_frame *ui_frame,
		unsigned long long vector) {

		uintr_received[vector] = 1;   //收到中断后将对应的uintr_received置1
}

void cleanup(char* shared_memory) {  //清理共享内存
	// Detach the shared memory from this process' address space.
	// If this is the last process using this shared memory, it is removed.
	shmdt(shared_memory);
}


int setup_handler_with_vector(int vector) {  //注册中断处理器并返回对应的文件描述符
	int fd;

	if (uintr_register_handler(ui_handler, 0))  //注册中断处理器
		printf("Interrupt handler register error\n");

	
	fd = uintr_create_fd(vector, 0);  //创建中断文件描述符

	if (fd < 0)
		printf("Interrupt vector registration error\n");

	return fd;
}

void setup_client(char* shared_memory) { //客户端的初始化以及注册中断接收方、中断发送方

    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);   //使用UNIX域套接字进行通信 UDP套接字

    struct sockaddr_un un;  //存储UNIX域套接字的地址信息

    un.sun_family = AF_UNIX;   //该套接字地址结构使用UNIX域套接字地址族

    unlink("process_a");  //清除之前创建的UNIX域套接字文件
    strcpy(un.sun_path, "process_a"); //设置套接字地址

    if (bind(socket_fd, (struct sockaddr*)&un, sizeof(un)) < 0) { //绑定套接字
        printf("bind failed\n");
		exit(EXIT_FAILURE);

    }

    char buf[512];
    struct iovec e = {buf, 512};

    char cmsg[CMSG_SPACE(sizeof(int))];
    struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};

    int n = recvmsg(socket_fd, &m, 0);  //接收文件描述符
    if(n<0)
    {
            exit(EXIT_FAILURE);
    }
    struct cmsghdr *c = CMSG_FIRSTHDR(&m);

    int uintrfd_server = *(int*)CMSG_DATA(c);

	uipi_index[SERVER_TOKEN] = uintr_register_sender(uintrfd_server, 0);

	//client作为中断接收方注册成功，接下来作为中断发送方
    uintrfd_client = setup_handler_with_vector(CLIENT_TOKEN);  //注册中断处理器并返回对应的文件描述符

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

void uintrfd_wait(unsigned int token) {  //等待中断

	
	while (!uintr_received[token]);  //等待中断

	uintr_received[token] = 0;  //收到中断后将对应的uintr_received置0
}

void uintrfd_notify(unsigned int token) {  //通知中断

	_senduipi(uipi_index[token]);

}

void communicate(char* shared_memory, struct Arguments* args) {    //通信函数


	setup_client(shared_memory);  //客户端的初始化

    char input[] = {"Hello,I'm client\n"};  //输入数据
	
    for (; args->count > 0; --args->count) {

	uintrfd_wait(CLIENT_TOKEN);
		// Read
	fifo->data=shared_memory + sizeof(struct __kfifo);

	char *res = (char*)malloc(sizeof(char)*kfifo_len(fifo));
	
    __kfifo_out(fifo, res, kfifo_len(fifo));
    // 打印读取的数据
    // Print buffer contents
  
		// Write back
	

    __kfifo_in(fifo, input, strlen(input));  //往无锁队列里写数据

 
	uintrfd_notify(SERVER_TOKEN);

    }
	destroy_client();

}

int main(int argc, char* argv[]) {
	int segment_id;   //共享内存的标识符

	char* shared_memory;  //共享内存

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

	communicate(shared_memory, &args);

	cleanup(shared_memory);

	return EXIT_SUCCESS;
}