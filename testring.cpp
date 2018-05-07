/**
 * zhaoxi
 * */
#include "ringbuffer.h"
#include <pthread.h>
//#include<thread>
#include <time.h>
#include"lock.h"
#include <unistd.h>
#define BUFFER_SIZE  1024 * 1024

typedef struct student_info {
	uint64_t stu_id;
	uint32_t age;
	uint32_t score;
} student_info;

void print_student_info(const student_info *stu_info) {
	assert(stu_info);
	printf("id:%lu\t", stu_info->stu_id);
	printf("age:%u\t", stu_info->age);
	printf("score:%u\n", stu_info->score);
}

student_info * get_student_info(time_t timer) {
	student_info *stu_info = (student_info *) malloc(sizeof(student_info));
	if (!stu_info) {
		fprintf(stderr, "Failed to malloc memory.\n");
		return NULL;
	}
	srand(timer);
	stu_info->stu_id = 10000 + rand() % 9999;
	stu_info->age = rand() % 30;
	stu_info->score = rand() % 101;
	print_student_info(stu_info);
	return stu_info;
}

int main() {
	void * buffer = NULL;
	uint32_t size = 0;
	struct ring_buffer *ring_buf = NULL;
	pthread_t consume_pid, produce_pid;

	slock_t *f_lock = (slock_t *) malloc(sizeof(slock_t));
	S_INIT_LOCK(f_lock);
	buffer = (void *) malloc(BUFFER_SIZE);
	if (!buffer) {
		fprintf(stderr, "Failed to malloc memory.\n");
		return -1;
	}
	size = BUFFER_SIZE;
	ring_buf = ring_buffer_init(buffer, size, f_lock);
	if (!ring_buf) {
		fprintf(stderr, "Failed to init ring buffer.\n");
		return -1;
	}
#if 0
	student_info *stu_info = get_student_info(638946124);
	ring_buffer_put(ring_buf, (void *)stu_info, sizeof(student_info));
	stu_info = get_student_info(976686464);
	ring_buffer_put(ring_buf, (void *)stu_info, sizeof(student_info));
	ring_buffer_get(ring_buf, (void *)stu_info, sizeof(student_info));
	print_student_info(stu_info);
#endif
#ifdef C++11_
	thread thr1([]() {
		for (int i = 0;i < 1000;i++) {
			time_t cur_time;
			time (&cur_time);
			srand(cur_time);
			int seed = rand() % 11111;
			student_info *stu_info = get_student_info(cur_time + seed);
			printf("put a student info to ring buffer.\n");
			ring_buffer_put(ring_buf, (void *) stu_info, sizeof(student_info));
		}
	});

	thread thr2([]() {
		for (int i = 0;i < 1000;i++) {
			ring_buffer_get(ring_buf, (void *) &stu_info, sizeof(student_info));
		}
	});

	thr1.join();
	thr2.join();
#endif
	while (1) {
		time_t cur_time;
		time (&cur_time);
		srand(cur_time);
		int seed = rand() % 11111;
		printf("******************************************\n");
		student_info *stu_info = get_student_info(cur_time + seed);
		printf("put a student info to ring buffer.\n");
		__ring_buffer_put(ring_buf, (void *) stu_info, sizeof(student_info));
		printf("ring buffer length: %u\n", ring_buffer_len(ring_buf));
		printf("******************************************\n");

		printf("------------------------------------------\n");
		printf("get a student info from ring buffer.\n");
		__ring_buffer_get(ring_buf, (void *) &stu_info, sizeof(student_info));
		printf("ring buffer length: %u\n", ring_buffer_len(ring_buf));
		printf("------------------------------------------\n");
	}
	ring_buffer_free(ring_buf);
	free(f_lock);
	return 0;
}
