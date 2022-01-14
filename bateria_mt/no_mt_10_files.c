#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

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

int main() {
    
    assert(tfs_init() != -1);
    char input[SIZE];
    for (int i = 0; i < SIZE; i++) {
        int r = rand() % (122 - 70) + 66;
        input[i] = (char) r;
    }

    for (int i = 1; i <= LOOP_SIZE; i++) {
        char path[10];
        sprintf(path, "/f%d", i);
        int fd = tfs_open(path, TFS_O_CREAT);
        assert(fd != -1);
        for (int j = 0; j < COUNT; j++) {
            assert(tfs_write(fd, input, SIZE) == SIZE);
        }
        assert(tfs_close(fd) != -1);
        fd = tfs_open(path, 0);
        assert(fd != -1 );

        for (int j = 0; j < COUNT; j++) {
            assert(tfs_read(fd, input, SIZE) == SIZE);
            assert (memcmp(input, input, SIZE) == 0);
        }

        assert(tfs_close(fd) != -1);
    }

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    
    successful_test();
    return 0;
}