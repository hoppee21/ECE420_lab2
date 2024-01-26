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


pthread_rwlock_t rwlock;
char **theArray;

void initializeArray(int array_size) {
    theArray = malloc(array_size * sizeof(char*));
    int i;
    for (i = 0; i < array_size; i++) {
        theArray[i] = malloc(COM_BUFF_SIZE * sizeof(char));  // Adjust size as needed
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

    pthread_t threads[4*COM_NUM_REQUEST];
    int clientFileDescriptor;
    int threadCount = 0;

    while (threadCount < 4*COM_NUM_REQUEST) {
        clientFileDescriptor = accept(serverFileDescriptor, NULL, NULL);
        if (clientFileDescriptor < 0) {
            perror("Accept failed");
            continue;
        }

        if (pthread_create(&threads[threadCount], NULL, ServerEcho, (void *)(long)clientFileDescriptor) != 0) {
            perror("Thread creation failed");
            close(clientFileDescriptor);
            continue;
        }

        threadCount++;
    }

    // Wait for all threads to finish
    long j;
    for (j = 0; j < threadCount; j++) {
        pthread_join(threads[j], NULL);
    }

    // Cleanup
    pthread_rwlock_destroy(&rwlock);
    for (j = 0; j < arraySize; j++) {
        free(theArray[j]);
    }
    free(theArray);

    close(serverFileDescriptor);
    return 0;
}
