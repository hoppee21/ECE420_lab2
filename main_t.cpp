#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "common.h"
#include "timer.h"


pthread_rwlock_t rwlock;
char **theArray;


double *times;
int timeBool = 1;
pthread_mutex_t timerMutex;
int length = 0;

void initializeTimeArray (int array_size){
    times = (double*) malloc(array_size * sizeof (double));
}


void initializeArray(int array_size) {
    theArray = (char **) malloc(array_size * sizeof(char*));
    int i;
    for (i = 0; i < array_size; i++) {
        theArray[i] = (char *) malloc(COM_BUFF_SIZE * sizeof(char));  // Adjust size as needed
        sprintf(theArray[i], "String %d: the initial value", i);
    }
}

void *ServerEcho(void *args)
{
    ClientRequest req;

    // First, cast args to intptr_t
    intptr_t temp = (intptr_t)args;

    // Then, cast it to int (assuming int can hold the file descriptor)
    int clientFileDescriptor = (int)temp;

    char str[COM_BUFF_SIZE];

    read(clientFileDescriptor,str,COM_BUFF_SIZE);
    ParseMsg(str, &req);

    // timer start components
    double start, end, elapsed;
    if (timeBool == 1){

        GET_TIME(start);
    }

    if(req.is_read){
        pthread_rwlock_rdlock(&rwlock);
        getContent(req.msg, req.pos, theArray);
        pthread_rwlock_unlock(&rwlock);
    } else {
        pthread_rwlock_wrlock(&rwlock);
        setContent(req.msg, req.pos, theArray);
        getContent(req.msg, req.pos, theArray);
        pthread_rwlock_unlock(&rwlock);
    }

    // timer end components
    if (timeBool == 1){
        GET_TIME(end);
        elapsed = end - start;
        pthread_mutex_lock(&timerMutex);
        times[length] = elapsed;
        length ++;
        if (length >= 1000) {
            //save average time
            saveTimes(times, length);
            length = 0;
        }
        pthread_mutex_unlock(&timerMutex);
    }

    if (write(clientFileDescriptor, req.msg, COM_BUFF_SIZE) == -1) {
        perror("Write failed");
        close(clientFileDescriptor);
        return NULL;
    }

    close(clientFileDescriptor);

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage: %s <Array Size> <Server IP> <Server Port>\n", argv[0]);
        return 1;
    }

    int arraySize = strtol(argv[1], NULL, 10);
    char *serverIP = argv[2];
    int serverPort = strtol(argv[3], NULL, 10);

    // Initialize the array with the given size
    initializeArray(arraySize);

    if (timeBool == 1){
        // Initialize the time array
        initializeTimeArray(1000);
        pthread_mutex_init(&timerMutex, NULL);
    }

    // Initialize the read-write lock
    pthread_rwlock_init(&rwlock, NULL);

    // Setup server socket
    int serverFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFileDescriptor < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in sock_var;
    memset(&sock_var, 0, sizeof(sock_var));
    sock_var.sin_addr.s_addr = inet_addr(serverIP);
    sock_var.sin_port = serverPort;
    sock_var.sin_family = AF_INET;

    if (bind(serverFileDescriptor, (struct sockaddr*)&sock_var, sizeof(sock_var)) < 0) {
        perror("Bind failed");
        close(serverFileDescriptor);
        return 1;
    }

    if (listen(serverFileDescriptor, 2000) < 0) {
        perror("Listen failed");
        close(serverFileDescriptor);
        return 1;
    }

    printf("Server started on %s:%d\n", serverIP, serverPort);

    pthread_t threads[COM_NUM_REQUEST];
    int clientFileDescriptor;
    int threadCount;

    fd_set fdSet;
    struct timeval timeout;
    int selectResult;

    while (1) {

        FD_ZERO(&fdSet); // Clear the socket set
        FD_SET(serverFileDescriptor, &fdSet); // Add the server socket to the set

        // Set timeout to 10 seconds
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        // Wait for an activity on the server socket, timeout after 10 seconds
        selectResult = select(serverFileDescriptor + 1, &fdSet, NULL, NULL, &timeout);

        if (selectResult == -1) {
            perror("select error");
            break;
        } else if (selectResult == 0) {
            printf("Timeout occurred! No client connection after 10 seconds.\n");
            break; // Break out of the loop if timeout occurs
        }

        for (threadCount = 0; threadCount < COM_NUM_REQUEST; threadCount++) {
            clientFileDescriptor = accept(serverFileDescriptor, NULL, NULL);
            if (clientFileDescriptor < 0) {
                perror("Accept failed");
                continue;
            }

            if (pthread_create(&threads[threadCount], NULL, ServerEcho, (void *) (long) clientFileDescriptor) != 0) {
                perror("Thread creation failed");
                close(clientFileDescriptor);
                continue;
            }

        }

        // Wait for all threads to finish
        long j;
        for (j = 0; j < threadCount; j++) {
            pthread_join(threads[j], NULL);
        }
    }


    //free the time list
    free(times);

    // Cleanup
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&timerMutex);
    int i;
    for (i = 0; i < arraySize; i++) {
        free(theArray[i]);
    }
    free(theArray);

    close(serverFileDescriptor);
    return 0;
}