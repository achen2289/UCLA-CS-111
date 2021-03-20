// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "ext2_fs.h"

int fd;
// Superblock fields
__u32 blocks_count, inodes_count, block_size,
             inode_size, blocks_per_group, inodes_per_group,
             first_non_reserved_inode;
struct ext2_super_block superblock;
struct ext2_group_desc group;

const char correct_usage[] = "./lab3a [FILE SYSTEM IMAGE]";
const char read_image_error[] = "ERROR: Image metadata could not be read in";
const char invalid_disk_image[] = "ERROR: Disk image could not be verified";

void check_return_error(int status, const char *msg, int exit_status);
void get_options(int argc, char **argv);
void close_img();
void read_superblock();
void read_group_descriptors();
void read_bitmap(char source);
void read_inodes();
void read_directory_entry(struct ext2_inode *curr_inode, __u32 parent_inode_num);
void read_indirect_ref(__u32 owner_inode_num, __u32 inode_num, __u32 indirection_lvl, __u32 offset);

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status < 0) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Correct usage: %s\n", correct_usage);
        exit(1);
    }
    fd = open(argv[1], O_RDONLY);
    check_return_error(fd, strerror(errno), 1);

    if ((long unsigned int) pread(fd, &superblock, sizeof(superblock), 1024) < sizeof(superblock)) {
        check_return_error(-1, read_image_error, 2);
    }
    if (superblock.s_magic != EXT2_SUPER_MAGIC) {
        check_return_error(-1, invalid_disk_image, 2);
    }
}

void close_img() {
    close(fd);
}

// Read superblock info
void read_superblock() {
    blocks_count = superblock.s_blocks_count;
    inodes_count = superblock.s_inodes_count;
    block_size = 1024 << superblock.s_log_block_size;
    inode_size = superblock.s_inode_size;
    blocks_per_group = superblock.s_blocks_per_group;
    inodes_per_group = superblock.s_inodes_per_group;
    first_non_reserved_inode = superblock.s_first_ino;

    fprintf(stdout, "%s,%u,%u,%u,%u,%u,%u,%u\n", 
           "SUPERBLOCK",
           blocks_count,
           inodes_count,
           block_size,
           inode_size,
           blocks_per_group,
           inodes_per_group,
           first_non_reserved_inode
    );    
}

// Read descriptor of the one and only group present in disk image
void read_group_descriptors() {
    int group_desc_offset = block_size == 1024 ? block_size * 2 : block_size;
    if ((long unsigned int) pread(fd, &group, sizeof(group), group_desc_offset) < sizeof(group)) {
        check_return_error(-1, read_image_error, 2);
    }
    fprintf(stdout, "%s,%d,%u,%u,%u,%u,%u,%u,%u\n",
            "GROUP",
            0,
            blocks_count,
            inodes_count,
            group.bg_free_blocks_count,
            group.bg_free_inodes_count,
            group.bg_block_bitmap,
            group.bg_inode_bitmap,
            group.bg_inode_table
    );
}

// Read block and inode bitmaps, depending on source
// Print out all available blocks and inodes
void read_bitmap(char source) {
    __u32 bitmap_start = source == 'b' ? group.bg_block_bitmap : group.bg_inode_bitmap;
    bitmap_start *= block_size;
    char bitmap[block_size]; // each bitmap limited to size of block
    if (bitmap == NULL) {
        check_return_error(-1, strerror(errno), 2);
    }
    if (pread(fd, bitmap, block_size, bitmap_start) < block_size) {
        check_return_error(-1, read_image_error, 2);
    }
    __u32 count = source == 'b' ? blocks_count : inodes_count;
    char *element = source == 'b' ? "BFREE" : "IFREE";
    for (__u32 i=0; i<count; i++) {
        char byte = bitmap[i/8];
        char bitmask = 1 << (i%8);
        if ((byte & bitmask) == 0) {
            fprintf(stdout, "%s,%u\n", element, i+1);
        }
    }
}

// Read inode table
void read_inodes() {
    __u32 inode_table_start = group.bg_inode_table * block_size;
    struct ext2_inode inode_table[inodes_count];
    if ((long unsigned int)pread(fd, &inode_table, sizeof(inode_table), inode_table_start) < sizeof(inode_table)) {
        check_return_error(-1, read_image_error, 2);
    }
    for (__u32 i=0; i<inodes_per_group; i++) {
        struct ext2_inode *curr_inode = &inode_table[i];
        if (curr_inode->i_mode == 0 || curr_inode->i_links_count == 0) {
            continue;
        }
        char file_type;
        if (S_ISDIR(curr_inode->i_mode)) {
            file_type = 'd';
        }
        else if (S_ISREG(curr_inode->i_mode)) {
            file_type = 'f';
        }
        else if (S_ISLNK(curr_inode->i_mode)) {
            file_type = 's';
        }
        else {
            file_type = '?';
        }
        time_t i_ctime = curr_inode->i_ctime;
        time_t i_mtime = curr_inode->i_mtime;
        time_t i_atime = curr_inode->i_atime;
        struct tm gmt_c = *gmtime(&i_ctime); 
        struct tm gmt_m = *gmtime(&i_mtime);
        struct tm gmt_a = *gmtime(&i_atime);
        char ctime[18], mtime[18], atime[18];
        strftime(ctime, sizeof(ctime), "%m/%d/%y %H:%M:%S", &gmt_c);
        strftime(mtime, sizeof(mtime), "%m/%d/%y %H:%M:%S", &gmt_m);
        strftime(atime, sizeof(atime), "%m/%d/%y %H:%M:%S", &gmt_a);
        fprintf(stdout, "%s,%u,%c,%o,%d,%d,%d,%s,%s,%s,%u,%u",
                "INODE",
                i+1,
                file_type,
                (curr_inode->i_mode & 0xfff),
                curr_inode->i_uid,
                curr_inode->i_gid,
                curr_inode->i_links_count,
                ctime,
                mtime,
                atime,
                curr_inode->i_size,
                curr_inode->i_blocks
        );
        if (file_type == 'f' || file_type == 'd' || (file_type == 's' && curr_inode->i_size >= 60)) {
            for (int j=0; j<EXT2_N_BLOCKS; j++) {
                fprintf(stdout, ",%u", curr_inode->i_block[j]);
            }
        }
        fprintf(stdout, "\n");
        if (file_type == 'd') {
            read_directory_entry(curr_inode, i+1);
        }
        if (curr_inode->i_block[12] != 0) {
            read_indirect_ref(i+1, curr_inode->i_block[12], 1, 12);
        }
        if (curr_inode->i_block[13] != 0) {
            read_indirect_ref(i+1, curr_inode->i_block[13], 2, 12+256);
        }
        if (curr_inode->i_block[14] != 0) {
            read_indirect_ref(i+1, curr_inode->i_block[14], 3, 12+256+256*256);
        }
    }
}

// Read the data blocks corresponding to a directory
void read_directory_entry(struct ext2_inode *curr_inode, __u32 parent_inode_num) {
    for (int i=0; i<EXT2_NDIR_BLOCKS; i++) {
        if (curr_inode->i_block[i] != 0) {
            __u32 block_offset = 0;
            while (block_offset < block_size) {
                struct ext2_dir_entry curr_entry;
                if (pread(fd, &curr_entry, sizeof(curr_entry), curr_inode->i_block[i]*block_size+block_offset) < 0) {
                    check_return_error(-1, read_image_error, 2);
                }
                if (curr_entry.inode != 0) {
                    fprintf(stdout, "%s,%u,%u,%u,%d,%d,'%s'\n",
                            "DIRENT",
                            parent_inode_num,
                            block_offset,
                            curr_entry.inode,
                            curr_entry.rec_len,
                            curr_entry.name_len,
                            curr_entry.name
                    );
                }
                block_offset += curr_entry.rec_len;
            }
        }
    }
}

// Read the indirect data block references
void read_indirect_ref(__u32 owner_inode_num, __u32 inode_num, __u32 indirection_lvl, __u32 offset) {
    if (indirection_lvl == 0) {
        return;
    }
    __u32 ref_count = block_size / sizeof(__u32);
    __u32 refs[ref_count];
    if (pread(fd, refs, block_size, inode_num*block_size) < 0) {
        check_return_error(-1, read_image_error, 2);
    }
    for (__u32 i=0; i<ref_count; i++) {
        if (refs[i] == 0) {
            continue;
        }
        fprintf(stdout, "%s,%u,%u,%u,%u,%u\n",
                "INDIRECT",
                owner_inode_num,
                indirection_lvl,
                offset+i,
                inode_num,
                refs[i]
        );
        read_indirect_ref(owner_inode_num, refs[i], indirection_lvl-1, offset);
    }
}

int main(int argc, char **argv) {
    get_options(argc, argv);
    atexit(close_img);
    read_superblock();
    read_group_descriptors();
    read_bitmap('b'); // read block bitmap
    read_bitmap('i'); // read inode bitmap
    read_inodes();
    exit(0);
}