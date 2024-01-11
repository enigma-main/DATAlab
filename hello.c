/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello -ug
 *
 * ## Source code ##
 * \include hello.c
 */

// 0804 : readdir, getattr, init is okay, we can do cd otfs, ls
// mkdir says "There is no file" and makes Input/Output Error

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <sys/stat.h>

#include <libgen.h> // dirname, basename 함수 사용 목적
//#include "myutil.h"

//#include "bitmap.h"

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */


// #define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) do {} while (0)
#endif

#define DEBUG_MODE_READDIR
#ifdef DEBUG_MODE_READDIR
#define DEBUG_PRINT_READDIR(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_READDIR(fmt, ...) do {} while (0)
#endif

#define DEBUG_MODE_OPEN
#ifdef DEBUG_MODE_OPEN
#define DEBUG_PRINT_OPEN(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_OPEN(fmt, ...) do {} while (0)
#endif

#define DEBUG_MODE_READ
#ifdef DEBUG_MODE_READ
#define DEBUG_PRINT_READ(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_READ(fmt, ...) do {} while (0)
#endif


// blocks
#define BLOCK_SIZE 4096
#define MAX_BLOCKS 64
#define MAX_FILENAME_LENGTH 256
#define MAX_FILES 56 // num of inodes
#define MAX_DIRECT_POINTER 13
#define ROOT 0
#define DIRENT_SIZE BLOCK_SIZE * sizeof(char) / sizeof(struct st_DIRENT)
#define GET_SIZE(X) X.blocks_count * BLOCK_SIZE * sizeof(union u_DATABLOCK);

// static const char *hello_str = "Hello World!\n";
// static const char *hello_path = "/helloasdf";

struct st_INODE {
	int mode; // file type (IFDIR, IFREG, IFLNK) + rwxrwxrwx
	// int uid;
	int size; // how many bytes in this file?
	int time;
	// int ctime;
	// int mtime;
	// int dtime;
	// int gid;
	int links_count; // how many hard links
	int blocks_count; // how many blocks allocated to this file?
	// int flags;
	// int osd1;
	int datablock[MAX_DIRECT_POINTER]; // disk pointer
	// int generation
	// int file_acl;
	// int dir_acl;
	// int i_osd2;
} ;

struct st_DIRENT {
	char d_name[MAX_FILENAME_LENGTH];
	ino_t d_ino;
	// off_t d_off;
	// unsigned short d_reclen;
	// unsigned char d_type;
} ;

union u_DATABLOCK {
	char data[BLOCK_SIZE];//
	struct st_DIRENT dirent[DIRENT_SIZE];//
} ;

struct _FileSystem {
	// superblock은 구현 x
	int inodebitmap[MAX_FILES];
	int databitmap[MAX_FILES];
	struct st_INODE inodetable[MAX_FILES];
	union u_DATABLOCK datablock[MAX_FILES];
	int file_count;
} ;

struct _FileSystem FS = {0, };


char **split_path(const char *path, int *count) {
    // path = "/mnt/myFS/hello.c"
	// components_count = 3
	// path_components = ["mnt", "myFS", "hello.c"]

    char **components = malloc(sizeof(char *) * MAX_FILES);
    char *temp = strdup(path);
    char *token = strtok(temp, "/");

    *count = 0;
    while (token != NULL) {
        components[*count] = strdup(token);
        token = strtok(NULL, "/");
        (*count)++;
    }

    free(temp);
    return components;
}



int find_empty_inode() {
	// first vs. last
	for (int i = 0; i < MAX_FILES; i++) {
		if (FS.inodebitmap[i] == 0) {
			return i;
		}
	}
	return -ENOMEM;
}
int find_empty_datablock() {
	for (int i = 0; i < MAX_FILES; i++) {
		if (FS.databitmap[i] == 0) {
			return i;
		}
	}
	return -ENOMEM;
}
int find_empty_dirent(int inode) {
	int datablock0 = FS.inodetable[inode].datablock[0];
	for (int j = 0; j < DIRENT_SIZE; j++) {
		if (FS.datablock[datablock0].dirent[j].d_ino == 0) {
			return j;
		}
	}
	return -ENOMEM;
}

int find_next_inode(const char *name, int inode) {
    // "/mnt/c/" "/mnt/myFS/"
    // input "myFS", "/mnt/" inode pointer
    // output "/mnt/myFS/" inode index
	DEBUG_PRINT("find_next_inode start\n");
	int datablock0 = FS.inodetable[inode].datablock[0];
	for (int j = 0; j < DIRENT_SIZE; j++) {
		if (strcmp(FS.datablock[datablock0].dirent[j].d_name, name) == 0){
			DEBUG_PRINT("find_next_inode finish\n");
			return FS.datablock[datablock0].dirent[j].d_ino;
		}
	}
	DEBUG_PRINT("find_next_inode finish\n");
    // for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	for (int j = 0; j < BLOCK_SIZE; j++) {
	// 		if (strcmp(iblock->datablock[i]->dirent[j].d_name, name) == 0)
    //         	return iblock->datablock[i]->dirent[j].d_ino;
    //     }
    // }
    return -1;  // Directory not found
}
int find_inode(const char *path) {
	// root 인 경우
	if (strcmp(path, "/") == 0) return ROOT;

	// root 아닌 경우
	char *temp_path = strdup(path); // string duplication
	// char *base_name = basename(temp_path); 
	// path parsing
	int components_count;
	char **path_components = split_path(temp_path, &components_count);
	DEBUG_PRINT("find_inode: comp_count: %d, cur_inode: %d\n", components_count, current_inode);
	for (int i = 0; i < components_count; i++){
		DEBUG_PRINT("%s, ", path_components[i]);
	}
	DEBUG_PRINT("\n");

	// root directory 부터 recursively path 탐색
	int inode = ROOT;
	for (int i = 0; i < components_count; i++) {
		inode = find_next_inode(path_components[i], inode);
		if (inode < 0) {
			for (int j = 0; j < components_count; j++) free(path_components[j]);
			free(path_components);
			break;
		}
	}
	free(temp_path);
	return inode;
}
int find_parent_inode(const char *path) {
    // root 디렉터리인 경우
    if (strcmp(path, "/") == 0) {
        return -EPERM;
    }

    char *temp_path = strdup(path); // 경로 복사
    int components_count;
    char **path_components = split_path(temp_path, &components_count);

    int inode = ROOT;
    DEBUG_PRINT("find_parent: comp_count: %d, cur_inode: %d\n", components_count, current_inode);
	for(int i = 0; i < components_count; i++){
		DEBUG_PRINT("%s, ", path_components[i]);
	}
	DEBUG_PRINT("\n");
	
	// 부모 디렉터리까지 탐색
    for (int i = 0; i < components_count - 1; i++) {
        inode = find_next_inode(path_components[i], inode);
        if (inode < 0) {
            // 오류 처리: 디렉터리가 존재하지 않음
            for (int j = 0; j < components_count; j++) {
                free(path_components[j]);
            }
            free(path_components);
            free(temp_path);
            return -ENOENT;
        }
    }
	
    // 메모리 정리
    for (int j = 0; j < components_count; j++) {
        free(path_components[j]);
    }
	DEBUG_PRINT("free: path_comp\n");
    free(path_components);
	DEBUG_PRINT("free: path_comps\n");
    free(temp_path);
	DEBUG_PRINT("free: temp_path\n");

    return inode;
}
int find_dirent(int current_inode, int parent_inode) {
	int current_datablock0 = FS.inodetable[current_inode].datablock[0];
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];

	for (int j = 2; j < DIRENT_SIZE; j++) {
		if (FS.datablock[parent_datablock0].dirent[j].d_ino == current_inode){
			return j;
		}
	}

	return -ENOENT;
}

static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }

static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};



int _g_fd;

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"hello\" file\n"
	       "                        (default: \"hello\")\n"
	       "    --contents=<s>      Contents \"hello\" file\n"
	       "                        (default \"Hello, World!\\n\")\n"
	       "\n");
}


// simple file system
// super block
// inode bitmap block
// data bitmap block
// inode table
// data region

static void *ot_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	DEBUG_PRINT("INIT start \n");
	
	memset(&FS, 0, sizeof(struct _FileSystem));
	DEBUG_PRINT("memset complete \n");
	
	FS.inodebitmap[ROOT] = 1;
	FS.databitmap[0] = 1;
	int inode = ROOT;
	int datablock0 = ROOT;
	DEBUG_PRINT("bitmap complete \n");

	FS.inodetable[inode].datablock[0] = datablock0;
	DEBUG_PRINT("datablock allocation \n");

	strcpy(FS.datablock[datablock0].dirent[0].d_name, ".");
	FS.datablock[datablock0].dirent[0].d_ino = 0;
	strcpy(FS.datablock[datablock0].dirent[1].d_name, "..");
	FS.datablock[datablock0].dirent[1].d_ino = 0;
	DEBUG_PRINT("datablock complete \n");
	
	FS.inodetable[inode].mode = S_IFDIR | 0777;
	FS.inodetable[inode].links_count = 2;
	FS.inodetable[inode].blocks_count = 1;
	FS.inodetable[inode].size = GET_SIZE(FS.inodetable[ROOT]);
	DEBUG_PRINT("metadata complete \n");
	// for (int i = 0; i < MAX_FILES; i++) {
	// for (int j = 0; j < BLOCK_SIZE; j++){
	// 		FS.datablock[i].data[j] = 0;
	// 	}
	// }
	// for (int i = 0; i < MAX_BLOCKS - MAX_FILES; i++){
	// 	// FS.inodetable[i].filename[0] = '\0';
	// 	memset(FS.inodetable[i].filename, 0, MAX_FILENAME_LENGTH);
	// 	FS.inodetable[i].block_count = 0;
	// 	FS.inodetable[i].start_blocks = 0;
	// 	FS.inodetable[i].permission = 0;
	// 	// FS.inodetable[i].owner[0] = '\0'; // memset(..., 0, ...)으로도 가능할 것 같긴 한데..
	// 	memset(FS.inodetable[i].owner, 0, MAX_FILENAME_LENGTH);
	// }

	// return fuse_get_context()->private_data;
	DEBUG_PRINT("MAX_DIRENT: %ld\n", DIRENT_SIZE);
}

static int ot_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	(void) fi;
	DEBUG_PRINT("\nGETATTR start \n");
	DEBUG_PRINT("path: %s\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	DEBUG_PRINT("memset finished\n");
	// root 처리 구분이 굳이 필요할까?
	// if (strcmp(path, "/") == 0) {
	// 	stbuf->st_mode = S_IFDIR | 0755;
	// 	stbuf->st_nlink = 2;
	// 	DEBUG_PRINT("GETATTR finish \n\n");
	// 	return 0;
	// }

	// root 아닌 경우
	// path parsing
	char *temp_path = strdup(path); // string duplication
	int components_count;
	char **path_components = split_path(temp_path, &components_count);
	
	// root directory 부터 recursively path 탐색
	int inode = find_inode(path);
	DEBUG_PRINT("inode: %d\n", inode);
	if (inode < 0) {
		DEBUG_PRINT("GETATTR finish \n\n");
		return -ENOENT;
	}
	
	stbuf->st_mode = FS.inodetable[inode].mode; // | S_IFREG
	stbuf->st_nlink = FS.inodetable[inode].links_count;
	stbuf->st_size = GET_SIZE(FS.inodetable[inode])
	stbuf->st_gid = getgid();
	stbuf->st_uid = getuid();
	stbuf->st_atime = time(NULL);
	stbuf->st_mtime = time(NULL);
	
	for (int j = 0; j < components_count; j++) free(path_components[j]);
	free(path_components);
	free(temp_path);
	DEBUG_PRINT("GETATTR finish \n\n");
	return 0;
}

static int ot_readdir(const char *path, void *buf, fuse_fill_dir_t filler, \
						off_t offset, struct fuse_file_info *fi)
{
	(void) offset; // directory 내에 파일이 여러 개 있을 경우, offset 부터 끝까지만 read 할 때 사용. 이 구현에서는 offset 기능 사용하지 않음
	(void) fi;
	memset(buf, 0, sizeof(void*));
	
	DEBUG_PRINT_READDIR("\nREADDIR start \n");
	DEBUG_PRINT_READDIR("path: %s\n", path);

	int inode = find_inode(path);
	DEBUG_PRINT_READDIR("inode: %d\n", inode);
	if (inode < 0) {
		DEBUG_PRINT_READDIR("READDIR FINISH \n\n");
		return -ENOENT;
	}
	
	// file list 생성
	// filler 함수 사용하여 buffer에 파일 이름 추가
	if ((FS.inodetable[inode].mode & S_IFDIR) != S_IFDIR){
		return -ENOTDIR;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	
	int i = 0;
	int datablock0 = FS.inodetable[inode].datablock[0];

	for (int j = 0; j < DIRENT_SIZE; j++) {
		if (FS.datablock[datablock0].dirent[j].d_ino > 0){
			filler(buf, FS.datablock[datablock0].dirent[j].d_name, NULL, 0, 0);
			printf("%s ", FS.datablock[datablock0].dirent[j].d_name);
		}
	}
	printf("\n");

	DEBUG_PRINT_READDIR("READDIR finish \n\n");
	return 0;
}

static int ot_open(const char *path, struct fuse_file_info *fi)
{
	DEBUG_PRINT_OPEN("\nOPEN start \n");
	DEBUG_PRINT_OPEN("path: %s\n", path);

	// check file not found: return -ENOENT
	int inode = find_inode(path);
	DEBUG_PRINT_OPEN("inode: %d\n", inode);
	if (inode < 0) {
		DEBUG_PRINT_OPEN("OPEN finish \n\n");
		return -ENOENT;
	}

	// check permission: EACCES
	DEBUG_PRINT_OPEN("mode: %d    fi->flags: %d \n", FS.inodetable[inode].mode, fi->flags);
	if ((fi->flags & O_ACCMODE) == O_RDONLY){
		if ((FS.inodetable[inode].mode & 04) == 04){
			DEBUG_PRINT_OPEN("OPEN RDONLY finish \n\n");
			return 0;
		}
	}
	else if ((fi->flags & O_ACCMODE) == O_WRONLY) {
		if ((FS.inodetable[inode].mode & 02) == 02){
			DEBUG_PRINT_OPEN("OPEN WRONLY finish \n\n");
			return 0;
		}
	}
	
	else {
		if ((FS.inodetable[inode].mode & 06) == 06){
			DEBUG_PRINT_OPEN("OPEN RW finish \n\n");
			return 0;
		}
	}
	DEBUG_PRINT_OPEN("OPEN finish \n\n");
	return -EACCES;
}

static int ot_read(const char *path, char *buf, size_t size, \
					off_t off, struct fuse_file_info *fi)
{
	DEBUG_PRINT_READ("\nREAD start \n");
	DEBUG_PRINT_READ("path: %s    size: %d\n", path, size);
	// size_t len;////
	(void) fi;
	
	memset(buf, 0, size);

	int inode = find_inode(path);
	int datablock0 = FS.inodetable[inode].datablock[0];
	
	// if (off < FS.inodetable[inode].size) {
	// 	if (off + size < FS.inodetable[inode].size) {
	// 		;
	// 	}
	// 	else {
	// 		size = FS.inodetable[inode].size - off;
	// 	}
	// }
	// else size = 0;
	
	buf = FS.datablock[datablock0].data;
	size = 12;

	// strcpy, memset 으로는 안되는건가?
	// strcpy(buf, FS.datablock[datablock0].data);
	printf("Read content: %s   inode[%d] datablock[%d]\n", \
			FS.datablock[datablock0].data, inode, datablock0);  // Print the content being read
	printf("Read buf: %s  \n", buf); 
	printf("size: %d   offset: %d    inodetableSize: %d\n", size, off, FS.inodetable[inode].size);
	// memcpy(buf, FS.datablock[datablock0].data, size);
	// memcpy(buf, &FS.datablock[datablock0], size);
	// 둘중 어느 것이 맞을까?

	DEBUG_PRINT_READ("READ finish \n");
	return size;
}


static int ot_write(const char *path, const char *mem, size_t size, off_t off,
		      struct fuse_file_info *fi)
{
	// create 이후 write가 호출된다고 가정
	printf("WRITE start\n");
	
	printf("path: %s\n", path);
	(void) fi;
	
	int current_inode = find_inode(path);
	if(current_inode < 0) {
		printf("WRITE finish with no entry\n");
		return -ENOENT;
	}
	int current_datablock0 = FS.inodetable[current_inode].datablock[0];

	// main: write
	int block_size = GET_SIZE(FS.inodetable[current_inode]);
	if (size < block_size) {
		memcpy(FS.datablock[current_datablock0].data, mem, size);
		printf("Writing content: %.*s    inode[%d]  datablock[%d] \n", \ 
			(int)size, mem, current_inode, current_datablock0); 
	}
	// else if ((size / block_size) < MAX_DIRECT_POINTER) {
	// 	for (int i = 0; i < (size / block_size); i++){
	// 		memcpy(current_inodeblock->datablock[i], mem + (i * current_inodeblock->size), current_inodeblock->size);
	// 	}
	// }
	else {
		printf("WRITE finish with too large file\n");
		return -EFBIG;
	}
	// if (offset < len) {
	// 	if (offset + size > len)
	// 		size = len - offset;
	// 	memcpy(buf, current_inodeblock->datablock + offset, size);
	// }
	// else 
	// 	size = 0;

	printf("WRTIE finish \n");
	return size;
}


static int ot_mkdir(const char *path, mode_t mode)
{
	DEBUG_PRINT("MKDIR start \n");
	DEBUG_PRINT("path: %s\n", path);

	// check parent inode
	int parent_inode = find_parent_inode(path);
	DEBUG_PRINT("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		DEBUG_PRINT("MKDIR finish \n");
		return -ENOENT;
	}
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];
	
	// allocate new inode
	int current_inode = find_empty_inode();
	DEBUG_PRINT("inode: %d\n", current_inode);
	if (current_inode < 0) {
		DEBUG_PRINT("MKDIR finish \n");
		return -ENOMEM;
	}
	// allocate new datablock
	int current_datablock0 = find_empty_datablock();
	DEBUG_PRINT("datablock: %d\n", current_datablock);
	if (current_datablock0 < 0) {
		DEBUG_PRINT("MKDIR finish \n");
		return -ENOMEM;
	}

	// link inode and datablock
	FS.inodetable[current_inode].datablock[0] = current_datablock0;
	DEBUG_PRINT("cur: inode datablock mapping: inode %d -> datablock %d\n", current_inode, current_datablock);
	
	// add ".", ".." to dirent0, dirent1
	strcpy(FS.datablock[current_datablock0].dirent[0].d_name, ".");
	FS.datablock[current_datablock0].dirent[0].d_ino = current_inode;
	strcpy(FS.datablock[current_datablock0].dirent[1].d_name, "..");
	FS.datablock[current_datablock0].dirent[1].d_ino = parent_inode;
	DEBUG_PRINT("cur: datablock ., .. add\n");

	// link new inode and parent datablock
	int new_dirent = find_empty_dirent(parent_inode);
	if (new_dirent < 0) {
		DEBUG_PRINT("MKDIR finish\n\n");
		return new_dirent;
	}
	FS.datablock[parent_datablock0].dirent[new_dirent].d_ino = current_inode;
	char *temp_path = strdup(path); // string duplication
	char *base_name = basename(temp_path); 
	strcpy(FS.datablock[parent_datablock0].dirent[new_dirent].d_name, base_name);
	// DEBUG_PRINT("pinode: %d, dirent[%d].d_ino = %d, name: %s\n", \
	//  	parent_inode, j, current_inode, base_name);
	// DEBUG_PRINT("pinode: %d, dirent[%d].d_ino = %d, name: %s\n", \
	//  	parent_inode, j, parent_inodeblock->datablock[0]->dirent[j].d_ino, \
	//  	parent_inodeblock->datablock[0]->dirent[j].d_name);


	// for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	for (int j = 2; j < DIRENT_SIZE; j++) {
	// 		if (parent_inodeblock->datablock[i]->dirent[j].d_ino == 0){
	// 			parent_inodeblock->datablock[i]->dirent[j].d_ino = current_inode;
	// 			char *temp_path = strdup(path); // string duplication
	// 			char *base_name = basename(temp_path); 
	// 			strcpy(parent_inodeblock->datablock[i]->dirent[j].d_name, base_name);
	// 		}
    //     }
    // }

	// Initialize the new datablock
	// for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	current_inodeblock->datablock[i] = &FS.datablock[current_datablock];
	// }

	// set metadata
	FS.inodetable[current_inode].mode = S_IFDIR | 0777;
	FS.databitmap[current_datablock0] = 1;
	FS.inodebitmap[current_inode] = 1;

	FS.inodetable[current_inode].blocks_count = 1;
	FS.inodetable[current_inode].size = DIRENT_SIZE * 2;
	FS.inodetable[current_inode].links_count = 2;
	
	FS.inodetable[parent_inode].size += DIRENT_SIZE;
	FS.inodetable[parent_inode].links_count += 1;
	DEBUG_PRINT("metadata\n");

	DEBUG_PRINT("MKDIR finish \n\n");
	return 0;
}

static int ot_rmdir(const char *path)
{
	DEBUG_PRINT("\nRMDIR start \n");
	DEBUG_PRINT("path: %s\n", path);

	// check root
	if (strcmp(path, "/") == 0){
		DEBUG_PRINT("RMDIR finish \n");
		return -EPERM;
	}
	// check parent inode
	int parent_inode = find_parent_inode(path);
	DEBUG_PRINT("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		DEBUG_PRINT("RMDIR finish \n");
		return -ENOENT;
	}
	// allocate new inode
	int current_inode = find_inode(path);
	DEBUG_PRINT("cur_inode: %d\n", current_inode);
	if (current_inode < 0) {
		DEBUG_PRINT("RMDIR finish \n");
		return -ENOENT;
	}

	// check is directory
	if ((FS.inodetable[current_inode].mode & S_IFDIR) != S_IFDIR) {
		DEBUG_PRINT("RMDIR finish\n");
		return -ENOTDIR;
	}

	// check directory is empty
	// dirent 전체를 scan.
	// for (int j = 2; j < DIRENT_SIZE; j++){
	// 	if (current_inodeblock->datablock[0]->dirent[j].d_ino != 0) {
	// 		DEBUG_PRINT("RMDIR finish\n\n");
	// 		return -EEXIST;
	// 	}
	// }
	// -> size 혹은 block_count로 개선하면 좋을 것 같음
	if (FS.inodetable[current_inode].size > 2 * DIRENT_SIZE) {
		DEBUG_PRINT("RMDIR finish\n\n");
		return -EEXIST;
	}

	// free current_datablock and current_inode
	int current_datablock0 = FS.inodetable[current_inode].datablock[0];
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];
	memset(FS.datablock[current_datablock0].data, 0, sizeof(union u_DATABLOCK));
	FS.inodetable[current_inode].datablock[0] = 0;
	
	// free parent_datablock and parent_inode
	int j = find_dirent(current_inode, parent_inode);
	FS.datablock[parent_datablock0].dirent[j].d_ino = 0;
	strcpy(FS.datablock[parent_datablock0].dirent[j].d_name, "");

	// set metadata
	memset(&FS.inodetable[current_inode], 0, sizeof(struct st_INODE));
	FS.databitmap[current_datablock0] = 0;
	FS.inodebitmap[current_inode] = 0;

	FS.inodetable[parent_inode].size -= DIRENT_SIZE;
	FS.inodetable[parent_inode].links_count -= 1;
	DEBUG_PRINT("metadata\n");

	DEBUG_PRINT("RMDIR finish \n\n");
	return 0;
}

static int ot_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("\nCREATE start \n");
	printf("path: %s\n", path);

	// check parent inode
	int parent_inode = find_parent_inode(path);
	printf("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		printf("CREATE finish \n\n");
		return parent_inode;
	}
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];
	
	// allocate new inode
	int current_inode = find_empty_inode();
	printf("inode: %d\n", current_inode);
	if (current_inode < 0) {
		printf("CREATE finish \n\n");
		return current_inode;
	}
	// allocate new directory entry
	int current_datablock0 = find_empty_datablock();
	printf("datablock: %d\n", current_datablock0);
	if (current_datablock0 < 0) {
		printf("CREATE finish \n\n");
		return current_datablock0;
	}
	int j = find_empty_dirent(parent_inode);
	if (j < 0) {
		printf("CREATE finish \n\n");
		return j;
	}
	
	// link inode and datablock
	FS.inodetable[current_inode].datablock[0] = current_datablock0;
	printf("cur: inode datablock mapping: inode %d -> datablock %d\n", current_inode, current_datablock0);

	// link new inode and parent datablock
	FS.datablock[parent_datablock0].dirent[j].d_ino = current_inode;
	char *temp_path = strdup(path); // string duplication
	char *base_name = basename(temp_path); 
	strcpy(FS.datablock[parent_datablock0].dirent[j].d_name, base_name);
	
	// **** test ****
	strcpy(FS.datablock[current_datablock0].data, "Hello World!\0");
	printf("after strcpy: %s   inode[%d] datablock[%d] \n", FS.datablock[current_datablock0].data, current_inode, current_datablock0);
	FS.inodetable[current_inode].size = strlen("Hello World!\0");
	printf("size: %d\n", FS.inodetable[current_inode].size);
	// for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	for (int j = 2; j < DIRENT_SIZE; j++) {
	// 		if (parent_inodeblock->datablock[i]->dirent[j].d_ino == 0){
	// 			parent_inodeblock->datablock[i]->dirent[j].d_ino = current_inode;
	// 			char *temp_path = strdup(path); // string duplication
	// 			char *base_name = basename(temp_path); 
	// 			strcpy(parent_inodeblock->datablock[i]->dirent[j].d_name, base_name);
	// 		}
    //     }
    // }


	// Initialize the new datablock
	// for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	current_inodeblock->datablock[i] = &FS.datablock[current_datablock];
	// }

	// set metadata
	FS.databitmap[current_datablock0] = 1;
	FS.inodebitmap[current_inode] = 1;

	FS.inodetable[current_inode].mode = S_IFREG | 0777;
	FS.inodetable[current_inode].blocks_count = 1;
	FS.inodetable[current_inode].size = 0;
	FS.inodetable[current_inode].links_count = 1;

	FS.inodetable[parent_inode].size += DIRENT_SIZE;
	printf("metadata\n");

	printf("CREATE finish \n\n");
	return 0;
}

static int ot_unlink(const char *path)
{
	printf("UNLINK start \n");
	printf("path: %s\n", path);

	// check parent inode
	int parent_inode = find_parent_inode(path);
	printf("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		printf("UNLINK finish \n");
		return -ENOENT;
	}
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];
	// allocate new inode
	int current_inode = find_inode(path);
	printf("cur_inode: %d\n", current_inode);
	if (current_inode < 0) {
		printf("UNLINK finish \n");
		return -ENOENT;
	}
	int current_datablock0 = FS.inodetable[current_inode].datablock[0];
	// check is regular file
	if ((FS.inodetable[current_inode].mode & S_IFDIR) == S_IFDIR) {
		printf("UNLINK finish\n");
		return -EISDIR;
	}

	// check link exist .. 필요할까?
	if (FS.inodetable[current_inode].links_count < 1) {
		printf("UNLINK finish\n");
		return -ENOENT;
	}

	// main: unlink 
	char *temp_path = strdup(path); // string duplication
	char *base_name = basename(temp_path); 
	for (int j = 2; j < DIRENT_SIZE; j++){
		if (strcmp(FS.datablock[current_datablock0].dirent[j].d_name, base_name) == 0) { // find_dirent는 d_ino로 비교. unlink는 d_name으로 비교 필요
			memset(&FS.datablock[current_datablock0].dirent[j], 0, sizeof(struct st_DIRENT));
			FS.inodetable[current_inode].links_count -= 1;
			printf("unlink: parent\n");
			return -EEXIST;
		}
	}


	// sub: if no link count
	if (FS.inodetable[current_inode].links_count < 1) {
		// free datablock
		memset(&FS.datablock[current_datablock0], 0, sizeof(union u_DATABLOCK));
		FS.inodetable[current_inode].datablock[0] = 0;
		// datablock의 pointer를 사용하지 않고 datablock의 index를 사용하도록 코드수정 필요
		// FS.databitmap[current_datablock] = 0;

		// unlink parent datablock
		for (int j = 2; j < DIRENT_SIZE; j++) {
			if (FS.datablock[parent_datablock0].dirent[j].d_ino == current_inode){
				FS.datablock[parent_datablock0].dirent[j].d_ino = 0;
				strcpy(FS.datablock[parent_datablock0].dirent[j].d_name, "");
				break;
			}
		}
		
		// set metadata
		// free inode
		memset(&FS.inodetable[current_inode], 0, sizeof(struct st_INODE));
		
		// free bitmap
		FS.inodebitmap[current_inode] = 0;
		FS.databitmap[current_datablock0] = 0;
		printf("metadata\n");	
	}
	
	printf("UNLINK finish \n");
	return 0;
}

static int ot_utimens(const char *path, const struct timespec tv[2],
			 struct fuse_file_info *fi)
{
	printf("UTIMENS start \n");
	printf("path: %s\n", path);

	// check inode
	int inode = find_inode(path);
	printf("cur_inode: %d\n", inode);
	if (inode < 0) {
		printf("UNLINK finish \n");
		return -ENOENT;
	}
	FS.inodetable[inode].time = time(NULL);

	printf("UTIMENS finish \n");
	return 0;
}

static int ot_release(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

// 경로 이동 기능 미구현
static int ot_rename(const char *source, const char *dest, unsigned int flags)
{
	printf("RENAME start \n");
	printf("source: %s  dest: %s\n", source, dest);

	// check parent inode
	int parent_inode = find_parent_inode(source);
	printf("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		printf("RENAME finish \n\n");
		return -ENOENT;
	}
	int parent_datablock0 = FS.inodetable[parent_inode].datablock[0];
	
	// check inode
	int inode = find_inode(source);
	printf("inode: %d\n", inode);
	if (inode < 0) {
		printf("RENAME finish \n\n");
		return -ENOENT;
	}
	int datablock0 = FS.inodetable[inode].datablock[0];
	int parent_dirent = find_dirent(inode, parent_inode);
	
	char *temp_path = strdup(dest);
	char *base_name = basename(temp_path); 
	strcpy(FS.datablock[parent_datablock0].dirent[parent_dirent].d_name, base_name);

	printf("RENAME finish \n");
	return 0;
}

static const struct fuse_operations ot_oper = {
	.init       = ot_init,
	.getattr	= ot_getattr,
	.readdir	= ot_readdir,
	.open		= ot_open,
	.read		= ot_read,
	.mkdir		= ot_mkdir,
	.rmdir		= ot_rmdir,
    .create		= ot_create, // option w,r,x 가능하지만 S_IFDIR S_IFREG 구분
	.write		= ot_write,
	.unlink		= ot_unlink,
	.utimens    = ot_utimens,
	.release	= ot_release,
	.rename		= ot_rename,
	/*
	.destroy	= ot_destroy, // 구현 x
	*/
};


int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* Set defaults -- we have to use strdup so that
	   fuse_opt_parse can free the defaults if other
	   values are specified */
	options.filename = strdup("hello");
	options.contents = strdup("Hello World!\n");

	/* Parse options */
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	ret = fuse_main(args.argc, args.argv, &ot_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
