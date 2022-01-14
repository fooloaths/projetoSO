#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 256
#define LOOP_SIZE 50

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

int main() {

    char *path = "/f1";

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    char input[SIZE]; 
    for (int i = 0; i < SIZE; i++) {
        int r = rand() % (122 - 65 + 1) + 65;
        input[i] = (char) r;
    }

    int fds[LOOP_SIZE];

    assert(tfs_init() != -1);



    /* Write input COUNT times into a new file */
    for (int i = 0; i < LOOP_SIZE; i++) {
        fds[i] = tfs_open(path, TFS_O_CREAT);
        assert(fds[i] != -1);
        assert(tfs_write(fds[i], input, SIZE) == SIZE);
        for (int k = 0; k < COUNT; k++) {
            assert(tfs_write(fds[i], input, SIZE) == SIZE);
        }
        assert(tfs_close(fds[i]) != -1);
    }

    successful_test();
    return 0;
}