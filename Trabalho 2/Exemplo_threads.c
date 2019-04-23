#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

pthread_t t1,t2,t3;

void main_t1(void) {
    while(1) {
        printf("thread 1\n");
        sleep(1);
    }
}

void main() {
    printf("Thread 1");

    pthread_create(&t1, NULL, (void *) main_t1, NULL);
}
