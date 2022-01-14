#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            pthread_rwlock_wrlock(&inode->i_lock);
            if (inode->i_size > 0) {
                if (inode->indirection_block != -1) {
                    /* Has indirect data blocks */
                    if (inode_free_indirect_blocks(inode) == -1) {
                        pthread_rwlock_unlock(&inode->i_lock);
                        return 1;
                    }
                }
                if (inode_free_direct_blocks(inode) == -1) {
                    pthread_rwlock_unlock(&inode->i_lock);
                    return -1;
                }
            }
            inode->i_data_block = (int *) realloc(inode->i_data_block, sizeof(int));
            if (inode->i_data_block == NULL) {
                pthread_rwlock_unlock(&inode->i_lock);
                return -1;
            }
            if (inode_alloc_first_block(inum) == -1) {
                pthread_rwlock_unlock(&inode->i_lock);
                return -1;
            }
            pthread_rwlock_unlock(&inode->i_lock);
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            pthread_rwlock_rdlock(&inode->i_lock);
            offset = inode->i_size;
            pthread_rwlock_unlock(&inode->i_lock);
        } else {
            offset = 0;
        }
    } 
    else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } 
    else {
        return -1;
    }
    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    open_file_entry_t *file = get_open_file_entry(fhandle);
    ssize_t bytes_written = 0;
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Write the information on the open file's corresponding inode */
    bytes_written = inode_write(file, inode, buffer, to_write);
    if (bytes_written == -1) {
        return -1;
    }
    return bytes_written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    open_file_entry_t *file = get_open_file_entry(fhandle);
    ssize_t bytes_read;
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    } 


    bytes_read = inode_read(file, inode, buffer, len);
    if (bytes_read == -1) {
        return -1;
    }
    return bytes_read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    int fhandleSource;

    //Passing 0 as a flag to tfs_open, opens the file with the offset at 0
    if ((fhandleSource = tfs_open(source_path, 0)) == -1) {
        //Source file doesn't exist
        return -1;
    }

    open_file_entry_t *file = get_open_file_entry(fhandleSource);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    void *buffer = malloc(inode->i_size);
    if (buffer == NULL) { //Out of memory
        return -1;
    }
    memset(buffer, 0, inode->i_size);

    FILE *destFile = fopen(dest_path, "w");
    if (destFile == NULL) {
        return -1;
    }

    ssize_t readBytes = tfs_read(fhandleSource, buffer, inode->i_size);
    if (readBytes == -1) {  //Read from file in TFS
        return -1;
    }

    size_t writeBytes = fwrite(buffer, 1, (size_t) readBytes, destFile);
    if (writeBytes != readBytes) {
        return -1;
    }
    
    int close = fclose(destFile);
    if (close == EOF) {
        return -1;
    }
    
    if ((fhandleSource = tfs_close(fhandleSource)) == -1) {
            return -1;
    }
    free(buffer);

    return 0;
}
