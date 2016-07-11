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


#define NUM_OF_RING                     (2)
#define NUM_OF_CELL                     (64) //must power of 2
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
static struct ringbuff_body *g_ring;
static struct ringbuff_body g_bodies[NUM_OF_RING]; 

int ring_body_idx = 0;
sem_t *spacesem;
#if !(__MACH__)
sem_t g_spacesem;
#endif
FILE *log_file;
struct timespec start, end_writer, end_reader; 

void clock_get_monotonic_time(struct timespec *ts){
#if __MACH__
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
  clock_gettime(CLOCK_REALTIME, ts);
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
#if __MACH__
        sem_unlink("ring_space_protect");
        spacesem = sem_open("ring_space_protect", O_CREAT, 0700, NUM_OF_RING);
#else
        spacesem = &g_spacesem;
        sem_init(g_spacesem, 0, 0);
#endif

        log_file = fopen(FILE_NAME,"w");
        setvbuf(log_file, NULL, _IONBF, 0);
}

void enqueue(void *value)
{
        struct ringbuff_body *r; 
        pthread_mutex_lock(&ring_lock);
        
        r = &g_bodies[ring_body_idx];
        r->cell[r->writer_idx++] = *(struct ringbuff_cell *)value;

        if (r->writer_idx == NUM_OF_CELL) {
            (spacesem);
            g_ring = r;
            ring_body_idx = (++ring_body_idx) & NUM_OF_RING;
            sem_post(spacesem);
        }
        pthread_mutex_unlock(&ring_lock);
}

void* dequeue(void)
{
        uint32_t cell_idx;
        pthread_mutex_lock(&ring_lock);
        for (cell_idx = 0; cell_idx < g_ring->writer_idx; cell_idx++)
        {
            struct ringbuff_cell *cell = &g_ring->cell[cell_idx];

            fprintf(log_file,"%ld.%9ld, %d\n", (long)cell->timestamp.tv_sec 
                                         , cell->timestamp.tv_nsec
                                         , cell->curr_heap_size);
        }

        memset(g_ring, 0, sizeof(*g_ring));
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
            clock_get_monotonic_time(&temp.timestamp);
            temp.curr_heap_size = in;
            enqueue(&temp);
        }
        clock_get_monotonic_time(&end_writer);
        return NULL;
}

void *reader(void *ptr)
{
        int out;
        out = *(int *)ptr;
        while (1)
        {
            if (sem_wait(spacesem) < 0) {
               assert(errno == 0);
            }
            dequeue();

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