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
    printf("\nStatistics for Process B:\n");
    printf("ProcB send %d messages\n", stats.count_msgs_B_send);
    printf("ProcB received %d messages\n", stats.count_msgs_A_send);
    printf("ProcB send %d packages\n", stats.count_packets_B_send);
    printf("ProcB received %d packages\n", stats.count_packets_A_send);
    long int a = stats.count_packets_B_send;
    long int b = stats.count_msgs_B_send;
    float average = (float)a / (float)b;
    printf("ProcB send %f packages per message\n", average);
    a = stats.waiting_first_msg_counterB;
    b = stats.count_msgs_B_send;
    average = (float)a / (float)b;
    printf("Average waiting time to receive the first package of a new message: %f microseconds\n\n", average);
}

void *send_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        if (shared_stuff->sh_running == 0)
            break;
        printf("Enter some text: ");
        fgets(shared_stuff->point_to_local_bufferB, BUFSIZ, stdin);

        int counter=0;
        strncpy(shared_stuff->bufferB, shared_stuff->point_to_local_bufferB + counter, SH_MEM_BUFF_SZ-1);
        if (strncmp(shared_stuff->bufferB, "end", 3) == 0) {
            shared_stuff->sh_running = 0;
            sem_post(&shared_stuff->wait_B_receive);
            sem_post(&shared_stuff->wait_A_receive);
            break;
        }
        while (1) {
            if (shared_stuff->bufferB[13] == '\0') {

                //  End of sending message
                shared_stuff->constracting_msg_A = 0;
                shared_stuff->stats.count_packets_B_send++;


                if (shared_stuff->first_msg_packetA) {
                    gettimeofday(&shared_stuff->stats.cur_timeB, NULL);
                }


                sem_post(&shared_stuff->wait_A_receive);
                sem_wait(&shared_stuff->constr_msg_A);
                counter = 0;
                break;
            }

            if (shared_stuff->first_msg_packetA) {
                gettimeofday(&shared_stuff->stats.cur_timeB, NULL);
            }

            //  Start sending message
            shared_stuff->constracting_msg_A = 1;
            shared_stuff->stats.count_packets_B_send++;
            sem_post(&shared_stuff->wait_A_receive);
            sem_wait(&shared_stuff->constr_msg_A);

            counter += SH_MEM_BUFF_SZ-1;
            strncpy(shared_stuff->bufferB, shared_stuff->point_to_local_bufferB + counter, SH_MEM_BUFF_SZ-1);
        }
    }
    pthread_exit("Thank you for the chat!");
}

void *receive_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;
    while (shared_stuff->sh_running) {
        sem_wait(&shared_stuff->wait_B_receive);
        if (shared_stuff->sh_running == 0)
            break;
        if (shared_stuff->constracting_msg_B == 1) {
            if (shared_stuff->first_msg_packetB == 1) {
                
                
                struct timeval cur_time1;
                gettimeofday(&cur_time1, NULL);
                shared_stuff->stats.waiting_first_msg_counterB += cur_time1.tv_usec- shared_stuff->stats.cur_timeA.tv_usec;
                

                strncpy(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferB[14] = '\0';
                shared_stuff->first_msg_packetB = 0;
            }
            else {
                strcat(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA);
            }
            sem_post(&shared_stuff->constr_msg_B);
        }
        else {
            if (shared_stuff->first_msg_packetB == 1) {
                
                
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);
                shared_stuff->stats.waiting_first_msg_counterB += cur_time.tv_usec- shared_stuff->stats.cur_timeA.tv_usec;

                
                strncpy(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferB[14] = '\0';
                shared_stuff->first_msg_packetB = 0;
            }
            else {
                strcat(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA);
            }
            shared_stuff->stats.count_msgs_A_send++;
            printf("\nYour friend wrote: %s", shared_stuff->point_to_local_bufferB);
            shared_stuff->first_msg_packetB = 1;
            sem_post(&shared_stuff->constr_msg_B);
            if (strncmp(shared_stuff->bufferA, "end", 3) == 0) {
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
	srand((unsigned int)getpid());
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
    shared_stuff->point_to_local_bufferB = buffer;
    shared_stuff->sh_running = 1;
    shared_stuff->constracting_msg_B = 0;
    shared_stuff->first_msg_packetB = 1;
    shared_stuff->stats.count_msgs_B_send = 0;
    shared_stuff->stats.count_packets_B_send = 0;
    shared_stuff->stats.waiting_first_msg_counterB = 0;
    sem_init(&shared_stuff->wait_B_receive, 1, 0);
    sem_init(&shared_stuff->constr_msg_B, 1, 0);

    
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
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		fprintf(stderr, "shmctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
