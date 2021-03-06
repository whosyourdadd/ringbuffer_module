#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h> 
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/kern_return.h>
#endif


#define NUM_OF_RING                     (1)
#define NUM_OF_CELL                     (128) //must power of 2
#define FILE_NAME                       "log.csv"
#define GET_RINGBUFF_CELL_IDX(idx)      ((idx) & (NUM_OF_CELL - 1))
#define IDX_METHOD_ENABLE               0                        

struct ringbuff_cell {
        struct timespec timestamp;
        uint32_t curr_heap_size;
};

struct ringbuff_body {
        struct ringbuff_cell cell[NUM_OF_CELL];
        uint64_t writer_idx;
        uint64_t reader_idx;
};

struct ring_group {
    struct ring_group           *next;
    struct ringbuff_body        *ring;
};

/*
As the mutex lock is stored in global (static) memory it can be 
    initialized with PTHREAD_MUTEX_INITIALIZER.
    If we had allocated space for the mutex on the heap, 
    then we would have used pthread_mutex_init(ptr, NULL)
*/
pthread_mutex_t ring_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
static struct ringbuff_body g_bodies[NUM_OF_RING]; 

int ring_body_idx = 0;
FILE *log_file;
struct timespec start, end_writer, end_reader; 

void clock_get_hw_time(struct timespec *ts){
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
  clock_gettime(CLOCK_MONOTONIC_COARSE, ts);
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
        log_file = fopen(FILE_NAME,"w");
        setvbuf(log_file, NULL, _IONBF, 0);
}

void enqueue(void *value)
{
        struct ringbuff_body *r; 
        pthread_mutex_lock(&ring_lock);
        
        r = &g_bodies[ring_body_idx];

        if (r->writer_idx == r->reader_idx - 1)
        {
            //pthread_mutex_unlock(&ring_lock);
            pthread_cond_wait(&cond, &ring_lock);
        }
        r->cell[GET_RINGBUFF_CELL_IDX(r->writer_idx)] = *(struct ringbuff_cell *)value;
        r->writer_idx++;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&ring_lock);
        
}

void* dequeue(void)
{
        struct ringbuff_body *r;
        pthread_mutex_lock(&ring_lock);
        r = &g_bodies[ring_body_idx];
        if (r->writer_idx == r->reader_idx) //ring buffer is empty
        {
            pthread_cond_wait(&cond, &ring_lock);
        }

            fprintf(log_file,"%ld.%9ld, %d\n", (long)r->cell[GET_RINGBUFF_CELL_IDX(r->reader_idx)].timestamp.tv_sec 
                                         , r->cell[GET_RINGBUFF_CELL_IDX(r->reader_idx)].timestamp.tv_nsec
                                         , r->cell[GET_RINGBUFF_CELL_IDX(r->reader_idx)].curr_heap_size);
        r->reader_idx++;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&ring_lock);

        return NULL;
}

void *writer(void *ptr)
{
        int in = 0;
        int count = *(int *)ptr;
        struct ringbuff_cell temp;
        for (in = 0; in < count; ++in)
        {
            clock_get_hw_time(&temp.timestamp);
            temp.curr_heap_size = in;
            enqueue(&temp);
        }
        clock_get_hw_time(&end_writer);
        return NULL;
}

void *reader(void *ptr)
{
        int out;
        out = *(int *)ptr;
        while (out > 0)
        {
            dequeue();
            out--;
        }
        fflush(log_file);
        fclose(log_file);
        clock_get_hw_time(&end_reader);
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


        printf("Input value %d\n", input_value);
        clock_get_hw_time(&start);
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