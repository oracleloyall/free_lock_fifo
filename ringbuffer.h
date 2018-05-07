/*
 * create time 2018
 * ring_buffer.h
 * author zhaoxi;
 * */

#ifndef KFIFO_HEADER_H
#define KFIFO_HEADER_H

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <assert.h>
#include<cmath>
#include"lock.h"
/*
 * 进一步优化：初始化元素内存对齐，malloc用tcmalloc替换
 */
using namespace std;
/*
 mb()                      适用于多处理器和单处理器的内存屏障。
 rmb()                     适用于多处理器和单处理器的读内存屏障。
 wmb()                   适用于多处理器和单处理器的写内存屏障。
 smp_mb()            适用于多处理器的内存屏障。
 smp_rmb()           适用于多处理器的读内存屏障。
 smp_wmb()         适用于多处理器的写内存屏障。

 */
#define barrier() __asm__ __volatile__("": : :"memory")
#ifdef CONFIG_X86_32
/*指令“lock; addl $0,0(%%esp)”表示加锁，把0加到栈顶的内存单元，该指令操作本身无意义，但这些指令起到内存屏障的作用，让前面的指令执行完成。具有XMM2特征的CPU已有内存屏障指令，就直接使用该指令*/
#define mb() alternative("lock; addl $0,0(%%esp)", "mfence", X86_FEATURE_XMM2)
#define rmb() alternative("lock; addl $0,0(%%esp)", "lfence", X86_FEATURE_XMM2)
#define wmb() alternative("lock; addl $0,0(%%esp)", "sfence", X86_FEATURE_XMM)
#else
#define mb() asm volatile("mfence":::"memory")
#define rmb() asm volatile("lfence":::"memory")
#define wmb() asm volatile("sfence" ::: "memory")
#endif
#ifdef CONFIG_SMP
#define smp_mb() mb()
#ifdef CONFIG_X86_PPRO_FENCE
# define smp_rmb() rmb()
#else
# define smp_rmb() barrier()
#endif
#ifdef CONFIG_X86_OOSTORE
# define smp_wmb() wmb()
#else
# define smp_wmb() barrier()
#endif
#define smp_read_barrier_depends() read_barrier_depends()
#define set_mb(var, value) do { (void)xchg(&var, value); } while (0)
#else
#define smp_mb() barrier()
#define smp_rmb() barrier()
#define smp_wmb() barrier()
#define smp_read_barrier_depends() do { } while (0)
#define set_mb(var, value) do { var = value; barrier(); } while (0)
#endif

//判断x是否是2的次方
#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))
//取a和b中最小值
#define min(a, b) (((a) < (b)) ? (a) : (b))
/*
 *
 */
//typedef typename std::aligned_storage<sizeof(node_t),
//		std::alignment_of<node_t>::value>::type aligned_node_t;
typedef char cache_line_pad_t[64]; // it's either 32 or 64 so 64 is good enough


struct ring_buffer {
	cache_line_pad_t _pad0;
	void *buffer;
	uint32_t size;
	uint32_t in;
	uint32_t out;
	slock_t *f_lock;    //spinlock
};
//init buffer
struct ring_buffer* ring_buffer_init(void *buffer, uint32_t size,
		slock_t *f_lock) {
	assert(buffer);
	struct ring_buffer *ring_buf = NULL;
	if (!is_power_of_2(size)) {
		fprintf(stderr, "size must be power of 2.\n");
		return ring_buf;
	}
	ring_buf = (struct ring_buffer *) malloc(sizeof(struct ring_buffer));
	//	aligned_node_t(sizeof(struct ring_buffer)));
	if (!ring_buf) {
		fprintf(stderr, "Failed to malloc memory,errno:%u,reason:%s", errno,
				strerror(errno));
		return ring_buf;
	}
	memset(ring_buf, 0, sizeof(struct ring_buffer));
	ring_buf->buffer = buffer;
	ring_buf->size = size;
	ring_buf->in = 0;
	ring_buf->out = 0;
	ring_buf->f_lock = f_lock;
	return ring_buf;
}
//release buffer
void ring_buffer_free(struct ring_buffer *ring_buf) {
	if (ring_buf) {
		if (ring_buf->buffer) {
			free(ring_buf->buffer);
			ring_buf->buffer = NULL;
		}
		free(ring_buf);
		ring_buf = NULL;
	}
}

//buffer len
uint32_t __ring_buffer_len(const struct ring_buffer *ring_buf) {
	return (ring_buf->in - ring_buf->out);
}

//get data
uint32_t __ring_buffer_get(struct ring_buffer *ring_buf, void * buffer,
		uint32_t size) {
	assert(ring_buf || buffer);
	uint32_t len = 0;
	size = min(size, ring_buf->in - ring_buf->out);
	/* first get the data from fifo->out until the end of the buffer */
	smp_rmb();
	len = min(size, ring_buf->size - (ring_buf->out & (ring_buf->size - 1)));
	memcpy(static_cast<char *>(buffer),
			static_cast<char *>(ring_buf->buffer)
					+ (ring_buf->out & (ring_buf->size - 1)),
			len);
	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(static_cast<char *>(buffer) + len,
			static_cast<char *>(ring_buf->buffer), size - len);
	smp_mb();
	ring_buf->out += size;
	return size;
}
//put data
uint32_t __ring_buffer_put(struct ring_buffer *ring_buf, void *buffer,
		uint32_t size) {
	assert(ring_buf || buffer);
	uint32_t len = 0;
	size = min(size, ring_buf->size - ring_buf->in + ring_buf->out);
	smp_mb();
	/* first put the data starting from fifo->in to buffer end */
	len = min(size, ring_buf->size - (ring_buf->in & (ring_buf->size - 1)));
	memcpy(
			static_cast<char *>(ring_buf->buffer)
					+ (ring_buf->in & (ring_buf->size - 1)),
			static_cast<char *>(buffer),
			len);
	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(static_cast<char *>(ring_buf->buffer),
			static_cast<char *>(buffer) + len, size - len);
	smp_wmb();
	ring_buf->in += size;
	return size;
}
//
uint32_t ring_buffer_len(const struct ring_buffer *ring_buf) {
	uint32_t len = 0;
	S_LOCK(ring_buf->f_lock);
	len = __ring_buffer_len(ring_buf);
	S_UNLOCK(ring_buf->f_lock);
	return len;
}
//for muti thread or process
uint32_t ring_buffer_get(struct ring_buffer *ring_buf, void *buffer,
		uint32_t size) {
	uint32_t ret;
	S_LOCK(ring_buf->f_lock);
	ret = __ring_buffer_get(ring_buf, buffer, size);
	//buffer中没有数据
	if (ring_buf->in == ring_buf->out)
		ring_buf->in = ring_buf->out = 0;
	S_UNLOCK(ring_buf->f_lock);
	return ret;
}
//for muti thread or process
uint32_t ring_buffer_put(struct ring_buffer *ring_buf, void *buffer,
		uint32_t size) {
	uint32_t ret;
	S_LOCK(ring_buf->f_lock);
	ret = __ring_buffer_put(ring_buf, buffer, size);
	S_UNLOCK(ring_buf->f_lock);
	return ret;
}
#endif
