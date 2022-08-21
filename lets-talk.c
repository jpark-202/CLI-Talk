#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include "list.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>

#define BUFFER_SIZE 4002

List * inputList;
List * outputList;
int socketFileDescriptor;
int socketFileDescriptor2;

struct addrinfo hints, *info, *p;
struct addrinfo inHints, *inInfo, *inP;
struct sockaddr_storage their_addr;

pthread_mutex_t outputLock, inputLock;
pthread_cond_t outputCond, inputCond;

struct threads {
    int socketFileDescriptor;
    int scoketFileDescriptor2;
    struct sockaddr_storage outAddress;
    struct addrinfo * inInfo, *inP, *outP;
    List * inputList;
    List * outputList;
};

struct threads t;

char *encryption(char* buffer, int status){
    if(status == 1){
        for(int i = 0; i < strlen(buffer); i++){
            if(buffer[i] != '\0' && buffer[i] != '\n'){
                buffer[i] += 1;
            }
        }
    } else if (status == 0){
        for(int i = 0; i < strlen(buffer); i++){
            if(buffer[i] != '\0' && buffer[i] != '\n'){
                buffer[i] -= 1;
            }
        }
    }
    return buffer;
}
void *keyboard_input(void * empty) {
    struct threads *t = empty;
    char *buffer = (char*)malloc(BUFFER_SIZE * sizeof(char));
    while(1) {
        if(fgets(buffer, BUFFER_SIZE, stdin)){
            if(strcmp(buffer, "!exit\n") == 0) {
                free(buffer);
                List_free(inputList, free);
                List_free(outputList, free);
                List_free(t->inputList, free);
                List_free(t->outputList, free);
                exit(0);
            }
			List_add(t->inputList, (char *)buffer);
        }
    } 
    free(buffer);
    pthread_exit(NULL);
}
void *receive_data(void * empty) {
    struct threads *t = empty;
    char *buffer = (char*)malloc(BUFFER_SIZE * sizeof(char));
    while(1) {
        socklen_t addr_len;
        int rv;
        pthread_mutex_lock(&outputLock);
        addr_len = sizeof t->outAddress;
        if ((rv = recvfrom(t->socketFileDescriptor, buffer, 4000 , 0,
            (struct sockaddr *)&(t->outAddress), &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        buffer[rv] = '\0';
        encryption(buffer, 0);
        List_add(t->outputList, (char *) buffer);
        pthread_cond_signal(&outputCond);
		pthread_mutex_unlock(&outputLock); 
    }
    free(buffer);
    pthread_exit(NULL);

}
void *print_data(void * empty) {
    struct threads *t = empty;
    char *buffer = (char*)malloc(BUFFER_SIZE * sizeof(char));

    while(1) {
        if(List_count(t->outputList)) {
            buffer = List_remove(t->outputList);  
            printf("%s", buffer);
            fflush(stdout);
        }
    }
    free(buffer);
    pthread_exit(NULL);

}
void *send_data(void * empty) {
    struct threads *t = empty;
    char *buffer = (char*)malloc(BUFFER_SIZE * sizeof(char));

    while(1) {
        if(List_count(t->inputList)) {
            buffer = List_remove(t->inputList);
            encryption(buffer, 1);
            int rv;
            if ((rv = sendto(t->scoketFileDescriptor2, buffer, strlen(buffer), 0,
                (t->inP)->ai_addr, (t->inP)->ai_addrlen)) == -1) {
                exit(1);
            }
            encryption(buffer, 0); 
        }
    }
    free(buffer);
    pthread_exit(NULL);

}


int main (int argc, char ** argv) 
{

    int res2;
    int res1;

    memset(&inHints, 0, sizeof inHints);
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET6; 
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; 
    inHints.ai_family = AF_INET6; 
    inHints.ai_socktype = SOCK_DGRAM;

    if ((res1 = getaddrinfo(NULL, argv[1], &hints, &info)) != 0) {
        freeaddrinfo(info);
        fprintf(stderr, "error");
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = info; p != NULL; p = p->ai_next) {
        if ((socketFileDescriptor = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            fprintf(stderr, "error");
            continue;
        }

        if (bind(socketFileDescriptor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketFileDescriptor);
            fprintf(stderr, "error");
            continue;
        }

        break;
    }
    t.outP = p;
    t.outAddress = their_addr;
    t.socketFileDescriptor = socketFileDescriptor;

    if (p == NULL) {
        freeaddrinfo(info);
        fprintf(stderr, "error");
        return 1;
    }
    if ((res2 = getaddrinfo(argv[2], argv[3], &inHints, &inInfo)) != 0) {
        freeaddrinfo(inInfo);
        fprintf(stderr, "error");        
        return 1;
    }

    for(inP = inInfo; inP != NULL; inP = inP->ai_next) {
        if ((socketFileDescriptor2 = socket(inP->ai_family, inP->ai_socktype,
                inP->ai_protocol)) == -1) {
            fprintf(stderr, "error");
            continue;
        }

        break;
    }

    if (inP == NULL) {
        freeaddrinfo(inInfo);
        fprintf(stderr, "error");
    }

    t.inP = inP;
    t.inInfo = inInfo;
    t.scoketFileDescriptor2 = socketFileDescriptor2;

    inputList = List_create();
    outputList = List_create();

    t.inputList = inputList;
    t.outputList = outputList;

    pthread_t receiving_thr, printer_thr, sending_thr, keyboard_thr;
    printf("Welcome to lets-talk! Please type your message now\n");
    pthread_create(&keyboard_thr, NULL, keyboard_input, &t);
    pthread_create(&sending_thr, NULL, send_data, &t);
    pthread_create(&receiving_thr, NULL, receive_data, &t);
    pthread_create(&printer_thr, NULL, print_data, &t);

    pthread_join(keyboard_thr, NULL);
    pthread_join(sending_thr, NULL);
    pthread_join(receiving_thr, NULL);
    pthread_join(printer_thr, NULL);

    freeaddrinfo(info);
    freeaddrinfo(inInfo);

    List_free(inputList, free);
    List_free(outputList, free);
    List_free(t.inputList, free);
    List_free(t.outputList, free);
    
    pthread_exit(0);
}
