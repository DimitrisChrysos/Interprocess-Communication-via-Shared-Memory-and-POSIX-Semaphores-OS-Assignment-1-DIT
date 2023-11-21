#define TEXT_SZ 2048

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>

struct shared_use_st {
    char bufferA[TEXT_SZ];
    char bufferB[TEXT_SZ];
    char* point_to_local_bufferA;
    char* point_to_local_bufferB;
    int sh_running;
    sem_t wait_A_receive;
    sem_t wait_B_receive;
};

void *send_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        if (shared_stuff->sh_running == 0)
            break;
        printf("Enter some text: ");
        fgets(shared_stuff->point_to_local_bufferA, BUFSIZ, stdin);

        // strncpy(shared_stuff->bufferA, shared_stuff->point_to_local_bufferA, TEXT_SZ);
        //
        strncpy(shared_stuff->bufferA, shared_stuff->point_to_local_bufferA, TEXT_SZ);
        //
        
        if (strncmp(shared_stuff->bufferA, "end", 3) == 0) {
            shared_stuff->sh_running = 0;
            sem_post(&shared_stuff->wait_A_receive);
        }
        sem_post(&shared_stuff->wait_B_receive);
    }
    pthread_exit("Thank you for the chat!");
}

void *receive_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        sem_wait(&shared_stuff->wait_A_receive);
        if (shared_stuff->sh_running == 0)
            break;
        printf("\nYour friend wrote: %s", shared_stuff->bufferB);
        if (strncmp(shared_stuff->bufferB, "end", 3) == 0) {
            shared_stuff->sh_running = 0;
        }
    }

    pthread_exit("Thank you for the chat!");
}

int main()
{   
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	char buffer[BUFSIZ];
	int shmid;
	shmid = shmget((key_t)999978, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	printf("Shared memory segment with id %d attached at %p\n", shmid, shared_memory);

	shared_stuff = (struct shared_use_st *)shared_memory;
    shared_stuff->point_to_local_bufferA = buffer;
    shared_stuff->sh_running = 1;
    sem_init(&shared_stuff->wait_A_receive, 1, 0);


    /* Create and Join the two Threads */
    int res;
    pthread_t a_thread, b_thread;

    // Call the receive_thread
    res = pthread_create(&b_thread, NULL, receive_thread, (void *)shared_stuff);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    // Call the send_thread
    res = pthread_create(&a_thread, NULL, send_thread, (void *)shared_stuff);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    // Join the receive_thread
    res = pthread_join(b_thread, NULL);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }

    // Join the send_thread
    res = pthread_join(a_thread, NULL);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }



	if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}