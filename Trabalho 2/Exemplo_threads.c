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

void main_t2(void) {
    while(1) {
        printf("thread 22\n");
        sleep(2);
    }
}

void main() {
    printf("Thread 1");

    pthread_create(&t1, NULL, (void *) main_t1, NULL);

    pthread_create(&t2, NULL, (void *) main_t2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Fim");
}

