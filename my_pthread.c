// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"
//#include <malloc.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

typedef struct {
  my_pthread_t thread_id;
  ucontext_t thread_context;
  int active;
  int executed;
  int exited;
  int isMain;
  void *ret;
}thread;

static bool first_create = true;
static int threadid;
ucontext_t common_context;
struct sigaction scheduler_interrupt_handler;
struct itimerval timeslice;
sigset_t signalMask;
void scheduler();
void *helper(void *(*function)(void*), void *arg);
thread_Queue queue = NULL;
finished_Queue finishedQueue = NULL;
tcb_ptr getCurrentControlBlock_Safe();
long millisec;


tcb_ptr getControlBlock_Main(){
  tcb_ptr controlBlock = (tcb_ptr)myallocate(sizeof(tcb), __FILE__, __LINE__, LIBRARY_REQ);
  controlBlock->thread_context.uc_stack.ss_flags = 0;
  controlBlock->thread_context.uc_link =0;
  controlBlock->isActive =0;
  controlBlock->isBlocked =0;
  controlBlock->isExecuted =0;
  controlBlock->isMain =1 ;
  controlBlock->next = NULL;
  controlBlock->page_id = -1;
  controlBlock->next_alloc = NULL;
  //controlBlock->rem_contig_space = PAGE_SIZE;
  controlBlock->rem_space = PAGE_SIZE;
  controlBlock->head = NULL;

  return controlBlock;
}

tcb_ptr getControlBlock(){
  tcb_ptr controlBlock = (tcb_ptr)myallocate(sizeof(tcb_ptr), __FILE__, __LINE__, LIBRARY_REQ);
  controlBlock->thread_context.uc_stack.ss_sp = myallocate(STACKSIZE, __FILE__, __LINE__, LIBRARY_REQ);
  controlBlock->thread_context.uc_stack.ss_size = STACKSIZE;
  controlBlock->thread_context.uc_stack.ss_flags = 0;
  controlBlock->thread_context.uc_link =0;
  controlBlock->isActive =0;
  controlBlock->isBlocked =0;
  controlBlock->isExecuted =0;
  controlBlock->isMain =0 ;
  controlBlock->next = NULL ;
  controlBlock->page_id = -1;
  controlBlock->next_alloc = NULL;
  //controlBlock->rem_contig_space = PAGE_SIZE;
  controlBlock->rem_space = PAGE_SIZE;
  controlBlock->head = NULL;

  return controlBlock;
}

int enqueue(thread_Queue queue,tcb_ptr tcb) {

  //check if queue or tcb is null
  //printf("Enqueing the thread\n");

  if(queue->head == NULL) {
    //this is the first node
    //printf("\nThis is first node\n");
    tcb->next= tcb;
    queue->head =tcb;
    queue->tail=tcb;
  }
  else {
    //printf("Not first\n");
    tcb->next =queue->head; //inserts tcb behinf the head in a circular queue
    queue->tail->next= tcb; //the existing tail should point to this tcb
    queue->tail =tcb; //the tail is the new tcb hence update it
  }
  queue ->count ++;

  return 0;
}

int dequeue(thread_Queue queue) {

  if(queue == NULL)
    return -1;
  else {
    //printf("\ndequeing blocks");
    tcb_ptr head,tail,temp;
    head = queue -> head;
    tail = queue -> tail;

    if(head != NULL) {
      temp = queue->head->next; //removing the head hence storing next block address in temp
      if(queue ->count ==1) {
	     queue->head = queue->tail= NULL;
      }
      else {
	     //printf("\n queue has more than 1 elements hence dequeing");
	     queue->head=temp; //temp is next block which is new head
	     tail->next=queue->head;  //tail next block is new head
      }
      freeControlBlock(head); //////free the old head
      //printf("\nFreed a block on queue");
      queue->count--;
    }
    else {
      return 0;
    }

  }
  return 0;
}

void freeControlBlock(tcb_ptr controlBlock) {
  //if(!(controlBlock->isMain))
    //free(controlBlock->thread_context.uc_stack.ss_sp);

  //free(controlBlock);
}

int next(thread_Queue queue){

  if(queue!= NULL) {
    tcb_ptr current = queue -> head;
    if(current != NULL) {
      queue->tail = current;
      queue->head=current->next;
    }
  }
  //printf("\n Returning from next");
  return 0;
}

tcb_ptr getCurrentBlock(thread_Queue queue){

  if(queue !=NULL && queue->head != NULL) {
    //printf("\n Returning CurrentBlock\n");
    return queue->head;
  }
  return NULL;
}

tcb_ptr getCurrentBlockByThread(thread_Queue queue,my_pthread_t threadid) {
  tcb_ptr headBlock = getCurrentBlock(queue);
  //if this is the required node
  if(headBlock!=NULL && headBlock->thread_id == threadid)
    return headBlock;
  tcb_ptr dummyThread=NULL;
  if(headBlock!=NULL)
    dummyThread = headBlock->next;

  while((headBlock != dummyThread)) {
    if(dummyThread ->thread_id == threadid)
      return dummyThread;

    dummyThread = dummyThread->next;
  }
  return NULL;
}

int getQueueSize(thread_Queue queue) {

  return queue->count;
}

thread_Queue getQueue() {

  thread_Queue queue = (thread_Queue)myallocate(sizeof(struct threadQueue), __FILE__, __LINE__, LIBRARY_REQ);
  queue->count=0;
  queue->head=queue->tail= NULL;
  return queue;
}

int enqueueToCompletedList(finished_Queue queue,finishedThread_ptr finishedThread ) {
  if(queue != NULL && finishedThread !=NULL) {
    finishedThread->next=queue->thread;
    queue->thread = finishedThread;
  }
  return 0;
}

finishedThread_ptr getFinishedThread(finished_Queue queue,my_pthread_t thread_id,int flag) {

  if(queue!=NULL) {
    finishedThread_ptr thread= queue->thread;
    finishedThread_ptr previous_thread = NULL;
    while((thread!=NULL)&& (thread->thread_id!=thread_id)) {
      previous_thread =thread;
      thread = thread ->next;
    }
    if(flag && thread!=NULL) {
      if(previous_thread == NULL)
	     queue->thread  = thread->next;
      else
	     previous_thread->next = thread->next;
    }
    return thread;
  }

  return NULL;
}

blockedThreadList_ptr getBlockedThreadList() {

  blockedThreadList_ptr newList = (blockedThreadList_ptr)myallocate(sizeof(struct blockedThreadList), __FILE__, __LINE__, LIBRARY_REQ);
  if(newList!=NULL) {
    newList->thread=NULL;
    newList->next=NULL;
  }
  return newList;
}

int addToBlockedThreadList(tcb_ptr fromNode,tcb_ptr toNode ) {

  blockedThreadList_ptr list = getBlockedThreadList();
  if(fromNode != NULL) {
    list->thread = toNode;
    list->next = fromNode->blockedThreads;
    fromNode->blockedThreads = list;
    toNode->isBlocked=1;
  }
  return 0;
}

finishedThread_ptr getCompletedThread() {
  finishedThread_ptr finishedThread = (finishedThread_ptr)myallocate(sizeof(struct finishedThread), __FILE__, __LINE__, LIBRARY_REQ);
  if(finishedThread == NULL) {
    return NULL;
  }
  finishedThread->returnValue = (void**)myallocate(sizeof(void*), __FILE__, __LINE__, LIBRARY_REQ);
  if(finishedThread->returnValue == NULL) {
    ////free(finishedThread);
    return NULL;
  }
  finishedThread->thread_id = -1;
  *(finishedThread->returnValue) = NULL;
  finishedThread->next = NULL;

  return finishedThread;
}

finished_Queue getFinishedQueue() {
  finished_Queue finishedQueue = (finished_Queue)myallocate(sizeof(struct finishedControlBlockQueue), __FILE__, __LINE__, LIBRARY_REQ);
  finishedQueue->thread = NULL;
  finishedQueue->count = 0;

  return finishedQueue;
}

void threadCompleted() {

  tcb_ptr currentNode = getCurrentControlBlock_Safe();
  blockedThreadList_ptr blockedThread = currentNode->blockedThreads;

  while(blockedThread != NULL)
  {
    blockedThread->thread->isBlocked =0;
    blockedThread = blockedThread->next;
  }

  //printf("\n Thread completed : %d",currentNode->thread_id );
  currentNode->isExecuted=1;
  raise(SIGVTALRM);
}

ucontext_t getCommonContext() {
  static int contextAlreadySet = 0;
  if(!contextAlreadySet)
  {
    getcontext(&common_context);
    common_context.uc_link = 0;
    common_context.uc_stack.ss_sp = myallocate(STACKSIZE, __FILE__, __LINE__, LIBRARY_REQ);
    common_context.uc_stack.ss_size = STACKSIZE;
    common_context.uc_stack.ss_flags= 0;
    makecontext( &common_context, (void (*) (void))&threadCompleted, 0);
    contextAlreadySet = 1;
  }
}

void scheduler(int signum, siginfo_t *si, void *unused){

    if(signum == 26 || signum == 28){
        int q_size = getQueueSize(queue);
        bool to_be_removed = 0;

        if(q_size == 1){
            if( getCurrentBlock(queue)->isExecuted ){
                // If current context has finished execution, dequeue
                dequeue(queue);
            }
        }
        else if(q_size > 1){

                tcb_ptr curr_context = getCurrentBlock(queue);

                if( curr_context != NULL ){
                    if( curr_context->isExecuted ){
                        to_be_removed = 1;
                        // dequeue
                        dequeue(queue);
                    }
                    else{
                        next(queue);
                    }

                    tcb_ptr next_context = getCurrentBlock(queue);

                    while( next_context != NULL && ( next_context->isBlocked || next_context->isExecuted ) ){
                        if( next_context->isExecuted ){
                            // dequeue
                            dequeue(queue);
                        }
                        else{
                            next(queue);
                        }

                        next_context = getCurrentBlock(queue);
                    }

                    if( next_context == NULL )
                        return;

                    if( next_context != curr_context ){
                        if( to_be_removed ){
                            // Set next thread as active, discard current thread
                            setcontext( &(next_context->thread_context) );
                        }
                        else{
                            // Swap current thread with next thread
                            swapcontext(&(curr_context->thread_context), &(next_context->thread_context) );
                            //mprotect(page_table[curr_context->page_id], PAGE_SIZE, PROT_NONE);  // protect swapped out context
                            //mprotect(page_table[next_context->page_id], PAGE_SIZE, PROT_READ | PROT_WRITE); // unprotect swapped in context
                        }
                    }

                }

            }
    }

    else if(signum == 11){
      // Memory manager
      int flag=0;
      int* address = si->si_addr;
      tcb_ptr  threadCB= getCurrentBlock(queue);
      for(int i = 0; i<=threadCB->count; i++){
        int* limit = threadCB->page_id[cnt];
        limit += 4*1024;
        if(address >= page_table[(threadCB->page_id[cnt])] || address < limit) {
        //page already exists -> swap file
          flag = 1;
          if()
        }
      }
      if(flag == 0){
        //allocate new page
        int i=0;
        char* temp;
        bool mem_iter_flag = false;
        if (mem_iter == kernel_head){
          if (free_list==NULL){
          }
          else{
              if(free_head == NULL){
                  free_head = free_list;
              }
              else{
                  temp = (*free_head);
                  *free_head = NULL;
                  if(free_head == free_tail)
                    free_head = free_list;
                  else
                    free_head++;
              }
          }
        }
        else{
            mem_iter_flag = true;
            //ctreating page node and incrementing mem_iter
            page_ptr nodule = (page_ptr) mem_iter;
            nodule->counter = 1;
            nodule->next= NULL;
            mem_iter+=sizeof(nodule);
            temp = mem_iter;
        }
        while(i<4096*1024/PAGE_SIZE){
            if(page_table[i] == NULL){
                page_table[i] = temp;
                if(mem_iter_flag)
                    mem_iter += (4*1024);
                //page[0]=i
                threadCB->count++;
                threadCB->page_id[threadCB->count]=i;
                nodule->th_id = threadCB->thread_id;
                break;
            }
            else
                i++;
        }
      }
    }
}

// init process
void my_pthread_init(long period){
  threadid = 1;
  sigemptyset(&signalMask);
  sigaddset(&signalMask, SIGVTALRM);
  //intializing the context of the scheduler
  finishedQueue = getFinishedQueue();
  queue = getQueue();
  millisec = period;
  tcb_ptr mainThread = getControlBlock_Main();
  mainThread->page_id = 0;
  //mainThread->head = &memory;
  page_ptr nodule = (page_ptr) mem_iter;
  nodule->counter = 1;
  nodule->next= NULL;
  mem_iter+=sizeof(nodule);
  page_table[0] = mem_iter;
  mem_iter += (4*1024);
  mainThread->page_id[0]=i;
  mainThread->count=1;
  //getcontext(&(MainThread->thread_context));
  //printf("in init \n");
  getCommonContext();
  mainThread->thread_context.uc_link = &common_context;
  mainThread->thread_id = threadid;
  nodule->th_id = mainThread->thread_id;
  enqueue(queue,mainThread);
  memset(&scheduler_interrupt_handler, 0, sizeof (scheduler_interrupt_handler));
  scheduler_interrupt_handler.sa_handler= &scheduler;
  sigaction(SIGVTALRM,&scheduler_interrupt_handler,NULL);
  millisec = period;
  timeslice.it_value.tv_sec = 0;
  timeslice.it_interval.tv_sec = 0;
  timeslice.it_value.tv_usec = millisec; // timer start decrementing from here to 0
  timeslice.it_interval.tv_usec = millisec; //timer after 0 resets to this value
  setitimer(ITIMER_VIRTUAL, &timeslice, NULL);
  //printf("Exiting init");
}

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
  if(first_create){
    first_create = false;
    my_pthread_init(25000);
  }

  int temp;
  if(queue != NULL) {
    sigprocmask(SIG_BLOCK,&signalMask,NULL);
    tcb_ptr  threadCB= getControlBlock_Main();
    getcontext((&threadCB->thread_context));
    threadCB->thread_context.uc_stack.ss_sp=myallocate(STACKSIZE, __FILE__, __LINE__, LIBRARY_REQ);
    threadCB->thread_context.uc_stack.ss_size=STACKSIZE;
    threadCB->thread_context.uc_stack.ss_flags=0;
    threadCB->isMain=0;
    threadCB->thread_context.uc_link = &common_context;
    //temp =rand();
    threadCB->thread_id= ++threadid;
    *thread = threadCB->thread_id;

    // Allocate 4kB entry in page table
    int i=0;
    char* temp;
    bool mem_iter_flag = false;
    if (mem_iter == kernel_head){
      if (free_list==NULL){
      }
      else{
          if(free_head == NULL){
              free_head = free_list;
          }
          else{
              temp = (*free_head);
              *free_head = NULL;
              if(free_head == free_tail)
                free_head = free_list;
              else
                free_head++;
          }
      }
    }
    else{
        mem_iter_flag = true;
        //ctreating page node and incrementing mem_iter
        page_ptr nodule = (page_ptr) mem_iter;
        nodule->counter = 1;
        nodule->next= NULL;
        mem_iter+=sizeof(nodule);
        temp = mem_iter;
    }
    while(i<4096*1024/PAGE_SIZE){
        if(page_table[i] == NULL){
            page_table[i] = temp;
            if(mem_iter_flag)
                mem_iter += (4*1024);
            //page[0]=i
            threadCB->page_id[0]=i;
            threadCB->count=1;
            nodule->th_id = threadCB->thread_id;
            break;
        }
        else
            i++;
    }

    makecontext(&(threadCB->thread_context),(void (*)(void))&helper,2,function,arg);

    //printf("Thread is created %d\n", *thread);
    enqueue(queue,threadCB);
    sigprocmask(SIG_UNBLOCK, &signalMask, NULL);
    sigemptyset(&(threadCB->thread_context.uc_sigmask));
    return 0;
  }
  //printf("Error: init() function not executed/n");
  return 0;
};

void *helper(void *(*function)(void*), void *arg){

  void *returnValue;
  tcb_ptr currentThread = getCurrentControlBlock_Safe();
  //printf("In Helper");
  returnValue = (*function)(arg);
  sigprocmask(SIG_BLOCK,&signalMask,NULL);
  finishedThread_ptr finishedThread = getCompletedThread();
  if(finishedThread != NULL) {
    *(finishedThread->returnValue) = returnValue;
    finishedThread->thread_id = currentThread->thread_id;
    enqueueToCompletedList(finishedQueue,finishedThread);
    int i=0;
      while(i<MEMORY_SIZE/PAGE_SIZE){
        if(free_list[i] == NULL){
          free_list[i] = page_table[currentThread->page_id];
          break;
        }
        else
          i++;
      }
      page_table[currentThread->page_id] = NULL;
  }
  sigprocmask(SIG_UNBLOCK,&signalMask,NULL);

  return returnValue;
  // set this value to the completed nodes return value
}

tcb_ptr getCurrentControlBlock_Safe() {

  tcb_ptr currentControlBlock = NULL;
  sigprocmask(SIG_BLOCK,&signalMask,NULL);
  currentControlBlock = getCurrentBlock(queue);
  sigprocmask(SIG_UNBLOCK,&signalMask,NULL);

  return currentControlBlock;
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
  //printf("\n-----Yield called-----\n");
  raise(SIGVTALRM);
  return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
  //printf("\n-----Exit called-----\n");
  sigprocmask(SIG_BLOCK,&signalMask,NULL);
  tcb_ptr currentThread = getCurrentBlock(queue);
  page_table[currentThread->page_id] = NULL;    // delete page table entry
  finishedThread_ptr finishedThread = getCompletedThread();
  if(finishedThread !=NULL && currentThread != NULL) {
    *(finishedThread->returnValue) = value_ptr;
    finishedThread->thread_id = currentThread->thread_id;
    enqueueToCompletedList(finishedQueue,finishedThread);
    int i=0;
      while(i<MEMORY_SIZE/PAGE_SIZE){
        if(free_list[i] == NULL){
          free_list[i] = page_table[currentThread->page_id];
          break;
        }
        else
          i++;
      }
      page_table[currentThread->page_id] = NULL;

  }
  threadCompleted();
  sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
  raise(SIGVTALRM);
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
  sigprocmask(SIG_BLOCK,&signalMask,NULL);
  tcb_ptr callingThread = getCurrentBlock(queue);
  tcb_ptr joinThread = getCurrentBlockByThread(queue,thread);

  //check if callingthread is blocking on itself or is null
  if(callingThread == NULL || callingThread == joinThread) {
    sigprocmask(SIG_UNBLOCK, &signalMask, NULL);
    return -1;
  }
  if(joinThread == NULL) {
    //The thread is finished hence can be found in finished Queue
    finishedThread_ptr finishedThread = getFinishedThread(finishedQueue,thread,1);
    sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
    if(finishedThread) {
      if(value_ptr)
	     *value_ptr =*(finishedThread->returnValue);
      ////free(finishedThread);
      return 0;
    }
    else
      return -1;
  }

  //printf("\n Value is %d :",(joinThread->blockedThreads==NULL));
  if(joinThread->blockedThreads == NULL) {
    addToBlockedThreadList(joinThread,callingThread);
    int isBlocked=callingThread->isBlocked;
    sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
    while(isBlocked){
      isBlocked=callingThread->isBlocked;
    }
    sigprocmask(SIG_BLOCK,&signalMask,NULL);
    finishedThread_ptr finishedThread = getFinishedThread(finishedQueue,thread,1);
    sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
    if(finishedThread != NULL && value_ptr != NULL) {
      if(value_ptr)
	     *value_ptr=*(finishedThread->returnValue);
      //free(finishedThread);
    }
    return 0;
    }
  else {
    sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
    return -1;
  }
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
  //printf("Mutex init \n");
  mutex->lock=0;
  mutex->owner =0;
  mutex->count=1;

  return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
  //printf("Mutex lock called \n");
  sigemptyset(&signalMask);
  sigaddset(&signalMask, SIGVTALRM);
  sigprocmask(SIG_BLOCK,&signalMask, NULL);
  tcb_ptr currentBlock = getCurrentBlock(queue);
  sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
  if(mutex->owner ==0 && (mutex->owner != currentBlock->thread_id) && mutex->lock==0) {
    while(mutex->count<=0);
    sigprocmask(SIG_BLOCK,&signalMask, NULL);
    mutex->count--;
    mutex->lock=1;
    mutex->owner = currentBlock->thread_id;
    sigprocmask(SIG_UNBLOCK,&signalMask, NULL);
    return 0;
  }
  else {
    //sigprocmask(SIG_BLOCK,&signalMask,NULL);
    while(1) {
      //printf("\n Spinning \n");
      if(mutex->owner==0)
	break;
    }
    //sigprocmask(SIG_UNBLOCK,&signalMask,NULL);
  }

  return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
  //printf("Mutex unlock called \n");
  sigprocmask(SIG_BLOCK,&signalMask,NULL);
  tcb_ptr currentThread = getCurrentBlock(queue);
  if(mutex->owner == currentThread->thread_id) {
    mutex->count++;
    mutex->lock=0;
    mutex->owner =0;
  }

  sigprocmask(SIG_UNBLOCK,&signalMask, NULL);

  return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
  //printf("Mutex destroy \n");
  mutex->lock = -1;
  mutex->owner = -1;
  mutex->count = -1;
	return 0;
};

void* myallocate(int size, char* file_num, int line_num, int alloc_flag){

    if(alloc_flag == 1){
        tcb_ptr block = getCurrentBlock(queue); // get 4kB block
        //node_ptr nodule = (node_ptr) char_iter;

        if(size > block->rem_space)
        	raise(SIGSEGV);
            // return NULL;

        if(block->head == NULL){
            char_iter = mem_iter;
            //node_iter = (node_ptr) mem_iter;
            node_ptr nodule = (node_ptr) char_iter;
            block->head = nodule;
            nodule->valid = 1;
            nodule->size = size;
            char_iter += sizeof(struct Node);
            nodule->data = char_iter;
            nodule->next = NULL;
            block->rem_space -= size;
            return nodule->data;
        }

        node_ptr iter = block->head;
        //node_ptr prev_node;

        while(iter != NULL){
          if(iter->valid){
            if(iter->next != NULL)
              iter = iter->next;
            else{
                // Current Valid and next is NULL
                if(size > block->rem_space){
                    return NULL;
                }
                char_iter = iter;
                char_iter += sizeof(struct Node) + iter->size;
                node_ptr nodule = (node_ptr) char_iter;
                iter->next = nodule;
                char_iter += sizeof(struct Node);
                nodule->data = char_iter;
                nodule->size = size;
                nodule->next = NULL;
                nodule->valid = 1;
                block->rem_space -= size;
                break;
            }
          }
          else{
              if(size == iter->size){
                  // Allocate and break
                  char_iter = iter;
                  node_ptr nodule = (node_ptr) char_iter;
                  char_iter += sizeof(struct Node);
                  nodule->data = char_iter;
                  nodule->size = size;
                  nodule->next = iter->next;
                  nodule->valid = 1;
                  block->rem_space -= size;
                  break;
              }
              else if(size < iter->size){
                // Allocate available space and make new pointer for remaining space
                char_iter = iter;
                node_ptr nodule = (node_ptr) char_iter;
                char_iter += sizeof(struct Node);
                nodule->data = char_iter;
                nodule->size = size;
                nodule->valid = 1;
                block->rem_space -= size;
                char_iter += size;
                node_ptr empty = (node_ptr) char_iter;
                nodule->next = empty;
                empty->size = iter->size - size;
                char_iter += sizeof(struct Node);
                empty->data = char_iter;
                empty->valid = 0;
                empty->next = iter->next;
                break;
            }
            else{
                // node invalid, requested size > available size
                iter = iter->next;
            }
          }
        }

        if(iter == NULL){
            return NULL;
        }

        return nodule->data;
    }
    else if(alloc_flag == 2){
        if(kernel_iter - &memory[0] == 8*1024*1024){
            return NULL;
        }
        char* ret_ptr = kernel_iter;
        kernel_iter += size;    // iterate through instead
        return ret_ptr;
    }
}

void mydeallocate(void* ptr, char* file_num, int line_num, int dealloc_flag){

    if(dealloc_flag == 1){
        // destroy nodes
        char* data = (char*) ptr;
        tcb_ptr block = getCurrentBlock(queue);
        node_ptr iter = block->head;
        bool null_flag = 0;
        while(iter->data != data){
            iter = iter->next;
            if(iter == NULL){
                null_flag = 1;
                break;
            }
        }
        if(!null_flag){
            iter->valid = 0;
            block->rem_space += iter->size;
        }
    }
    else if(dealloc_flag == 2){
        // destroy tcb and 4kb block
        *ptr = NULL;
    }

}
