#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 80
#define SIZE 256
#define LOOP_SIZE 20

typedef struct {
    const char *path;
    char *input;
    size_t input_size;
} thread_args;

void successful_test() {
    printf("\033[0;32m");
    printf("Successful test\n");
    printf("\033[0m");
}

void* thread_read(void* arg) {
    /* Open again to check if contents are as expected */
    thread_args *args = (thread_args*) arg;
    int fd = tfs_open(args->path, 0);
    size_t input_size = args->input_size;
    char *input = args->input;
    assert(fd != -1);
    char output[input_size];

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(fd, output, input_size) == input_size);
        assert(memcmp(input, output, input_size) == 0);
    }

    assert(tfs_close(fd) != -1);
    pthread_exit(NULL);
}

void* thread_delete(void *arg) {
    thread_args *args = (thread_args *) arg;
    int fd = tfs_open(args->path, 0);   //Get file handle
    assert(fd != -1);

    open_file_entry_t *file = get_open_file_entry(fd);
    assert(file != NULL);

    int inumber = file->of_inumber;
    assert(inode_delete(inumber) != -1);

    pthread_exit(NULL);
}


int main() {
    const char *path = "/f1";

    char* input = "chiquinho pila louca";
    size_t input_size = strlen(input);

    assert(tfs_init() != -1);

    /* Write input COUNT times into a new file */
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, input_size) == input_size);
    }
    assert(tfs_close(fd) != -1);
    pthread_t threads[LOOP_SIZE];

    thread_args args;
    args.path = path;
    args.input = input;
    args.input_size = input_size;

    for (int i = 0; i < LOOP_SIZE; i++) {
        if (i == (LOOP_SIZE / 2)) {
            pthread_create(&threads[i], NULL, thread_delete, (void *) &args);
        }
        else {
            pthread_create(&threads[i], NULL, thread_read, (void *) &args);
        }
    }

    for (int i = 0; i < LOOP_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    successful_test();
    return 0;
}
