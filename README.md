# Simple File System

## File System Structure
### Super Block
Space: Block 0
``` C
typedef struct
{
    // Directory info
    int dir_index;
    int dir_len;

    // Data info
    int data_index;
} super_block;
```

### Directory infomation
Space: Block 1
Function: List of file information.
``` C
typedef struct
{
    bool used;                   // whether the file is being used
    char name[MAX_FILENAME_LEN]; // file name
    int size;                    // file size
    int head;                    // first data block
    int num_blocks;              // number of blocks
    int fd_count;                // number of file descriptors using this file
} file_info;
```

### Data block usage
Space: Block 2-3
Function: Store the file index of each block.

### Data blocks
Space: Block 4-8191
Function: Store the data of the file.

## Function Implementation
### File system management
#### make_fs(char *disk_name)
1. Initialize the super block
2. Write the super block to disk

#### mount_fs(char *disk_name)
1. Read super block
2. Read directory info
3. Clear file descriptors

#### umount_fs(char *disk_name)
1. Write directory info
2. Clear file descriptors

### File system operations
#### fs_open(char *name)
1. Allocate a file descriptor

#### fs_close(int fildes)
1. Free the allocated file descriptor

#### fs_create(char *name)
1. Initialize file information

#### fs_delete(char *name)
1. Remove file information
2. Free file blocks

#### fs_read(int fildes, void *buf, size_t nbyte)
1. Load current block
2. Read current block
3. Read the following blocks

#### fs_write(int fildes, void *buf, size_t nbyte)
1. Load current block
2. Write current block
3. Write the allocated blocks
4. Write into new blocks

#### fs_get_filesize(int fildes)
1. Get file info from the file descriptor table

#### fs_lseek(int fildes, off_t offset)
1. Modify fd of the file descriptor table

#### fs_truncate(int fildes, off_t length)
1. Free blocks
2. Modify file information
3. Truncate fd offset

### Helper function
1. `char find_file(char* name)`
2. `int find_free_file_des(char file_index)`
3. `int find_free_block(char file_index)`
4. `int find_next_block(int current, char file_index)`
