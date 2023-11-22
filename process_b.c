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

/* Struct used to print statistics of the process at the end */
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

/* Struct used for Shared Memory*/
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
    int procA_exited;
    int procB_exited;
};

/* Prints the statistics of the Process */
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

/* Thread used to send a message to the other Process */
void *send_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;

    // Creates a loop to send as many messages as needed
    while (shared_stuff->sh_running) {

        // If program stops running break and exit thread
        if (shared_stuff->sh_running == 0)
            break;

        // Take user input
        printf("Enter some text: ");
        fgets(shared_stuff->point_to_local_bufferB, BUFSIZ, stdin);

        // We break the message from the user into smaller packages, each consisting 
        // of 14 chars from the original message and '\0'
        int counter=0;
        strncpy(shared_stuff->bufferB, shared_stuff->point_to_local_bufferB + counter, SH_MEM_BUFF_SZ-1);
        if (strncmp(shared_stuff->bufferB, "#BYE#", 5) == 0) {

            // If user input == "#BYE#" Process stops running
            shared_stuff->procB_exited = 1;
            shared_stuff->sh_running = 0;
            sem_post(&shared_stuff->wait_B_receive);
            sem_post(&shared_stuff->wait_A_receive);
            break;
        }
        while (1) {
            
            // If we have a message smaller than 14 chars or we are sending the 
            // last package of a message
            if (shared_stuff->bufferB[13] == '\0') {

                // Notify that we are no logner constracting a message for B
                shared_stuff->constracting_msg_A = 0;

                // For Statistics
                shared_stuff->stats.count_packets_B_send++;
                if (shared_stuff->first_msg_packetA) {
                    gettimeofday(&shared_stuff->stats.cur_timeB, NULL);
                }

                // Break the loop and continue to the original loop
                sem_post(&shared_stuff->wait_A_receive);
                sem_wait(&shared_stuff->constr_msg_A);
                counter = 0;
                break;
            }

            // If we have a message larger than 14 chars
            // For Statistics
            if (shared_stuff->first_msg_packetA) {
                gettimeofday(&shared_stuff->stats.cur_timeB, NULL);
            }

            // Send each Packet
            shared_stuff->constracting_msg_A = 1;
            shared_stuff->stats.count_packets_B_send++;
            sem_post(&shared_stuff->wait_A_receive);
            sem_wait(&shared_stuff->constr_msg_A);

            // For the next packet
            counter += SH_MEM_BUFF_SZ-1;
            strncpy(shared_stuff->bufferB, shared_stuff->point_to_local_bufferB + counter, SH_MEM_BUFF_SZ-1);
        }
    }
    pthread_exit("Thank you for the chat!");
}

/* Thread used to receive a message from the other Process */
void *receive_thread(void *shared_st) {
    struct shared_use_st *shared_stuff = shared_st;

    // Creates a loop to receive as many messages as needed
    while (shared_stuff->sh_running) {

        // Wait for the other Process to send a message, or if the same Process
        // terminates the program with "#BYE#" 
        sem_wait(&shared_stuff->wait_B_receive);
        
        // If program stops running break and exit thread 
        if (shared_stuff->sh_running == 0)
            break;

        // If we are currently constracting a message that is send by the other Process
        // Else print the message
        if (shared_stuff->constracting_msg_B) {
            
            // If we are receiving the first packet of a message use strcpy to initialize 
            // the local buffer, else use strcat to add to the current string
            if (shared_stuff->first_msg_packetB) {
                
                // For Statistics
                struct timeval cur_time1;
                gettimeofday(&cur_time1, NULL);
                shared_stuff->stats.waiting_first_msg_counterB += cur_time1.tv_usec- shared_stuff->stats.cur_timeA.tv_usec;
                
                // Add the first packet
                strncpy(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferB[14] = '\0';
                shared_stuff->first_msg_packetB = 0;
            }
            else {
                // Add the next packet
                strcat(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA);
            }

            // Continue constracting the message from the other Process
            sem_post(&shared_stuff->constr_msg_B);
        }
        else {

            // If we are not constracting a message anymore, or the message was under 14 chars,
            // we print the message

            // If we are receiving the first packet of a message use strcpy to initialize 
            // the local buffer, else use strcat to add to the current string
            if (shared_stuff->first_msg_packetB) {
                
                // For statistics
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);
                shared_stuff->stats.waiting_first_msg_counterB += cur_time.tv_usec- shared_stuff->stats.cur_timeA.tv_usec;

                // Add the first packet
                strncpy(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA, SH_MEM_BUFF_SZ-1);
                shared_stuff->point_to_local_bufferB[14] = '\0';
                shared_stuff->first_msg_packetB = 0;
            }
            else {
                // Add the next packet
                strcat(shared_stuff->point_to_local_bufferB, shared_stuff->bufferA);
            }

            // For Statistics
            shared_stuff->stats.count_msgs_A_send++;
            
            // Print the message that was received and initialize again the thread 
            // to receive a new message
            printf("\nYour friend wrote: %s", shared_stuff->point_to_local_bufferB);
            shared_stuff->first_msg_packetB = 1;
            
            // Make send thread from the other process able to get a new user input
            sem_post(&shared_stuff->constr_msg_B);
            
            // If user input was "#BYE#", exit the thread
            if (strncmp(shared_stuff->bufferA, "#BYE#", 5) == 0) {
                shared_stuff->sh_running = 0;
            }
        }
    }
    pthread_exit("Thank you for the chat!");
}

int main()
{
    // Create/Open shared memory
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
    char buffer[BUFSIZ];
	int shmid;
	srand((unsigned int)getpid());
	shmid = shmget((key_t)999961, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}

    // Initialize values of items in Shared Memory
	shared_stuff = (struct shared_use_st *)shared_memory;
    shared_stuff->point_to_local_bufferB = buffer;
    shared_stuff->sh_running = 1;
    shared_stuff->constracting_msg_B = 0;
    shared_stuff->first_msg_packetB = 1;
    shared_stuff->stats.count_msgs_B_send = 0;
    shared_stuff->stats.count_packets_B_send = 0;
    shared_stuff->stats.waiting_first_msg_counterB = 0;
    shared_stuff->procB_exited = 0;
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

    // We cancel the send thread if it is stack at fgets() after 
    // all other threads have been joined
    if (shared_stuff->sh_running == 0 && shared_stuff->procA_exited == 1) {
        printf("procA exited\n");
        pthread_cancel(a_thread);
    }

    // Join the send_thread
    res = pthread_join(a_thread, NULL);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }

    // After the two threads have returned, we print some
    // statistics on the exit
    print_on_exit(shared_stuff->stats);

    // Detach Shared Memory
	if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}

    // Mark the Shared Memory segment to be destroyed
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		fprintf(stderr, "shmctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
