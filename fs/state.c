#include "state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
pthread_rwlock_t inode_table_mutex = PTHREAD_RWLOCK_INITIALIZER;
static inode_t inode_table[INODE_TABLE_SIZE];
pthread_rwlock_t freeinode_ts_mutex = PTHREAD_RWLOCK_INITIALIZER;
static char freeinode_ts[INODE_TABLE_SIZE];

/* Data blocks */
pthread_rwlock_t fs_data_mutex = PTHREAD_RWLOCK_INITIALIZER;
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
pthread_rwlock_t free_blocks_mutex = PTHREAD_RWLOCK_INITIALIZER;
static char free_blocks[DATA_BLOCKS];

/* Volatile FS state */
static open_file_entry_t open_file_table[MAX_OPEN_FILES];
pthread_rwlock_t free_open_file_entries_mutex = PTHREAD_RWLOCK_INITIALIZER;
static char free_open_file_entries[MAX_OPEN_FILES];

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }
}

void state_destroy() {
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        if (freeinode_ts[i] == TAKEN) {
            inode_delete(i);
        }
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == TAKEN) {
            open_file_entry_t *file = get_open_file_entry(i);
            pthread_rwlock_destroy(&file->of_lock);
            remove_from_open_file_table(i);
        }
    }

    pthread_rwlock_destroy(&inode_table_mutex);
    pthread_rwlock_destroy(&freeinode_ts_mutex);
    pthread_rwlock_destroy(&fs_data_mutex);
    pthread_rwlock_destroy(&free_blocks_mutex);
    pthread_rwlock_destroy(&free_open_file_entries_mutex);
}

/*
 * Checks how much free memory (in bytes) there is left on TFS
 * Returns:
 *  number of bytes
 */
int get_free_memory() {
    int size = 0;

    pthread_rwlock_rdlock(&free_blocks_mutex);
    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        if (free_blocks[i] == FREE) {
            size = size + BLOCK_SIZE;
        }
    }
    pthread_rwlock_unlock(&free_blocks_mutex);
    return size;
}

/*
 * Allocs an inode's first direct data block
 * Input:
 *  - inumber
 * Returns:
 *  0 if successful, -1 if an error occured
 */
int inode_alloc_first_block(int inumber) {
    if (!valid_inumber(inumber)) {
        return -1;
    }

    int b = data_block_alloc();
    if (b == -1) {
        pthread_rwlock_wrlock(&freeinode_ts_mutex);
        freeinode_ts[inumber] = FREE;
        pthread_rwlock_unlock(&freeinode_ts_mutex);
        return -1;
    }

    inode_table[inumber].i_data_block[0] = b;
    return 0;
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {


    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int) sizeof(allocation_state_t)) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

        pthread_rwlock_wrlock(&freeinode_ts_mutex);

        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;
            pthread_rwlock_unlock(&freeinode_ts_mutex);
            insert_delay(); // simulate storage access delay (to i-node)
            inode_table[inumber].i_node_type = n_type;

            //Allocate memory for first data block
            inode_table[inumber].i_data_block = (int *) malloc(sizeof(int) * 1);
            if (inode_table[inumber].i_data_block == NULL) { //Out of memory

                return -1;
            }
            inode_table[inumber].number_of_blocks = 1;
            inode_table[inumber].number_indirect_blocks = 0;
            inode_table[inumber].indirection_block = -1;
            pthread_rwlock_init(&inode_table[inumber].i_lock, NULL);

            if (n_type == T_DIRECTORY) {
                //Allocate memory for first data block
                int b = data_block_alloc();
                if (b == -1) {
                    pthread_rwlock_wrlock(&freeinode_ts_mutex);
                    freeinode_ts[inumber] = FREE;
                    pthread_rwlock_unlock(&freeinode_ts_mutex);
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
                inode_table[inumber].i_data_block[0] = b;

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {
                    pthread_rwlock_wrlock(&freeinode_ts_mutex);
                    freeinode_ts[inumber] = FREE;
                    pthread_rwlock_unlock(&freeinode_ts_mutex);
                    return -1;
                }

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }

            }
            else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;
                if (inode_alloc_first_block(inumber) == -1) {
                    return -1;
                }
            }
            return inumber;
        }
    }
    return -1;
}

/*
 * Frees an inode's direct data blocks
 * Input:
 *  - inode: pointer to an inode_t struct
 * Returns:
 *  1 if successful, -1 otherwise
 */
int inode_free_direct_blocks(inode_t *inode) {
    for (size_t i = 0; i < DIRECT_BLOCKS_COUNT; i++) {


        if (data_block_free(inode->i_data_block[i]) == -1) {
            pthread_rwlock_unlock(&inode->i_lock);
            return -1;
        }
    }

    inode->i_size = 0;
    inode->number_of_blocks = 0;

    return 1;
}

/*
 * Frees an inode's indirect data blocks
 * Input:
 *  - inode: pointer to an inode_t struct
 * Returns:
 *  1 if successful, -1 otherwise
 */
int inode_free_indirect_blocks(inode_t *inode) {
    int *block_of_indexes = data_block_get(inode->indirection_block);
    if (block_of_indexes == NULL) {
        return -1;
    }

    size_t end = BLOCK_SIZE / sizeof(int);
    size_t i = 1;
    while (i < end && block_of_indexes[i] != -1) {
        if (data_block_free(block_of_indexes[i]) == -1) {
            return -1;
        }
        i++;
    }
    if (data_block_free(inode->indirection_block) == -1) {
        return -1;
    }

    inode->number_indirect_blocks = 0;
    inode->i_size = DIRECT_BLOCKS_COUNT * BLOCK_SIZE;
    inode->indirection_block = -1;

    return 1;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();
    inode_t *inode;

    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE) {
        return -1;
    }

    freeinode_ts[inumber] = FREE;

    if ((inode = inode_get(inumber)) == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&inode->i_lock);

    if (inode_table[inumber].i_size > 0) {
        if (inode_free_direct_blocks(inode) == -1) {
            return -1;
        }
        if (data_block_free(inode->i_data_block[0]) == -1) {
            return -1;
        }
        free(inode_table[inumber].i_data_block);

        if (inode_table[inumber].indirection_block != -1) {
            //If the inode has any associated indirect blocks
            if (inode_free_indirect_blocks(inode) == -1) {
                return -1;
            }
        }
    }
    pthread_rwlock_destroy(&inode->i_lock);
    pthread_rwlock_destroy(&inode_table[inumber].i_lock);
    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    inode_t *inode = &inode_table[inumber];
    return inode;
}

/*
 * Checks if a certain inode is free
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: 1 if free, 0 if taken and -1 if it is an invalid inumber
 */
int inode_is_free(int inumber) {

    if (!valid_inumber(inumber)) {
        return -1;
    }
    pthread_rwlock_rdlock(&freeinode_ts_mutex);
    if (freeinode_ts[inumber] == TAKEN) {
        pthread_rwlock_unlock(&freeinode_ts_mutex);
        return 0;
    }
    pthread_rwlock_unlock(&freeinode_ts_mutex);
    return 1;
}


/*
 * Inicializes direct data blocks, within a given range, for an inode
 * Input:
 *  - inode: pointer to an inode
 *  - start: number of first data block
 *  - end: number of last data block to be inicialized
 * Returns: 0 if successful, -1 if an error occured
 */
int inode_inicialize_direct_blocks(inode_t *inode, size_t start, size_t end) {

    if (end < start || start > DIRECT_BLOCKS_COUNT) {
        //Incorrect bounds
        return -1;
    }

    for (size_t i = start; i < end; i++) {
        inode->i_data_block[i] = data_block_alloc();
        if (inode->i_data_block[i] == -1) {
            return -1;
        }
    }
    return 0;
}

/*
 * Writes a data block's index on an inode's indirection block
 * Input:
 *  - inode: pointer to an inode_t struct
 *  - block: block index
 * Returns:
 *  0 if successful, -1 otherwise
 */
int write_index_to_block(inode_t *inode, int block) {
    if (inode->number_indirect_blocks == 0) {
        return -1;
    }

    int *block_of_indexes = data_block_get(inode->indirection_block);
    if (block_of_indexes == NULL) {
        return -1;
    }

    size_t position = inode->number_indirect_blocks - 1;
    pthread_rwlock_wrlock(&fs_data_mutex);
    block_of_indexes[position] = block;
    pthread_rwlock_unlock(&fs_data_mutex);

    return 0;
}

/*
 * Inicializes indirect data blocks, within a given range, for an inode
 * Input:
 *  - inode: pointer to an inode
 *  - start: number of first data block
 *  - end: number of last data block to be inicialized
 * Returns: 0 if successful, -1 if an error occured
 */
int inode_inicialize_indirect_blocks(inode_t *inode, size_t start, size_t end) {
    unsigned int limit = BLOCK_SIZE / sizeof(int) + 1;
    int *block_of_indexes;

    if (inode->indirection_block == -1) {
        return -1;
    }

    block_of_indexes = data_block_get(inode->indirection_block);
    if (block_of_indexes == NULL) {
        return -1;
    }

    if (end < start || start > limit) {
        //Incorrect bounds
        return -1;
    }

    for (size_t i = start; i < limit; i++) {
        if (i < end) {
            int block = data_block_alloc();
            inode->number_indirect_blocks += 1;
            //Write index on indirection block
            write_index_to_block(inode, block);
        }
        else {
            pthread_rwlock_wrlock(&fs_data_mutex);
            block_of_indexes[i] = -1;
            pthread_rwlock_unlock(&fs_data_mutex);
        }
    }
    return 0;
}

/*
 * Associates the required amount of data blocks to an inode, in order to
 * store a given amount of information on the inode
 * Input:
 *  - inumber
 *  - sizeToBeAdded: memory in bytes required
 *  - offset: offset of the open file
 * Returns: 0 if successful, -1 if an error occured
 */
int inode_add_blocks(int inumber, size_t sizeToBeAdded, size_t offset) {
    int memory;
    size_t number_blocks;
    inode_t *inode;
    if (inode_is_free(inumber)) {
        return -1;
    }
    if ((inode = inode_get(inumber)) == NULL) {
        return -1;
    }

    if ((offset + sizeToBeAdded) <= ((inode->number_of_blocks + inode->number_indirect_blocks) * BLOCK_SIZE)) {
        //If there is no need for additional blocks
        return 0;
    }

    memory = get_free_memory();
    if (memory < sizeToBeAdded) { //Not enough data blocks
        return -1;
    }


    number_blocks = sizeToBeAdded / (unsigned int) BLOCK_SIZE; //Number of blocks to be added
    if ((offset + sizeToBeAdded) >= ((inode->number_of_blocks + inode->number_indirect_blocks) * BLOCK_SIZE)) {
        number_blocks++;
    }

    //Associate direct data blocks first
    if (inode->number_of_blocks < DIRECT_BLOCKS_COUNT) {
        size_t to_add = DIRECT_BLOCKS_COUNT - (unsigned int) inode->number_of_blocks;

        if (to_add > number_blocks) { //Still doesn't need 10 direct blocks
            to_add = number_blocks;
        }
        inode->i_data_block = (int *) realloc(inode->i_data_block, sizeof(int) * (unsigned int) (inode->number_of_blocks + to_add));
        if (inode->i_data_block == NULL) {
            return -1;
        }

        inode_inicialize_direct_blocks(inode, inode->number_of_blocks, inode->number_of_blocks + to_add);
        inode->number_of_blocks += to_add;

        number_blocks -= to_add;
    }
    if (number_blocks > 0 && inode->indirection_block == -1) {
        inode->indirection_block = data_block_alloc();
        if (inode->indirection_block == -1) {
            return -1;
        }
        inode_inicialize_indirect_blocks(inode, inode->number_indirect_blocks, inode->number_indirect_blocks + number_blocks);

    }
    return 0;
}

/*
 * Checks if it is an invalid block associated with an inode
 * Input:
 *  - inode
 *  - block_number
 * Returns: 0 if it is a valid block, otherwise returns a non-zero integer
 */
int inode_invalid_indirect_block(inode_t *inode, size_t block_number) {
    int *block_of_indexes = data_block_get(inode->indirection_block);


    return (block_of_indexes == NULL || (block_number >= (BLOCK_SIZE / sizeof(int))) ||
        block_of_indexes[block_number] == -1);
}


/*
 * Writes from a buffer to an inode's data blocks
 * Input:
 *  - file: pointer to an open file entry
 *  - inode: pointer to an inode_t struct
 *  - buffer: input buffer
 *  - to_write: number of bytes to write
 * Returns:
 *  number of bytes written if successful, -1 otherwise
 */
ssize_t inode_write(open_file_entry_t *file, inode_t *inode, void const*buffer, size_t to_write) {
    size_t bytes_written = 0;
    void *block = NULL;

    pthread_rwlock_wrlock(&inode->i_lock);
    pthread_rwlock_wrlock(&file->of_lock);
    if (file->of_offset > inode->i_size) { //If the file was truncated
        file->of_offset = inode->i_size;
    }
    if ((file->of_offset + to_write) > ((DIRECT_BLOCKS_COUNT + (BLOCK_SIZE / sizeof(int))) * BLOCK_SIZE)) {
        //If trying to write more than the inode can store
        to_write = ((DIRECT_BLOCKS_COUNT + (BLOCK_SIZE / sizeof(int))) * BLOCK_SIZE) - file->of_offset;
    }

    if (file->of_offset > inode->i_size) { //If the file was truncated
        file->of_offset = inode->i_size;
    }

    size_t block_to_write = file->of_offset / BLOCK_SIZE;
    if (block_to_write >= DIRECT_BLOCKS_COUNT) {
        block_to_write++; //Skip block of indexes
    }
    if (to_write > inode->i_size ) { //Need to increase inode size
        inode_add_blocks(file->of_inumber, to_write, file->of_offset);
    }
    else if ((file->of_offset + to_write) >= ((inode->number_of_blocks + inode->number_indirect_blocks) * BLOCK_SIZE)) {

        size_t memory_needed = to_write;
        if (to_write < BLOCK_SIZE) {
            memory_needed = BLOCK_SIZE;
        }
        inode_add_blocks(file->of_inumber, memory_needed, file->of_offset);
    }

    while (to_write > 0) {
        if (block_to_write == DIRECT_BLOCKS_COUNT) {
            block_to_write++; //Skip block of indexes
        }
        if (block_to_write < DIRECT_BLOCKS_COUNT) { //Start writing on direct data blocks
            block = data_block_get(inode->i_data_block[block_to_write]);
        }
        else {  //Start writing on indirect data blocks
            int *block_of_indexes = data_block_get(inode->indirection_block);
            if (inode_invalid_indirect_block(inode, block_to_write - DIRECT_BLOCKS_COUNT)) {
                int new_block = data_block_alloc();
                if (new_block == -1) {
                    pthread_rwlock_unlock(&inode->i_lock);
                    pthread_rwlock_unlock(&file->of_lock);
                    return -1;
                }
                inode->number_indirect_blocks++;
                pthread_rwlock_wrlock(&fs_data_mutex);
                block_of_indexes[inode->number_indirect_blocks] = new_block;
                pthread_rwlock_unlock(&fs_data_mutex);
                if (write_index_to_block(inode, new_block) == -1) {
                    pthread_rwlock_unlock(&inode->i_lock);
                    pthread_rwlock_unlock(&file->of_lock);
                    return -1;
                }

                if (inode_invalid_indirect_block(inode, block_to_write - DIRECT_BLOCKS_COUNT)) {
                    pthread_rwlock_unlock(&inode->i_lock);
                    pthread_rwlock_unlock(&file->of_lock);
                    return -1;
                }
            }
            block = data_block_get(block_of_indexes[block_to_write - DIRECT_BLOCKS_COUNT]);
        }

        if (block == NULL) {
            pthread_rwlock_unlock(&file->of_lock);
            pthread_rwlock_unlock(&inode->i_lock);
            return -1;
        }

        size_t size;
        size_t room_in_block = (block_to_write + 1) * BLOCK_SIZE - file->of_offset;
        if (to_write > room_in_block) {
            size = room_in_block;
        }
        else {
            size = to_write;
        }

        /* Perform the actual write */
        pthread_rwlock_wrlock(&fs_data_mutex);
        memcpy(block + (file->of_offset % BLOCK_SIZE), buffer, size);
        pthread_rwlock_unlock(&fs_data_mutex);

        /* Advance buffer pointer and update loop variables */
        buffer += size; //Disregard already written part of buffer
        to_write = to_write - size;
        bytes_written += size;

        /* The offset associated with the file handle is
        * incremented accordingly */
        file->of_offset += size;
        if (file->of_offset % BLOCK_SIZE == 0) {
            block_to_write += 1;
        }
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    pthread_rwlock_unlock(&file->of_lock);
    pthread_rwlock_unlock(&inode->i_lock);
    return (ssize_t) bytes_written;
}

/*
 * Reads to a buffer from an inode's data blocks
 * Input:
 *  - file: pointer to an open file entry
 *  - inode: pointer to an inode_t struct
 *  - buffer: output buffer
 *  - to_write: number of bytes to read
 * Returns:
 *  number of bytes read if successful, -1 otherwise
 */
ssize_t inode_read(open_file_entry_t *file, inode_t *inode, void *buffer, size_t len) {
    size_t bytes_read = 0;
    void *block;

    pthread_rwlock_rdlock(&inode->i_lock);
    pthread_rwlock_rdlock(&file->of_lock);

    if (file->of_offset > inode->i_size) {
        file->of_offset = inode->i_size;
    }
    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    bytes_read = 0;
    while (to_read > 0 && (file->of_offset < inode->i_size)) {
        size_t block_to_read = file->of_offset / BLOCK_SIZE;
        if (block_to_read >= DIRECT_BLOCKS_COUNT) {
            block_to_read++;
        }
        if (block_to_read < DIRECT_BLOCKS_COUNT) {
            block = data_block_get(inode->i_data_block[block_to_read]);
        }
        else {
            if (inode->indirection_block == -1 || inode_invalid_indirect_block(inode, block_to_read - DIRECT_BLOCKS_COUNT)) {
                pthread_rwlock_unlock(&inode->i_lock);
                pthread_rwlock_unlock(&file->of_lock);
                return -1;
            }
            int *block_of_indexes = data_block_get(inode->indirection_block);
            block = data_block_get(block_of_indexes[block_to_read - DIRECT_BLOCKS_COUNT]);
        }

        if (block == NULL) {
            pthread_rwlock_unlock(&inode->i_lock);
            pthread_rwlock_unlock(&file->of_lock);
            return -1;
        }

        size_t size;
        size_t room_in_block = (block_to_read + 1) * BLOCK_SIZE - file->of_offset;
        if (to_read > room_in_block) {
            size = room_in_block;
        }
        else {
            size = to_read;
        }
        /* Perform the actual read */
        pthread_rwlock_rdlock(&fs_data_mutex);
        memcpy(buffer, block + (file->of_offset % BLOCK_SIZE), size);
        pthread_rwlock_unlock(&fs_data_mutex);

        /* Advance buffer pointer and update loop variables*/
        bytes_read += size;
        to_read -= size;
        buffer += size;

        /* The offset associated with the file handle is
        * incremented accordingly */
        file->of_offset += size;
    }
    pthread_rwlock_unlock(&file->of_lock);
    pthread_rwlock_unlock(&inode->i_lock);
    return (ssize_t) bytes_read;
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */

int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber
    pthread_rwlock_wrlock(&inode_table_mutex);

    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
        pthread_rwlock_unlock(&inode_table_mutex);
        return -1;
    }

    if (strlen(sub_name) == 0) {
        pthread_rwlock_unlock(&inode_table_mutex);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block[0]);
    if (dir_entry == NULL) {
        pthread_rwlock_unlock(&inode_table_mutex);
        return -1;
    }
    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_entry[i].d_inumber == -1) {
            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;
            pthread_rwlock_unlock(&inode_table_mutex);
            return 0;
        }
    }

    pthread_rwlock_unlock(&inode_table_mutex);
    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber
    if (!valid_inumber(inumber) ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    /* Locates the block containing the directory's entries */

    pthread_rwlock_rdlock(&inode_table_mutex);
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block[0]);
    if (dir_entry == NULL) {
        pthread_rwlock_unlock(&inode_table_mutex);
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++)
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            pthread_rwlock_unlock(&inode_table_mutex);
            return dir_entry[i].d_inumber;
        }

    pthread_rwlock_unlock(&inode_table_mutex);
    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */

int data_block_alloc() {
    pthread_rwlock_wrlock(&free_blocks_mutex);
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
            pthread_rwlock_unlock(&free_blocks_mutex);
            return i;
        }
    }
    pthread_rwlock_unlock(&free_blocks_mutex);
    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
    pthread_rwlock_wrlock(&free_blocks_mutex);
    free_blocks[block_number] = FREE;
    pthread_rwlock_unlock(&free_blocks_mutex);
    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    pthread_rwlock_rdlock(&fs_data_mutex);
    void* ret = &fs_data[block_number * BLOCK_SIZE];
    pthread_rwlock_unlock(&fs_data_mutex);
    return ret;
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    pthread_rwlock_wrlock(&free_open_file_entries_mutex);
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            free_open_file_entries[i] = TAKEN;
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;
            pthread_rwlock_init(&open_file_table[i].of_lock, NULL);
            pthread_rwlock_unlock(&free_open_file_entries_mutex);
            return i;
        }
    }
    pthread_rwlock_unlock(&free_open_file_entries_mutex);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    pthread_rwlock_wrlock(&free_open_file_entries_mutex);
    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        pthread_rwlock_unlock(&free_open_file_entries_mutex);
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;
    pthread_rwlock_unlock(&free_open_file_entries_mutex);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    open_file_entry_t *entry = &open_file_table[fhandle];
    return entry;
}
