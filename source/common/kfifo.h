#include <stdlib.h>
#include <memory.h>
#include<stdio.h>
#define min(x,y) ((x)<(y)?(x):(y))  // 定义一个宏，用于返回两个数中的最小值
struct __kfifo {  // 定义一个结构体，用于描述 FIFO 的数据结构
        unsigned int    in;
        unsigned int    out;
        unsigned int    mask;
        unsigned int    esize;
        void            *data;
};
void print_kfifo(struct __kfifo *fifo);

unsigned int kfifo_len(struct __kfifo *fifo);

unsigned int kfifo_cap(struct __kfifo *fifo);
unsigned int kfifo_avail(struct __kfifo *fifo);

int kfifo_is_full(struct __kfifo *fifo);

int kfifo_is_empty(struct __kfifo *fifo);

int __kfifo_init(struct __kfifo *fifo, void *buffer, unsigned int size, size_t esize);

unsigned int __kfifo_in(struct __kfifo *fifo, const void *buf, unsigned int len);

unsigned int __kfifo_out_peek(struct __kfifo *fifo, void *buf, unsigned int len);

unsigned int __kfifo_out(struct __kfifo *fifo, void *buf, unsigned int len);

static inline unsigned int roundup_pow_of_two(unsigned int v);

static inline unsigned int kfifo_unused(struct __kfifo *fifo);

int is_power_of_2(unsigned int size);

static void kfifo_copy_in(struct __kfifo *fifo, const void *src, unsigned int len, unsigned int off);

static void kfifo_copy_out(struct __kfifo *fifo, void *dst, unsigned int len, unsigned int off);


 void print_kfifo(struct __kfifo *fifo) {  // 打印 kfifo 的状态
        unsigned int avail = kfifo_avail(fifo);
        unsigned int size = fifo->mask + 1;
        unsigned int in = fifo->in;
        unsigned int out = fifo->out;
        unsigned int esize = fifo->esize;

        printf("kfifo structure:\n");
        printf("Available items: %u\n", avail);
        printf("Size: %u\n", size);
        printf("In index: %u\n", in);
        printf("Out index: %u\n", out);
        printf("Element size: %u\n", esize);
    }


static inline unsigned int roundup_pow_of_two(unsigned int v) { //找最小上二次幂
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

unsigned int kfifo_len(struct __kfifo *fifo) { //返回fifo中的元素个数
    return fifo->in - fifo->out;
};


unsigned int kfifo_cap(struct __kfifo *fifo) { //返回fifo中的最大元素个数
    return fifo->mask + 1;
};

 
unsigned int kfifo_avail(struct __kfifo *fifo) {  //返回fifo中的剩余空间
    return kfifo_cap(fifo) - kfifo_len(fifo);
};

int kfifo_is_full(struct __kfifo *fifo) {  //判断fifo是否已满
    return kfifo_len(fifo) > fifo->mask;
};

int kfifo_is_empty(struct __kfifo *fifo) {  //判断fifo是否为空
    return fifo->in == fifo->out;
};



static inline unsigned int kfifo_unused(struct __kfifo *fifo)  //返回fifo中的剩余空间
{
	return (fifo->mask + 1) - (fifo->in - fifo->out);
}


int is_power_of_2(unsigned int size) { //判断size是否为2的幂次方
    return (size & (size - 1)) == 0;
}


int __kfifo_init(struct __kfifo *fifo, void *buffer,   //初始化fifo
		unsigned int size, size_t esize)
{
	size /= esize;  // 计算 FIFO 可容纳元素的数量，先将总大小除以每个元素的大小
	if (!is_power_of_2(size))
		size = roundup_pow_of_two(size);  // 将计算得到的元素数量向上取整至最近的 2 的幂次方数

	fifo->in = 0;  // 将 FIFO 中写入元素的位置初始化为 0
	fifo->out = 0;  // 将 FIFO 中读取元素的位置初始化为 0
	fifo->esize = esize;  // 设置 FIFO 中每个元素的大小
	fifo->data = buffer;  // 将 FIFO 的数据缓冲区指针指向传入的 buffer

	if (size < 2) {  // 如果计算得到的元素数量小于 2，表示初始化失败
		fifo->mask = 0;  // 将 FIFO 的 mask 设置为 0，表示初始化失败
		return -1;  // 返回 -1 表示初始化失败
	}
	fifo->mask = size - 1;  // 计算 mask 值，用于计算 FIFO 中元素的位置

	return 0;  // 返回 0 表示初始化成功
}


static void kfifo_copy_in(struct __kfifo *fifo, const void *src,   //将数据写入kfifo
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;  // 计算 FIFO 可容纳的元素数量
	unsigned int esize = fifo->esize;  // 得到 FIFO 中每个元素的大小
	unsigned int l;
	off &= fifo->mask;  // 计算偏移量 off，保证在FIFO大小范围内
	if (esize != 1) {  // 如果每个元素的大小不为1
		off *= esize;  // 将 off 乘以每个元素的大小
		size *= esize;  // 将 FIFO 可容纳的元素数量乘以每个元素的大小
		len *= esize;  // 将要拷贝的数据长度乘以每个元素的大小
	}
	l = min(len, size - off);  // 取 len 和 size-off 中的较小值，确保不溢出

	memcpy(fifo->data + off, src, l);  // 将数据从 src 拷贝到 FIFO 的偏移位置 off 处，长度为 l
	memcpy(fifo->data, src + l, len - l);  // 将 src 中剩余的数据拷贝到 FIFO 中，补全剩余数据部分
	// 确保在增加 fifo->in 索引计数器之前，FIFO 中的数据是最新的
	__sync_synchronize();
}


unsigned int __kfifo_in(struct __kfifo *fifo,   //将数据写入kfifo
		const void *buf, unsigned int len)
{
	unsigned int l;

	l = kfifo_unused(fifo);  // 获取 FIFO 中还未使用的空间大小
	if (len > l)  // 如果要拷贝的数据长度大于未使用的空间大小
	{	len = l;  // 限制要拷贝的数据长度为未使用的空间大小
                printf("space is not enough,can write %d\n",len);
        }
        kfifo_copy_in(fifo, buf, len, fifo->in);  // 将数据拷贝到 FIFO 中
	fifo->in += len;  // 更新 FIFO 中写入位置
	return len;  // 返回实际拷贝的数据长度
}

static void kfifo_copy_out(struct __kfifo *fifo, void *dst,   //从kfifo中读取数据
		unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;  // 计算 FIFO 可容纳的元素数量
	unsigned int esize = fifo->esize;  // 得到 FIFO 中每个元素的大小
	unsigned int l;

	off &= fifo->mask;  // 计算偏移量 off，保证在FIFO大小范围内
	if (esize != 1) {  // 如果每个元素的大小不为1
		off *= esize;  // 将 off 乘以每个元素的大小
		size *= esize;  // 将 FIFO 可容纳的元素数量乘以每个元素的大小
		len *= esize;  // 将要拷贝的数据长度乘以每个元素的大小
	}
	l = min(len, size - off);  // 取 len 和 size-off 中的较小值，确保不溢出
	memcpy(dst, fifo->data + off, l);  // 将 FIFO 中的数据从偏移位置 off 处拷贝到目标地址 dst，长度为 l
	memcpy(dst + l, fifo->data, len - l);  // 将 FIFO 中剩余的数据拷贝到目标地址 dst 中，补全剩余数据部分 
	// 确保在增加 fifo->out 索引计数器之前，数据已经拷贝完成
	__sync_synchronize();
}


unsigned int __kfifo_out_peek(struct __kfifo *fifo, //从kfifo中读取数据但不删除
		void *buf, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;  // 计算 FIFO 中待读取数据的长度
	if (len > l)  // 如果要读取的数据长度大于待读取数据的长度
		len = l;  // 限制要读取的数据长度为待读取数据的长度
	kfifo_copy_out(fifo, buf, len, fifo->out);  // 将 FIFO 中的数据拷贝到目标地址
	return len;  // 返回实际读取的数据长度
}


unsigned int __kfifo_out(struct __kfifo *fifo,  //从kfifo中读取数据并删除
		void *buf, unsigned int len)
{
	len = __kfifo_out_peek(fifo, buf, len);  // 调用 __kfifo_out_peek 函数，读取数据但不删除
	fifo->out += len;  // 更新 FIFO 中读取的位置
	return len;  // 返回实际读取的数据长度
}