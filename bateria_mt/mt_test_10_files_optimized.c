#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 256
#define LOOP_SIZE 20

/**
   This test fills in a new file up to 10 blocks via multiple writes, 
   each write always targeting only 1 block of the file, 
   then checks if the file contents are as expected
 */

void successful_test() {
    printf("\033[0;32m");
    printf("Successful test\n");
    printf("\033[0m");
}

void* thread_func1(void* arg) {
    size_t input_size = (size_t) (rand () % SIZE) + 1;
    char input[input_size]; 

    for (int i = 0; i < input_size; i++) {
        int r = rand() % (122 - 65 + 1) + 65;
        input[i] = (char) r;
    }



    char* path = (char*) arg;
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, input_size) == input_size);
    }
    assert(tfs_close(fd) != -1);
    pthread_exit(NULL);
}

void* thread_func2(void* arg) {
    size_t input_size = (size_t) (rand () % SIZE) + 1;
    char input[input_size]; 

    for (int i = 0; i < input_size; i++) {
        int r = rand() % (122 - 65 + 1) + 65;
        input[i] = (char) r;
    }

    char* path = (char*) arg;
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, input_size) == input_size);
    }
    assert(tfs_close(fd) != -1);
    pthread_exit(NULL);
}


int main() {    
    
    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    
    assert(tfs_init() != -1);

    pthread_t t1, t2;
    char *paths[LOOP_SIZE];
    for (int i = 0; i < LOOP_SIZE; i++) {
        char path[100];
        sprintf(path, "/f%d", i);
        paths[i] = strdup(path);
    }

    pthread_create(&t1, NULL, thread_func1,(void*)paths);
    pthread_create(&t2, NULL, thread_func2,(void*)paths);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    successful_test();
    return 0;
}