#include "fs/operations.h"
#include <assert.h>
#include <string.h>

int main() {

    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    assert(tfs_init() != -1);

    int f;
    ssize_t r;

    /* Create File */

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    /* Write to file */

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    /* Close file */

    assert(tfs_close(f) != -1);

    /* Open file with offset at 0 */

    f = tfs_open(path, 0);
    assert(f != -1);

    /* Read from file */

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    /* Close file */

    assert(tfs_close(f) != -1);

    /* Open in truncate */

    f = tfs_open(path, TFS_O_TRUNC);
    assert(f != -1);

    /* Write to file */

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    /* Close file */

    assert(tfs_close(f) != -1);

    /* Open file with offset at 0*/

    f = tfs_open(path, 0);
    assert(f != -1);

    /* Read from file */

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    /* Close file */

    assert(tfs_close(f) != -1);

    /* Open in append */

    f = tfs_open(path, TFS_O_APPEND);
    assert(f != -1);

    /* Write to file */

    /*r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);*/

    /* Read from file */

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == 0);

    /* Close file */

    assert(tfs_close(f) != -1);

    printf("\033[0;32m");
    printf("Successful test\n");
    printf("\033[0m");

    return 0;
}
