// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

#define USE_MY_PTHREAD 1
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include <assert.h>
#define STACKSIZE 8 * 1024
#define MAXTHREADS 20
#define MEMORY_SIZE 8 * 1024 * 1024
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define THREAD_REQ 1
#define LIBRARY_REQ 2

// Globals
typedef uint my_pthread_t;
char* memory = memalign(PAGE_SIZE, MEMORY_SIZE);     // 8MB memory
char* mem_head = memory;
char* mem_iter = mem_head;
char* kernel_head = memory[4096*1024*1024];         // Pointer at second set of 4MB
char* kernel_iter = kernel_head;
char* shared_head = memory[8372224];                // 8MB - 16kB
char* shared_iter = shared_head;
size_t rem_shared_space = 16*1024;

// 32 bytes
typedef struct Node{
    int size;       // 4
    char* data;     // 8
    struct Node* next; // 8
    bool valid;     // 1
}node,*node_ptr;

node_ptr sh_list_head = NULL;

char* page_table[2048] = {NULL};

char* free_list[2048] = {NULL};
char** free_head = free_list;
char** free_tail = &free_list[2047];

typedef struct threadContextNode{
    int size;
    char* data;
    struct threadContextNode* next;
}threadNode, *threadNode_ptr;

typedef struct threadControlBlock {
  // Thread related params
  my_pthread_t thread_id;
  ucontext_t thread_context;
  int isActive;
  int isExecuted;
  int isBlocked;
  int isMain;
  struct threadControlBlock *next;
  struct blockedThreadList *blockedThreads;

  // Memory related params
  // Total page size for a thread equals 4KB
  char* next_alloc;                     // Next available location
  int rem_space;
  int page_id;
  node_ptr head;
  char* char_iter;

} tcb, *tcb_ptr;

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
  int lock;
  int count;
  volatile my_pthread_t owner;
} my_pthread_mutex_t;

typedef struct threadQueue {
  tcb_ptr head;
  tcb_ptr tail;
  long count;
}*thread_Queue;

typedef struct blockedThreadList {
  tcb_ptr thread;
  struct blockedThreadList *next;
}*blockedThreadList_ptr;

typedef struct finishedThread {
  my_pthread_t thread_id;
  void **returnValue;
  struct finishedThread *next;
}*finishedThread_ptr;

typedef struct finishedControlBlockQueue {
  struct finishedThread *thread;
  long count;
}*finished_Queue;

tcb_ptr getControlBlock_Main();
tcb_ptr getControlBlock();
tcb_ptr getCurrentBlockByThread(thread_Queue,my_pthread_t);
tcb_ptr getCurrentBlock(thread_Queue queue);
int getQueueSize(thread_Queue queue);
thread_Queue getQueue();
void freeControlBlock(tcb_ptr);
int next(thread_Queue);
int enqueueToCompletedList(finished_Queue,finishedThread_ptr);
finishedThread_ptr getFinishedThread(finished_Queue,my_pthread_t,int);
blockedThreadList_ptr getBlockedThreadList();
int addToBlockedThreadList(tcb_ptr,tcb_ptr);
finishedThread_ptr getCompletedThread();
finished_Queue getFinishedQueue();
void* myallocate(int size, char* file_num, int line_num, int alloc_flag);
void mydeallocate(void* ptr, char* file_num, int line_num, int dealloc_flag);

/* Function Declarations: */

// init process
void my_pthread_init(long period);

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);


#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
//#define malloc(x) myallocate(x, __FILE__, __LINE__, THREAD_REQ)
//#define free(x) mydeallocate(x, __FILE__, __LINE__, THREAD_REQ)
#endif

#endif
