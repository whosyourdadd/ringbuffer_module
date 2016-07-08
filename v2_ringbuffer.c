#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/kern_return.h>
#endif


#define NUM_OF_CELL (16384) //must power of 2
#define FILE_NAME "heap.log"
#define GET_RINGBUFF_CELL_IDX(idx) ((idx) & (NUM_OF_CELL - 1))
#define IDX_METHOD_ENABLE 0                      

struct ringbuff_cell {
        struct timespec timestamp;
        uint32_t curr_heap_size;
};

struct ringbuff_body {
        struct ringbuff_cell cell[NUM_OF_CELL];
        uint64_t writer_idx;
        uint64_t reader_idx;
};

struct ringbuff_body ring;

/*
As the mutex lock is stored in global (static) memory it can be 
    initialized with PTHREAD_MUTEX_INITIALIZER.
    If we had allocated space for the mutex on the heap, 
    then we would have used pthread_mutex_init(ptr, NULL)
*/
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 
sem_t countsem; 
sem_t spacesem;
FILE *log_file;
struct timespec start, end_writer, end_reader; 

void clock_get_monotonic_time(struct timespec *ts){
#ifdef __MACH__
    clock_serv_t cclock;
    mach_timespec_t mts;
    kern_return_t ret_val;
    if ((ret_val = host_get_clock_service(mach_host_self(),
                                          SYSTEM_CLOCK, &cclock) != KERN_SUCCESS))
        goto ret;

    ret_val = clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
ret:
    return;
#else
  clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

static double diff_in_second(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec - t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}


void ring_buffer_init()
{
        sem_init(&countsem, 0, 0);
        sem_init(&spacesem, 0, NUM_OF_CELL);
}

void enqueue(void *value)
{
        // wait if there is no space left:
        sem_wait( &spacesem );
        
        pthread_mutex_lock(&lock);
#if IDX_METHOD_ENABLE
        if (ring.writer_idx == ring.reader_idx - 1) //ring buffer is full
        {
            /* TODO Need to wait the consumer clean a cell*/
        } 
        else
        {
            ring.cell[GET_RINGBUFF_CELL_IDX(ring.writer_idx)] = *(struct ringbuff_cell *)value;
            ring.writer_idx ++;
        }
#else
       ring.cell[(ring.writer_idx++) & (NUM_OF_CELL - 1)] = *(struct ringbuff_cell *)value;
#endif /* IDX_METHOD_ENABLE */
        pthread_mutex_unlock(&lock);
        // increment the count of the number of items
        sem_post(&countsem);
}

void* dequeue(void)
{
        // Wait if there are no items in the buffer
        void *out_value = NULL;
        sem_wait(&countsem);

        pthread_mutex_lock(&lock);
#if IDX_METHOD_ENABLE       
        if (ring.writer_idx == ring.reader_idx) //ring buffer is empty
        {
            /* TODO Need to wait the producer enqueue the buffer*/
        } 
        else
        {
            fprintf(log_file,"%ld.%9lds %d\n", (long) ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].timestamp.tv_sec 
                                              , ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].timestamp.tv_nsec
                                              , ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].curr_heap_size);
            ring.reader_idx++;
        }
#else
        fprintf(log_file,"%ld.%9lds %d\n", (long) ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].timestamp.tv_sec 
                                          , ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].timestamp.tv_nsec
                                          , ring.cell[GET_RINGBUFF_CELL_IDX(ring.reader_idx)].curr_heap_size);
        ring.reader_idx++;
#endif /*IDX_METHOD_ENABLE*/
        pthread_mutex_unlock(&lock);

        // Increment the count of the number of spaces
        sem_post(&spacesem);
        return out_value;
}

void *writer(void *ptr)
{
        int i = 0;
        int count = *(int *)ptr;
        struct ringbuff_cell temp;
        for (i = 0; i < count; ++i)
        {
            clock_get_monotonic_time(&temp.timestamp);
            temp.curr_heap_size = i;
            enqueue(&temp);
        }
        clock_get_monotonic_time(&end_writer);
        return NULL;
}

void *reader(void *ptr)
{
        int spaceloop, countloop;
        //loop = *(int *)ptr;
        while(1)
        {
            dequeue();
            sem_getvalue(&spacesem,&spaceloop);
            sem_getvalue(&countsem,&countloop);
            if (spaceloop == NUM_OF_CELL && countloop == 0)
                break;
            //loop--;
        }
        fflush(log_file);
        fclose(log_file);
        clock_get_monotonic_time(&end_reader);
        printf("Writer elapsed time: %fs\n", diff_in_second(start, end_writer));
        printf("Reader elapsed time: %fs\n", diff_in_second(start, end_reader));
        return NULL;
}

int main(int argc, char const *argv[])
{
        /* TODO test ringbuffer */
        pthread_t tid1, tid2;
        int ret1, ret2, input_value;
        if (argc == 1) 
        {
                printf("Please enter parameter\n");
                return 0;
        }
        
        input_value = atoi(argv[1]);
        ring_buffer_init();
        log_file = fopen(FILE_NAME,"w");
        setvbuf(log_file, NULL, _IONBF, 0);


        printf("Input value %d\n", input_value);
        clock_get_monotonic_time(&start);
        ret1 = pthread_create(&tid1, NULL, writer,&input_value);
        ret2 = pthread_create(&tid2, NULL, reader,&input_value);
        if (ret1 && ret2)
        {
            printf("OK\n");
        }
        printf("pthread_create() for thread 1 \n");
        printf("pthread_create() for thread 2 \n");

        pthread_join(tid1, NULL);
        pthread_join(tid2, NULL);
        
        printf("All Finish. Ready exit\n");

        //pthread_exit(0);
        return 0;
}