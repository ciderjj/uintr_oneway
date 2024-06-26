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

volatile unsigned long uintr_received;
int uintrfd_client;
int uipi_index;


void cleanup(int segment_id, char* shared_memory) {   //清理共享内存
/*
 从这个进程的地址空间中分离共享内存。
 如果这是最后一个使用该共享内存的进程，共享内存将被移除
*/
	shmdt(shared_memory);  //将共享内存从进程的地址空间中分离

	shmctl(segment_id, IPC_RMID, NULL);  //删除共享内存
}

void setup_server(char* shared_memory) {  //服务器的初始化以及注册中断发送方、中断接收方

    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);   //使用UNIX域套接字进行通信 UDP套接字
    struct sockaddr_un un;  //存储UNIX域套接字的地址信息
    un.sun_family = AF_UNIX;   //该套接字地址结构使用UNIX域套接字地址族
    unlink("process_b");  //清除之前创建的UNIX域套接字文件
    strcpy(un.sun_path, "process_b"); //设置套接字地址
    if (bind(socket_fd, (struct sockaddr*)&un, sizeof(un)) < 0) { //绑定套接字
          printf("bind failed\n");
          exit(EXIT_FAILURE);
    }

    char buf[512];
    struct iovec f = {buf, 512};
    char dmsg[CMSG_SPACE(sizeof(int))];
    struct msghdr n = {NULL, 0, &f, 1, dmsg, sizeof(dmsg), 0};

    int o = recvmsg(socket_fd, &n, 0);
    if(o<0)
    {
            exit(EXIT_FAILURE);
    }

    struct cmsghdr *d = CMSG_FIRSTHDR(&n);

    int uintrfd_client = *(int*)CMSG_DATA(d);
	uipi_index= uintr_register_sender(uintrfd_client, 0); //注册客户端的uintrfd
    
}


void uintrfd_notify(unsigned int token) {

	_senduipi(uipi_index);

}

void communicate(char* shared_memory, struct Arguments* args) {
 

	// Setup server
	setup_server(shared_memory);  //初始化服务器
		// Write
    
	
    while (1)  {
	uint64_t timestamp = now();
    

    __kfifo_in(fifo, &timestamp, 8);  //往无锁队列里写数据


	uintrfd_notify(CLIENT_TOKEN);   //通知客户端
    
	while(kfifo_len(fifo)!=0);


	}


}

int main(int argc, char* argv[]) {

	int segment_id;  //共享内存的标识符
	char* shared_memory;  //共享内存

	key_t segment_key;  //共享内存的key 

    // Fetch command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);

	segment_key = ftok("uintr-shm",'X');
    /*
		参数： 1. 共享内存键。这必须在整个操作系统中是唯一的。 
		2. 要分配的字节数。这将被舍入到操作系统的页面大小以实现对齐。 
		3. 创建标志和权限位，其中： 
		- IPC_CREAT 表示将创建一个新段 
		- IPC_EXCL 表示如果段键已被使用，则调用将失败 
        - 0666 表示用户、组和全局的读写权限。 当共享内存键已经存在时，此调用将失败。要查看当前正在使用哪些键，并移除特定段，
		您可以使用以下shell命令： 
		- 使用 ipcs -m 来显示共享内存段及其ID 
		- 使用 ipcrm -m <segment_id> 来移除/释放共享内存段
	*/
	segment_id = shmget(segment_key, sizeof(struct __kfifo) + DEFAULT_MESSAGE_SIZE, IPC_CREAT | 0666);  //创建共享内存

	if (segment_id < 0) {
		printf("Error allocating segment");
	}

	/*
		创建共享内存段后，必须将其附加到希望使用它的每个进程的地址空间。为此，我们传递：
      1.由shmget返回的段ID
      2.要附加共享内存段的指针。此地址必须是页面对齐的。如果简单地传递NULL，则操作系统将找到适合附加段的区域。
      3.标志，例如：
        SHM_RND：将第二个参数（要附加的地址）舍入到页面大小的倍数。如果您不传递此标志但指定非空地址作为第二个参数，则必须自行确保页面对齐。
        SHM_RDONLY：仅用于读取附加（独立于访问位） shmat将返回指向它附加共享内存的地址空间的指针。使用fork()创建的子进程会继承此段。
	*/

	shared_memory = (char*)shmat(segment_id, NULL, 0);  //将共享内存附加到进程的地址空间

	if (shared_memory == (char*)-1) {
		printf("Error attaching segment");
	}


	fifo = (struct __kfifo*) shared_memory;

	__kfifo_init(fifo, shared_memory + sizeof(struct __kfifo), DEFAULT_MESSAGE_SIZE, sizeof(char));  //初始化无锁队列

	communicate(shared_memory, &args);

	cleanup(segment_id, shared_memory);

	return EXIT_SUCCESS;
}