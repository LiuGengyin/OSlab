#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define NUM_FDESCS 16
#define FS_START_SEC       0x100000
#define BLKMAP_START_SEC   FS_START_SEC + 8
#define INODEMAP_START_SEC BLKMAP_START_SEC + 8 * 32
#define INODE_START_SEC    INODEMAP_START_SEC + 8
#define DATA_START_SEC     INODE_START_SEC + 8 * 8

#define TYPE_NULL 4
#define TYPE_DIR  0
#define TYPE_FILE 1
#define TYPE_ANCES 2
#define TYPE_SELF 3

#define NUM_DENTRYS 512
#define NUM_INODES  512

#define MODE_R 0x00000001
#define MODE_W 0x00000002
#define MODE_X 0x00000004
#define MODE_NULL 0x00000000

/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    int magic;
    int fssz;
    int phy_start_sec;
    
    int blockmap_blk;
    int blockmap_sz;
    
    int inodemap_blk;
    int inodemap_sz;
    
    int inode_blk;
    int inode_sz;
    
    int data_blk;
    int data_sz;
} superblock_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    int inodeid;
    uint8_t type;
    char name[27];
} dentry_t; //32bytes

typedef struct inode_t{ 
    // TODO [P6-task1]: Implement the data structure of inode
    int inodeid;
    int refer;
    int mode;
    int used; //若是目录，used指示目录项使用量；若是文件，used指示有效字节数
    int ptr0[7];
    int ptr1[3]; //二级间接指针。8MB一共需要2^11个数据块，需要2^11个指针。一个二级间接指针指向一个块，若用四个字节存储一个指针，一块可以存放2^10个指针，两个二级间址一共2^11个指针，即支持8MB大文件。数据块一共2^17个，17位即可存下，使用最高位标志这个指针有没有被占用。
    int ptr2[2];
} inode_t; //64bytes

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    int used;
    int inodeid;
    int mode;
    int rpos;
    int wpos;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path, int p);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset);

#endif