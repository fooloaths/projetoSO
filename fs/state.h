#ifndef STATE_H
#define STATE_H

#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define DIRECT_BLOCKS_COUNT 10

/*
 * Directory entry
 */
typedef struct {
    char d_name[MAX_FILE_NAME];
    int d_inumber;
} dir_entry_t;

typedef enum { T_FILE, T_DIRECTORY } inode_type;

/*
 * I-node
 */
typedef struct {
    inode_type i_node_type;
    size_t i_size;
    int *i_data_block;
    size_t number_of_blocks;
    int indirection_block;
    size_t number_indirect_blocks;
    pthread_rwlock_t i_lock;
    /* in a real FS, more fields would exist here */
} inode_t;

typedef enum { FREE = 0, TAKEN = 1 } allocation_state_t;

/*
 * Open file entry (in open file table)
 */
typedef struct {
    int of_inumber;
    size_t of_offset;
    pthread_rwlock_t of_lock;
} open_file_entry_t;


#define MAX_DIR_ENTRIES (BLOCK_SIZE / sizeof(dir_entry_t))

void state_init();
void state_destroy();

int get_free_memory();

int inode_alloc_first_block(int inumber);
int inode_create(inode_type n_type);
int inode_free_direct_blocks(inode_t *inode);
int inode_free_indirect_blocks(inode_t *inode);
int inode_delete(int inumber);
inode_t *inode_get(int inumber);
int inode_is_free(int inumber);
int inode_inicialize_direct_blocks(inode_t *inode, size_t start, size_t end);
int write_index_to_block(inode_t *inode, int block);
int inode_inicialize_indirect_blocks(inode_t *inode, size_t start, size_t end);
size_t inode_compute_required_blocks(size_t size_to_be_added, size_t offset, inode_t *inode);
int inode_add_blocks(int inumber, size_t sizeToBeAdded, size_t offset);
int inode_invalid_indirect_block(inode_t *inode, size_t block_number);
ssize_t inode_write(open_file_entry_t *file, inode_t *inode, void const *buffer, size_t to_write);
ssize_t inode_read(open_file_entry_t *file, inode_t *inode, void *buffer, size_t to_read);

int clear_dir_entry(int inumber, int sub_inumber);
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name);
int find_in_dir(int inumber, char const *sub_name);

int data_block_alloc();
int data_block_free(int block_number);
void *data_block_get(int block_number);

int add_to_open_file_table(int inumber, size_t offset);
int remove_from_open_file_table(int fhandle);
open_file_entry_t *get_open_file_entry(int fhandle);
#endif // STATE_H