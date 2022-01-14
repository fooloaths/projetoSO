#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 80
#define SIZE 256
#define LOOP_SIZE 20

typedef struct {
    const char *path;
    const char *input;
    size_t input_size;
} thread_args;

void successful_test() {
    printf("\033[0;32m");
    printf("Successful test\n");
    printf("\033[0m");
}

void* thread_func(void* arg) {
    /* Open again to check if contents are as expected */
    thread_args *args = (thread_args*) arg;
    int fd = tfs_open(args->path, TFS_O_CREAT);
    size_t input_size = args->input_size;
    char const *input = args->input;
    assert(fd != -1);
    char output[input_size];

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(fd, output, input_size) == input_size);
        assert(memcmp(input, output, input_size) == 0);
    }

    assert(tfs_close(fd) != -1);
    pthread_exit(NULL);
}


int main() {
    char *paths[LOOP_SIZE];
    for (int i = 0; i < LOOP_SIZE; i++) {
        char path[100];
        sprintf(path, "/f%d", i);
        paths[i] = strdup(path);
    }

    const size_t input_size = (size_t) (rand () % SIZE) + 1;
    char input[input_size]; 
    for (int i = 0; i < input_size; i++) {
        int r = rand() % (122 - 65 + 1) + 65;
        input[i] = (char) r;
    }

    assert(tfs_init() != -1);

    for (int i = 0; i < LOOP_SIZE; i++) {
        /* Write input COUNT times into a new file */
        int fd = tfs_open(paths[i], TFS_O_CREAT);
        assert(fd != -1);
        for (int k = 0; k < COUNT; k++) {
            assert(tfs_write(fd, input, input_size) == input_size);
        }
        assert(tfs_close(fd) != -1);
    }

    pthread_t threads[LOOP_SIZE];
    
    thread_args args[LOOP_SIZE];
    for (int i = 0; i < LOOP_SIZE; i++) {
        args[i].path = paths[i];
        args[i].input = input;
        args[i].input_size = input_size;
    }

    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void *) &args[i]);
    }

    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    successful_test();
    return 0;
}
