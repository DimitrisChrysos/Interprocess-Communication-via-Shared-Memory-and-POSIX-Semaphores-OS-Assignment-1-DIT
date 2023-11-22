#define TEXT_SZ 2048
#define SH_MEM_BUFF_SZ 15

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

struct statistics {
    int count_msgs_A_send;
    int count_msgs_B_send;
    int count_packets_A_send;
    int count_packets_B_send;
    struct timeval cur_timeA;
    struct timeval cur_timeB;
    long int waiting_first_msg_counterA;
    long int waiting_first_msg_counterB;
};

struct shared_use_st {
    char bufferA[SH_MEM_BUFF_SZ];
    char bufferB[SH_MEM_BUFF_SZ];
    char* point_to_local_bufferA;
    char* point_to_local_bufferB;
    int sh_running;
    sem_t wait_A_receive;
    sem_t wait_B_receive;
    int constracting_msg_A;
    int constracting_msg_B;
    sem_t constr_msg_A;
    sem_t constr_msg_B;
    int first_msg_packetA;
    int first_msg_packetB;
    struct statistics stats;
};

void print_on_exit(struct statistics stats) {
    printf("\nStatistics for Process A:\n");
    printf("ProcA send %d messages\n", stats.count_msgs_A_send);
    printf("ProcA received %d messages\n", stats.count_msgs_B_send);
    printf("ProcA send %d packages\n", stats.count_packets_A_send);
    printf("ProcA received %d packages\n", stats.count_packets_B_send);
    long int a = stats.count_packets_A_send;
    long int b = stats.count_msgs_A_send;
    float average = (float)a / (float)b;
    printf("ProcA send %f packages per message\n", average);
    a = stats.waiting_first_msg_counterA;
    b = stats.count_msgs_A_send;
    average = (float)a / (float)b;
    printf("Average waiting time to receive the first package of a new message: %f microseconds\n\n", average);
}

void *send_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        if (shared_stuff->sh_running == 0)
            break;
        printf("Enter some text: ");
        fgets(shared_stuff->point_to_local_bufferA, BUFSIZ, stdin);

        int counter=0;
        strncpy(shared_stuff->bufferA, shared_stuff->point_to_local_bufferA + counter, SH_MEM_BUFF_SZ-1);
        if (strncmp(shared_stuff->bufferA, "end", 3) == 0) {
            shared_stuff->sh_running = 0;
            sem_post(&shared_stuff->wait_A_receive);
            sem_post(&shared_stuff->wait_B_receive);
            break;
        }
        while (1) {
            if (shared_stuff->bufferA[13] == '\0') {

                //  End of sending message
                shared_stuff->constracting_msg_B = 0;
                shared_stuff->stats.count_packets_A_send++;


                if (shared_stuff->first_msg_packetB) {
                    gettimeofday(&shared_stuff->stats.cur_timeA, NULL);
                }
                
                
                sem_post(&shared_stuff->wait_B_receive);
                sem_wait(&shared_stuff->constr_msg_B);
                counter = 0;
                break;
            }

            if (shared_stuff->first_msg_packetB) {
                gettimeofday(&shared_stuff->stats.cur_timeA, NULL);
            }

            //  Start sending message
            shared_stuff->constracting_msg_B = 1;
            shared_stuff->stats.count_packets_A_send++;
            sem_post(&shared_stuff->wait_B_receive);
            sem_wait(&shared_stuff->constr_msg_B);

            counter += SH_MEM_BUFF_SZ-1;
            strncpy(shared_stuff->bufferA, shared_stuff->point_to_local_bufferA + counter, SH_MEM_BUFF_SZ-1);
        }
    }
    pthread_exit("Thank you for the chat!");
}

void *receive_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        sem_wait(&shared_stuff->wait_A_receive);
        if (shared_stuff->sh_running == 0)
            break;
        if (shared_stuff->constracting_msg_A == 1) {
            if (shared_stuff->first_msg_packetA == 1) {
                
                
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);
                shared_stuff->stats.waiting_first_msg_counterA += cur_time.tv_usec- shared_stuff->stats.cur_timeB.tv_usec;
                
                
                strncpy(shared_stuff->point_to_local_bufferA, shared_stuff->bufferB, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferA[14] = '\0';
                shared_stuff->first_msg_packetA = 0;
            }
            else {
                strcat(shared_stuff->point_to_local_bufferA, shared_stuff->bufferB);
            }
            sem_post(&shared_stuff->constr_msg_A);
        }
        else {
            if (shared_stuff->first_msg_packetA == 1) {
                
                
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);
                shared_stuff->stats.waiting_first_msg_counterA += cur_time.tv_usec- shared_stuff->stats.cur_timeB.tv_usec;


                strncpy(shared_stuff->point_to_local_bufferA, shared_stuff->bufferB, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferA[14] = '\0';
                shared_stuff->first_msg_packetA = 0;
            }
            else {
                strcat(shared_stuff->point_to_local_bufferA, shared_stuff->bufferB);
            }
            shared_stuff->stats.count_msgs_B_send++;
            printf("\nYour friend wrote: %s", shared_stuff->point_to_local_bufferA);
            shared_stuff->first_msg_packetA = 1;
            sem_post(&shared_stuff->constr_msg_A);
            if (strncmp(shared_stuff->bufferB, "end", 3) == 0) {
                shared_stuff->sh_running = 0;
            }
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
	shmid = shmget((key_t)999962, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
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
    shared_stuff->constracting_msg_A = 0;
    shared_stuff->first_msg_packetA = 1;
    shared_stuff->stats.count_msgs_A_send = 0;
    shared_stuff->stats.count_packets_A_send = 0;
    shared_stuff->stats.waiting_first_msg_counterA = 0;
    sem_init(&shared_stuff->wait_A_receive, 1, 0);
    sem_init(&shared_stuff->constr_msg_A, 1, 0);


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

    print_on_exit(shared_stuff->stats);

	if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}