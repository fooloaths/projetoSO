#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 256
#define LOOP_SIZE 20

typedef struct {
    char *path;
    char *path2;
    char *str;
} thread_args;

void successful_test() {
    printf("\033[0;32m");
    printf("Successful test\n");
    printf("\033[0m");
}

void* thread_func(void* arg) {
    thread_args *args = (thread_args*) arg;
    int file = tfs_open(args->path, TFS_O_CREAT);
    assert(file != -1);
    assert(tfs_write(file, args->str, strlen(args->str)) != -1);
    assert(tfs_close(file) != -1);
    assert(tfs_copy_to_external_fs(args->path, args->path2) != -1);
    pthread_exit(NULL);
}

void* thread_func2(void* arg) {
    char to_read[SIZE];
    thread_args *args = (thread_args*) arg;
    FILE *fp = fopen(args->path2, "r");
    assert(fp != NULL);
    assert(fread(to_read, sizeof(char), strlen(args->str), fp) == strlen(args->str));
    assert(strcmp(args->str, to_read) == 0);
    assert(fclose(fp) != -1);
    unlink(args->path2);
    pthread_exit(NULL);
}

int main() {
    const size_t input_size = (size_t) (rand () % SIZE) + 1;
    char str[input_size]; 
    for (int i = 0; i < input_size; i++) {
        int r = rand() % (122 - 65 + 1) + 65;
        str[i] = (char) r;
    }

    assert(tfs_init() != -1);  
    pthread_t threads[LOOP_SIZE + 1];

    char *path1 = "/f1";

    char *paths2[LOOP_SIZE];
    for (int i = 0; i < LOOP_SIZE; i++) {
        char path[100];
        sprintf(path, "f%d.txt", i);
        paths2[i] = strdup(path);
    }

    /*write to external file */
    thread_args args[LOOP_SIZE];
    for (int i = 0; i < LOOP_SIZE; i++) {
        args[i].path = path1;
        args[i].path2 = paths2[i];
        args[i].str = str;
    }

    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void *) &args[i]);
    }
    
    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }
    /* check external file*/
    
    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void *) &args[i]);
    }
    
    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }
    
    successful_test();
    return 0;
}