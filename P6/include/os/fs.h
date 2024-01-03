#ifndef INCLUDE_FS_H_
#define INCLUDE_FS_H_

#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <screen.h>

#define NAMESIZ 28
#define BCACHE_NR 32
#define BLOCKSIZ 4096
#define INO_NUM 1024
#define BCACHE_SIZ 4096
#define NRDIR 3
#define NRINDIR1 (1 << 10)
#define NRINDIR2 (1 << 20)
#define T_DIR 0
#define T_FIL 1
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define ROOT_INO 0
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define B2SEC(x) ((x) + (1 << 12)) << 3
#define INO2BLOCK(x) (x) / 128 + 6
#define DATABLOCK 0
#define INDIRBLOCK 1

typedef struct{
    uint32_t magic;
    uint32_t sb_id;
    uint32_t sb_num;
    uint32_t bmap_id;
    uint32_t bmap_num;
    uint32_t inmap_id;
    uint32_t inmap_num;
    uint32_t in_id;
    uint32_t in_num;
    uint32_t root_ino;
    uint32_t fs_siz;
    uint32_t bl_used;
    uint32_t in_used;
    char pad[460];
}superblock_t;

typedef struct{
    char mode;
    char owner;
    char link;
    char pad;
    int fsiz;
    uint32_t time;
    int addr[NRDIR+2];
}inode_t;

typedef struct{
    int ino;
    char name[NAMESIZ];
}dentry_t;

typedef struct{
    char buff[BCACHE_SIZ];
    int valid;
    int block_id;
}bcache_t;

extern superblock_t sb;
extern bcache_t bcache[BCACHE_NR];

extern void init_fs();
extern void do_mkfs();
extern void do_statfs(char*);
extern int do_enter_fs(char*);
extern void do_get_cwdpath(char*);
extern int do_mkdir(char*);
extern int do_rmdir(char*);
extern int do_ls(char*, char*, int);
extern int do_create(char*);
extern int do_open(char*, int);
extern int do_read(int, char*, int);
extern int do_write(int, char*, int);
extern void do_close(int);
extern int do_cat(char*);
extern int do_lseek(int, int, int);
extern int do_link(char*, char*);

extern int read_file(inode_t*, char*, int, int);
extern int write_file(inode_t*, char*, int, int);
extern void dinit(int, int, char*);
extern void finit(int, int, char*);
extern int ialloc();
extern void iinit(int, char);
extern void ifree(int);
extern void block_zero(int);
extern int balloc(int);
extern void bfree(int);
extern int bread(int);
extern void bwrite(int);
extern int block_map(inode_t*, int);
extern int dirlookup(inode_t*, char*);
extern int path_resolver(char*, int, char*);

#endif