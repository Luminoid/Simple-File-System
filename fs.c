//
// Created by Ethan Lee on 2016/11/25.
//
#include <stddef.h>
#include <printf.h>
#include <memory.h>
#include <stdlib.h>
#include "fs.h"
#include "disk.h"

super_block*    super_block_ptr;               // Super block
file_info*      dir_info;                      // Directory info
file_descriptor fd_table[MAX_FILE_DESCRIPTOR]; // File descriptor table

int make_fs(char *disk_name)
{
    make_disk(disk_name);
    open_disk(disk_name);

    /* Initialize the super block */
    super_block_ptr = (super_block*)malloc(sizeof(super_block));
    if (super_block_ptr == NULL) return -1;
    super_block_ptr->dir_index = 1;
    super_block_ptr->dir_len = 0;
    super_block_ptr->data_index = 2;

    /* write the super block to disk (block 0) */
    char buf[BLOCK_SIZE] = "";
    // memset(void *b, int c, size_t len):
    // writes len bytes of value c (converted to an unsigned char) to the string b.
    memset(buf, 0, BLOCK_SIZE);
    // memcpy(void *restrict dst, const void *restrict src, size_t n);
    // copies n bytes from memory area src to memory area dst.
    memcpy(buf, &super_block_ptr, sizeof(super_block));
    block_write(0, buf);
    free(super_block_ptr);

    close_disk();
    printf("make_fs()\t called successfully.\n");
    return 0;
}

int mount_fs(char *disk_name)
{
    if(disk_name == NULL) return -1;
    open_disk(disk_name);

    /* read super block */
    char buf[BLOCK_SIZE] = "";
    memset(buf, 0, BLOCK_SIZE);
    block_read(0, buf);
    memcpy(&super_block_ptr, buf, sizeof(super_block_ptr));

    /* read directory info */
    dir_info = (file_info*)malloc(BLOCK_SIZE);
    memset(buf, 0, BLOCK_SIZE);
    block_read(super_block_ptr->dir_index, buf);
    memcpy(dir_info, buf, sizeof(file_info) * super_block_ptr->dir_len);

    /* clear file descriptors */
    int i;
    for(i = 0; i < MAX_FILE_DESCRIPTOR; ++i) {
        fd_table[i].used = false;
    }

    printf("mount_fs()\t called successfully: file system [%s] mounted.\n", disk_name);
    return 0;
}

int umount_fs(char *disk_name)
{
    if(disk_name == NULL) return -1;

    /* write directory info */
    int i, j = 0;
    file_info* file_ptr = (file_info*)dir_info;
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    char* block_ptr = buf;

    for (i = 0; i < MAX_FILE; ++i) {
        if(dir_info[i].used == true) {
            memcpy(block_ptr, &dir_info[i], sizeof(dir_info[i]));
            block_ptr += sizeof(file_info);
        }
    }

    block_write(super_block_ptr->dir_index, buf);

    /* clear file descriptors */
    for(j = 0; j < MAX_FILE_DESCRIPTOR; ++j) {
        if(fd_table[j].used == 1) {
            fd_table[j].used = false;
            fd_table[j].file = -1;
            fd_table[j].offset = 0;
        }
    }

    free(dir_info);
    close_disk();
    printf("umount_fs()\t called successfully: file system [%s] umounted.\n", disk_name);
    return 0;
}

int fs_open(char *name)
{
    char file_index = find_file(name);
    if(file_index < 0) {  // file not found
        fprintf(stderr, "fs_open()\t error: file [%s] does not exist.\n",name);
        return -1;
    }

    int fd = find_free_file_des(file_index);
    if (fd < 0){
        fprintf(stderr, "fs_open()\t error: exceed the maximum file descriptor number.\n");
        return -1;
    }

    dir_info[file_index].fd_count++;
    printf("fs_open()\t called successfully: file [%s] opened.\n", name);
    return fd;
}

int fs_close(int fildes)
{
    if(fildes < 0 || fildes >= MAX_FILE_DESCRIPTOR || !fd_table[fildes].used) {
        return -1;
    }

    file_descriptor* fd = &fd_table[fildes];

    dir_info[fd->file].fd_count--;
    fd->used = false;

    printf("fs_close()\t called successfully: file [%s] closed\n", dir_info[fd->file].name);
    return 0;
}

int fs_create(char *name)
{
    char file_index = find_file(name);

    if (file_index < 0){  // Create file
        char i;
        for(i = 0; i < MAX_FILE; i++) {
            if(dir_info[i].used == false) {
                super_block_ptr->dir_len++;
                /* Initialize file information */
                dir_info[i].used = true;
                strcpy(dir_info[i].name, name);
                dir_info[i].size = 0;
                dir_info[i].head = -1;
                dir_info[i].num_blocks = 0;
                dir_info[i].fd_count = 0;
                printf("fs_create()\t called successfully: file [%s] created.\n", name);
                return 0;
            }
        }
        fprintf(stderr, "fs_create()\t error: exceed the maximum file number.\n");
        return -1;
    } else {              // File already exists
        fprintf(stderr, "fs_create()\t error: file [%s] already exists\n",name);
        return 0;
    }
}

int fs_delete(char *name)
{
    char i;

    for(i = 0; i < MAX_FILE; ++i) {
        if(strcmp(dir_info[i].name, name) == 0) {
            char file_index = i;
            file_info* file = &dir_info[i];
            int block_index = file->head;
            int block_count = file->num_blocks;

            if(dir_info[i].fd_count != 0) { // File is currently open
                fprintf(stderr, "fs_delete()\t error: file [%s] is currently open.\n",name);
                return -1;
            }

            // Remove file information
            super_block_ptr->dir_len--;
            file->used = false;
            strcpy(file->name, "");
            file->size = 0;
            file->fd_count = 0;

            /* Free file blocks */
            char buf1[BLOCK_SIZE] = "";
            char buf2[BLOCK_SIZE] = "";
            block_read(super_block_ptr->data_index, buf1);
            block_read(super_block_ptr->data_index + 1, buf2);
            while (block_count > 0){
                if (block_index < BLOCK_SIZE){
                    buf1[block_index] = '\0';
                } else {
                    buf2[block_index - BLOCK_SIZE] = '\0';
                }
                block_index = find_next_block(file->head, file_index);
                block_count--;
            }

            dir_info[i].head = -1;
            dir_info[i].num_blocks = 0;
            block_write(super_block_ptr->data_index, buf1);
            block_write(super_block_ptr->data_index + 1, buf2);

            printf("fs_delete()\t called successfully: file [%s] deleted.\n", name);
            return 0;
        }
    }

    fprintf(stderr, "fs_delete()\t error: file [%s] does not exists\n", name);
    return -1;
}

int fs_read(int fildes, void *buf, size_t nbyte)
{
    if(nbyte <= 0 || !fd_table[fildes].used) {
        return -1;
    }

    int i, j = 0;
    char *dst = buf;
    char block[BLOCK_SIZE] = "";
    char file_index = fd_table[fildes].file;
    file_info* file = &dir_info[file_index];
    int block_index = file->head;
    int block_count = 0;
    int offset = fd_table[fildes].offset;

    /* load current block */
    while (offset >= BLOCK_SIZE){
        block_index = find_next_block(block_index, file_index);
        block_count++;
        offset -= BLOCK_SIZE;
    }
    block_read(block_index, block);

    /* read current block */
    int read_count = 0;
    for(i = offset; i < BLOCK_SIZE; i++) {
        dst[read_count++] = block[i];
        if(read_count == (int)nbyte) {
            fd_table[fildes].offset += read_count;
            return read_count;
        }
    }
    block_count++;

    /* read the following blocks */
    strcpy(block,"");
    while(read_count < (int)nbyte && block_count <= file->num_blocks) {
        block_index = find_next_block(block_index, file_index);
        strcpy(block,"");
        block_read(block_index, block);
        for(j=0; j < BLOCK_SIZE; j++, i++) {
            dst[read_count++] = block[j];
            if(read_count == (int)nbyte ) {
                fd_table[fildes].offset += read_count;
                return read_count;
            }
        }
        block_count++;
    }
    fd_table[fildes].offset += read_count;
    return read_count;
}

int fs_write(int fildes, void *buf, size_t nbyte)
{
    if(nbyte <= 0 || !fd_table[fildes].used) {
        return -1;
    }

    int i = 0;
    char *src = buf;
    char block[BLOCK_SIZE] = "";
    char file_index = fd_table[fildes].file;
    file_info* file = &dir_info[file_index];
    int block_index = file->head;
    int size = file->size;
    int block_count = 0;
    int offset = fd_table[fildes].offset;

    /* load current block */
    while (offset >= BLOCK_SIZE){
        block_index = find_next_block(block_index, file_index);
        block_count++;
        offset -= BLOCK_SIZE;
    }

    int write_count = 0;
    if (block_index != -1){
        /* write current block */
        block_read(block_index, block);
        for(i = offset; i < BLOCK_SIZE; i++) {
            block[i] = src[write_count++];
            if (write_count == (int)nbyte || write_count == strlen(src)) {
                block_write(block_index, block);
                fd_table[fildes].offset += write_count;
                if(size < fd_table[fildes].offset){
                    file->size = fd_table[fildes].offset;
                }
                return write_count;
            }
        }
        block_write(block_index, block);
        block_count++;
    }

    /* write the allocated blocks */
    strcpy(block, "");
    while(write_count < (int)nbyte && write_count < strlen(src) && block_count < file->num_blocks) {
        block_index = find_next_block(block_index, file_index);
        for(i = 0; i < BLOCK_SIZE; i++) {
            block[i] = src[write_count++];
            if(write_count == (int)nbyte || write_count == strlen(src)) {
                block_write(block_index, block);
                fd_table[fildes].offset += write_count;
                if(size < fd_table[fildes].offset){
                    file->size = fd_table[fildes].offset;
                }
                return write_count;
            }
        }
        block_write(block_index, block);
        block_count++;
    }

    /* write into the new blocks */
    strcpy(block, "");
    while(write_count < (int)nbyte && write_count < strlen(src)) {
        block_index = find_free_block(file_index);
        file->num_blocks++;
        if (file->head == -1){
            file->head = block_index;
        }
        if (block_index < 0){
            fprintf(stderr, "fs_write()\t error: No free blocks.\n");
            return -1;
        }
        for(i = 0; i < BLOCK_SIZE; i++) {
            block[i] = src[write_count++];
            if(write_count == (int)nbyte || write_count == strlen(src)) {
                block_write(block_index, block);
                fd_table[fildes].offset += write_count;
                if(size < fd_table[fildes].offset){
                    file->size = fd_table[fildes].offset;
                }
                return write_count;
            }
        }
        block_write(block_index, block);
    }

    fd_table[fildes].offset += write_count;
    if(size < fd_table[fildes].offset){
        file->size = fd_table[fildes].offset;
    }
    return write_count;
}

int fs_get_filesize(int fildes){
    if(!fd_table[fildes].used){
        fprintf(stderr, "fs_get_filesize()\t error: Invalid file descriptor.\n");
        return -1;
    }
    return dir_info[fd_table[fildes].file].size;
}

int fs_lseek(int fildes, off_t offset)
{
    if (offset > dir_info[fd_table[fildes].file].size || offset < 0){
        fprintf(stderr, "fs_lseek()\t error: Can't set the file pointer beyond the file range.\n");
        return -1;
    } else if(!fd_table[fildes].used){
        fprintf(stderr, "fs_lseek()\t error: Invalid file descriptor.\n");
        return -1;
    } else {
        fd_table[fildes].offset = (int)offset;
        printf("fs_lseek()\t called successfully.\n");
        return 0;
    }
}

int fs_truncate(int fildes, off_t length)
{
    char file_index = fd_table[fildes].file;
    file_info* file  = &dir_info[file_index];

    if(!fd_table[fildes].used){
        fprintf(stderr, "fs_truncate()\t error: Invalid file descriptor.\n");
        return -1;
    }

    if (length > file->size || length < 0) {
        fprintf(stderr, "fs_truncate()\t error: Can't set the offset beyond the file range.\n");
        return -1;
    }

    /* free blocks */
    int new_block_num = (int) (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int i;
    int block_index = file->head;
    for (i = 0; i < new_block_num; ++i) {
        block_index = find_next_block(block_index, file_index);
    }
    while (block_index > 0){
        char buf[BLOCK_SIZE] = "";
        if (block_index < BLOCK_SIZE){
            block_read(super_block_ptr->data_index, buf);
            buf[block_index] = '\0';
            block_write(super_block_ptr->data_index, buf);
        } else {
            block_read(super_block_ptr->data_index + 1, buf);
            buf[block_index - BLOCK_SIZE] = '\0';
            block_write(super_block_ptr->data_index + 1, buf);
        }
        block_index = find_next_block(block_index, file_index);
    }

    /* file information */
    file->size = (int)length;
    file->num_blocks = new_block_num;

    /* truncate fd offset */
    for(i = 0; i < MAX_FILE_DESCRIPTOR; i++) {
        if(fd_table[i].used == true && fd_table[i].file == file_index) {
            fd_table[i].offset = (int)length;
        }
    }
    printf("fs_truncate()called successfully.\n");
    return 0;
}

char find_file(char* name)
{
    char i;

    for(i = 0; i < MAX_FILE; i++) {
        if(dir_info[i].used == 1 && strcmp(dir_info[i].name, name) == 0) {
            return i;  // return the file index
        }
    }

    return -1;         // file not found
}

int find_free_file_des(char file_index)
{
    int i;

    for(i = 0; i < MAX_FILE_DESCRIPTOR; i++) {
        if(fd_table[i].used == false) {
            fd_table[i].used = true;
            fd_table[i].file = file_index;
            fd_table[i].offset = 0;
            return i;  // return the file descriptor number
        }
    }

    fprintf(stderr, "find_free_file_des()\t error: no available file descriptor.\n");
    return -1;         // no empty file descriptor available
}

int find_free_block(char file_index)
{
    char buf1[BLOCK_SIZE] = "";
    char buf2[BLOCK_SIZE] = "";
    block_read(super_block_ptr->data_index, buf1);
    block_read(super_block_ptr->data_index + 1, buf2);
    int i;

    for(i = 4; i < BLOCK_SIZE; i++) {
        if(buf1[i] == '\0') {
            buf1[i] = (char)(file_index + 1);
            block_write(super_block_ptr->data_index, buf1);
            return i;  // return block number
        }
    }
    for(i = 0; i < BLOCK_SIZE; i++) {
        if(buf2[i] == '\0') {
            buf2[i] = (char)(file_index + 1);
            block_write(super_block_ptr->data_index, buf2);
            return i;  // return block number
        }
    }
    fprintf(stderr, "find_free_block()\t error: no available blocks.\n");
    return -1;         // no free blocks
}

int find_next_block(int current, char file_index){
    char buf[BLOCK_SIZE] = "";
    int i;

    if (current < BLOCK_SIZE){
        block_read(super_block_ptr->data_index, buf);
        for(i = current + 1; i < BLOCK_SIZE; i++) {
            if (buf[i] == (file_index + 1)){
                return i;
            }
        }
    } else {
        block_read(super_block_ptr->data_index + 1, buf);
        for(i = current - BLOCK_SIZE + 1; i < BLOCK_SIZE; i++) {
            if (buf[i] == (file_index + 1)){
                return i + BLOCK_SIZE;
            }
        }
    }

    return -1; // no next block
}

int main()
{
    int i;
    char* disk_name = "RootDir";
    if(make_fs(disk_name) < 0) {
        fprintf(stderr, "make_fs()\t error.\n");
    }

    if(mount_fs(disk_name) < 0) {
        fprintf(stderr, "mount_fs()\t error.\n");
    }

    if(fs_create("test.txt") < 0) {
        fprintf(stderr, "fs_create()\t error.\n");
    }

    /* fs_delete() test */
    if(fs_delete("test.txt") < 0) {
        fprintf(stderr, "fs_delete()\t error.\n");
    }

    if(fs_create("test.txt") < 0) {
        fprintf(stderr, "fs_create()\t error.\n");
    }

    /* fs_write() test */
    int fd1;
    if((fd1 = fs_open("test.txt")) < 0) {
        fprintf(stderr, "fs_open()\t error.\n");
    }
    int fd2;
    if((fd2 = fs_open("test.txt")) < 0) {
        fprintf(stderr, "fs_open()\t error.\n");
    }

    char str1[BLOCK_SIZE * 2];
    for(i = 0; i < BLOCK_SIZE / 2; i++) {
        str1[i] = 'a';
        str1[i + BLOCK_SIZE / 2] = 'b';
        str1[i + BLOCK_SIZE] = 'c';
        str1[i + BLOCK_SIZE * 3 / 2] = 'd';
    }
    char str2[BLOCK_SIZE * 2];
    for(i = 0; i < BLOCK_SIZE / 2; i++) {
        str2[i] = 'e';
        str2[i + BLOCK_SIZE / 2] = 'f';
        str2[i + BLOCK_SIZE] = 'g';
        str2[i + BLOCK_SIZE * 3 / 2] = 'h';
    }
    fs_write(fd1, str1, BLOCK_SIZE * 2);
    fs_write(fd1, str2, BLOCK_SIZE * 2);

    /* fs_lseek() test */
    char str3[BLOCK_SIZE * 2];
    for(i = 0; i < BLOCK_SIZE / 2; i++) {
        str3[i] = 'i';
        str3[i + BLOCK_SIZE / 2] = 'j';
        str3[i + BLOCK_SIZE] = 'k';
        str3[i + BLOCK_SIZE * 3 / 2] = 'l';
    }
    fs_lseek(fd2, BLOCK_SIZE * 2);
    fs_write(fd2, str3, BLOCK_SIZE * 2);

    /* fs_truncate() test */
    fs_truncate(fd2, BLOCK_SIZE * 5 / 2);
    char str4[BLOCK_SIZE * 3 / 2];
    for(i = 0; i < BLOCK_SIZE / 2; i++) {
        str4[i] = 'm';
        str4[i + BLOCK_SIZE / 2] = 'n';
        str4[i + BLOCK_SIZE] = 'o';
    }
    fs_write(fd2, str4, BLOCK_SIZE * 3 / 2);

    if(fs_close(fd1) < 0) {
        fprintf(stderr, "fs_close()\t error.\n");
    }

    if(fs_close(fd2) < 0) {
        fprintf(stderr, "fs_close()\t error.\n");
    }

    if(umount_fs(disk_name) < 0) {
        fprintf(stderr, "umount_fs()\t error.\n");
    }

    if(mount_fs(disk_name) < 0) {
        fprintf(stderr, "mount_fs()\t error.\n");
    }

    if((fd1 = fs_open("test.txt")) < 0) {
        fprintf(stderr, "fs_open()\t error.\n");
    }

    /* fs_read() Test */
    char buf1[BLOCK_SIZE * 4] = "";
    char val1[BLOCK_SIZE * 4];
    for(i = 0; i < BLOCK_SIZE / 2; i++) {
        val1[i] = 'a';
        val1[i + BLOCK_SIZE / 2] = 'b';
        val1[i + BLOCK_SIZE] = 'c';
        val1[i + BLOCK_SIZE * 3 / 2] = 'd';
        val1[i + BLOCK_SIZE * 2] = 'i';
        val1[i + BLOCK_SIZE * 5 / 2] = 'm';
        val1[i + BLOCK_SIZE * 3] = 'n';
        val1[i + BLOCK_SIZE * 7 / 2] = 'o';
    }
    int fd3;
    if((fd3 = fs_open("test.txt")) < 0) {
        fprintf(stderr, "fs_open()\t error.\n");
    }
    if(fs_read(fd3, buf1, BLOCK_SIZE * 4) < 0) {
        fprintf(stderr, "fs_read()\t error.\n");
    } else {
        printf("fs_read()\t called successfully.\n");
    }
    for (i = 0; i < BLOCK_SIZE * 4; ++i) {
        if (buf1[i] != val1[i]) {
            fprintf(stderr, "fs_read()\t error: Content [%d] error: [%c] and [%c].\n", i, buf1[i], val1[i]);
            break;
        }
    }

    /* fs_get_filesize() Test */
    int file_size;
    if((file_size = fs_get_filesize(fd1)) < 0) {
        fprintf(stderr, "fs_get_filesize()\t error.\n");
    } else {
        printf("fs_get_filesize()\t called successfully: The file size is %d\n", file_size);
    }

    /* multiple file test */
    char j;
    int k;
    for (j = 0; j < 63; ++j) {
        /* create */
        char* file_name = (char*)malloc(9);
        char index[2];
        index[0] = (char) (j + 48);
        index[1] = '\0';
        strcat(file_name, "test");
        strcat(file_name, index);
        strcat(file_name, ".txt");
        fs_create(file_name);

        /* write */
        char str[BLOCK_SIZE * 64];
        int fd_w = fs_open(file_name);
        for(k = 0; k < BLOCK_SIZE * 64; ++k) {
            str[k] = (char) (j + 48);
        }
        fs_write(fd_w, str, BLOCK_SIZE * 64);

        /* read */
        char buf[BLOCK_SIZE * 64];
        int fd_r = fs_open(file_name);
        fs_read(fd_r, buf, BLOCK_SIZE * 64);
        for (k = 0; k < BLOCK_SIZE * 64; ++k) {
            if (str[k] != buf[k]) {
                fprintf(stderr, "fs_read()\t error: Content [%d] error: [%c] and [%c].\n", k, str[k], buf[k]);
                break;
            }
        }

        /* close */
        fs_close(fd_w);
        fs_close(fd_r);
        memset(file_name, 0, 9);
        free(file_name);
    }

    if(umount_fs(disk_name) < 0) {
        fprintf(stderr, "umount_fs()\t error.\n");
    }

    return 0;
};