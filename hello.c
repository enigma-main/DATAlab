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

// blocks
#define BLOCK_SIZE 1024
#define MAX_BLOCKS 64
#define MAX_FILENAME_LENGTH 256
#define MAX_FILES 56 // num of inodes
#define MAX_DIRECT_POINTER 13
#define ROOT 0


// static const char *hello_str = "Hello World!\n";
// static const char *hello_path = "/helloasdf";

struct st_INODE {
	int mode; // file type (IFDIR, IFREG, IFLNK) + rwxrwxrwx
	// int uid;
	int size; // how many bites in this file?
	// int time;
	// int ctime;
	// int mtime;
	// int dtime;
	// int gid;
	int links_count; // how many hard links
	int blocks_count; // how many blocks allocated to this file?
	// int flags;
	// int osd1;
	union u_DATABLOCK* datablock[MAX_DIRECT_POINTER]; // disk pointer
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
	struct st_DIRENT dirent[];//
} ;

struct _FileSystem {
	// superblock은 구현 x
	int inodebitmap[BLOCK_SIZE];
	int databitmap[BLOCK_SIZE];
	struct st_INODE inodetable[MAX_FILES];
	union u_DATABLOCK datablock[MAX_BLOCKS];
	int file_count;
} ;
struct _FileSystem FS;

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

int find_next_inode(const char *name, const struct st_INODE *iblock) {
    // "/mnt/c/" "/mnt/myFS/"
    // input "myFS", "/mnt/" inode pointer
    // output "/mnt/myFS/" inode index

    for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
		for (int j = 0; j < BLOCK_SIZE; j++) {
			if (strcmp(iblock->datablock[i]->dirent[j], name) == 0)
            	return iblock->datablock[i]->dirent[j].d_ino;
        }
    }
    return -1;  // Directory not found
}

int find_inode(const char *path) {
	// root 아닌 경우
	char *temp_path = strdup(path); // string duplication
	// char *base_name = basename(temp_path); 
	
	// path parsing
	int components_count;
	char **path_components = split_path(temp_path, &components_count);
	
	// root directory 부터 recursively path 탐색
	int current_inode = ROOT;
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	for (int i = 0; i < components_count; i++) {
		current_inodeblock = &FS.inodetable[current_inode];
		current_inode = find_next_inode(path_components[i], current_inodeblock);
		if (current_inode < 0) {
			for (int j = 0; j < components_count; j++) free(path_components[j]);
			free(path_components);
			free(temp_path);
			return -ENOENT;
		}
	}

	return current_inode;
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

// init -> *void
static void *ot_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	printf("INIT start \n");
	memset(FS, 0, sizeof(struct _FileSystem));
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
}

// getattr -> int
static int ot_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	(void) fi;
	printf("GETATTR start \n");

	memset(stbuf, 0, sizeof(struct stat));

	// path로부터 file 찾기
	// root 처리
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		printf("GETATTR finish \n");
		return 0;
	}

	// root 아닌 경우
	// path parsing
	int components_count;
	char **path_components = split_path(temp_path, &components_count);
	
	// root directory 부터 recursively path 탐색
	int current_inode = find_inode(path);
	if (current_inode < 0) return -ENOENT;

	stbuf->st_mode = current_inodeblock->mode; // | S_IFREG
	stbuf->st_nlink = current_inodeblock->links_count;
	stbuf->st_size = current_inodeblock->blocks_count * BLOCK_SIZE * sizeof(union u_DATABLOCK);
	
	for (int j = 0; j < components_count; j++) free(path_components[j]);
	free(path_components);
	free(temp_path);
	printf("GETATTR finish \n");
	return 0;
	// for (int i = 0; i < FS.file_count; i++) {
	// 	if (strcmp(FS.inodetable[i].filename, base_name) == 0) {
	// 		// stbuf 채우기
	// 		stbuf->st_mode = __S_IFREG | FS.inodetable[i].permission;
	// 		stbuf->st_nlink = 1;
	// 		stbuf->st_size = FS.inodetable[i].block_count * BLOCK_SIZE;
	// 		free(temp_path);
	// 		printf("GETATTR finish \n");
	// 		return 0;
	// 	}
	// }
}

// readdir -> int
static int ot_readdir(const char *path, void *buf, fuse_fill_dir_t filler, \
						off_t offset, struct fuse_file_info *fi)
{
	(void) offset; // directory 내에 파일이 여러 개 있을 경우, offset 부터 끝까지만 read 할 때 사용. 이 구현에서는 offset 기능 사용하지 않음
	(void) fi;

	printf("READDIR start \n");

	memset(buf, 0, sizeof(buf));

	char *temp_path = strdup(path); // string duplication
	char *base_name = basename(temp_path); 

	int current_inode = ROOT;
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];

	if (strcmp(path, "/") != 0) {
		int components_count;
		char **path_components = split_path(temp_path, &components_count);
		
		for (int i = 0; i < components_count; i++) {
			current_inode = find_directory(path_components[i], current_inodeblock);
			if (current_inode < 0) {
				for (int j = 0; j < components_count; j++) free(path_components[j]);
				free(path_components);
				free(temp_path);
				printf("READDIR finish \n");
				return -ENOENT;
			}
			current_inodeblock = &FS.inodetable[current_inode];
		}
		for (int i = 0; i < components_count; i++) free(path_components[i]);
		free(path_components);
	}
	
	// file list 생성
	// filler 함수 사용하여 buffer에 파일 이름 추가
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	
	for (int i = 0; i < FS.file_count; i++) {
		for (int j = 0; j < MAX_BLOCKS; j++) {
			if (current_inodeblock->datablock[i]->dirent[j].d_ino > 0)
				filler(buf, current_inodeblock->datablock[i]->dirent[j].d_name, NULL, 0, 0);
		}
	}

	free(temp_path);
	printf("READDIR finish \n");
	return 0;
}

static int ot_open(const char *path, struct fuse_file_info *fi)
{
	printf("OPEN start \n");
	
	
	
	
	return 0;
}

static int ot_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("@@@@@@@READ start @@@@@@@\n");
	size_t len;
	(void) fi;
	
	if (strcmp(path, hello_path) != 0){
		printf("@@@@@@@READ finish @@@@@@@\n");
		return -ENOENT;
	}
		

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	}
	else 
		size = 0;

	printf("@@@@@@@READ finish @@@@@@@\n");
	return size;
}


static const struct fuse_operations ot_oper = {
	.init       = ot_init,
	.getattr	= ot_getattr,
	.readdir	= ot_readdir,
	.open		= ot_open,
	.read		= ot_read,

	/*
	.mkdir		= ot_mkdir,
	.rmdir		= ot_rmdir,
	
    .create		= ot_create,
	
    	.write		= ot_write,
        .unlink		= ot_unlink,
        .utimens        = ot_utimens,
	.release	= ot_release,
	.rename		= ot_rename,	
	.destroy	= ot_destroy,
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
