#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#define NUM_OF_CELL (8192) //must power of 2
int ring_buffer[NUM_OF_CELL] = {0};
unsigned int in = 0;
unsigned int out = 0;
/*
As the mutex lock is stored in global (static) memory it can be 
    initialized with PTHREAD_MUTEX_INITIALIZER.
    If we had allocated space for the mutex on the heap, 
    then we would have used pthread_mutex_init(ptr, NULL)
*/
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 
sem_t countsem; 
sem_t spacesem;
#define FILE_NAME "heap.log"
FILE *log_file;

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
        //printf("Enqueue the value %d\n", *(int *)value);
        ring_buffer[(in++) & (NUM_OF_CELL - 1)] = *(int *)value;
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
        out_value = ring_buffer[(out++) & (NUM_OF_CELL - 1)];
        pthread_mutex_unlock(&lock);

        // Increment the count of the number of spaces
        sem_post(&spacesem);
        return out_value;
}

void *writer(void *ptr)
{
    int i = 0;
    int count = *(int *)ptr;
    for (i = 0; i < count; ++i)
    {
        enqueue(&i);
    }
    return NULL;
}

void *reader(void *ptr)
{
    int loop;
    loop = *(int *)ptr;
    while(loop > 0)
    {
        fprintf(log_file,"%d\n", dequeue());
        loop--;
    }
    fflush(log_file);
    fclose(log_file);
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