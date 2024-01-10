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

int find_next_inode(const char *name, const struct st_INODE *iblock) {
    // "/mnt/c/" "/mnt/myFS/"
    // input "myFS", "/mnt/" inode pointer
    // output "/mnt/myFS/" inode index
	printf("find_next_inode start\n");
	for (int j = 0; j < DIRENT_SIZE; j++) {
		if (strcmp(iblock->datablock[0]->dirent[j].d_name, name) == 0){
			printf("find_next_inode finish\n");
			return iblock->datablock[0]->dirent[j].d_ino;
		}
	}
	printf("find_next_inode finish\n");
    // for (int i = 0; i < MAX_DIRECT_POINTER; i++) {
	// 	for (int j = 0; j < BLOCK_SIZE; j++) {
	// 		if (strcmp(iblock->datablock[i]->dirent[j].d_name, name) == 0)
    //         	return iblock->datablock[i]->dirent[j].d_ino;
    //     }
    // }
    return -1;  // Directory not found
}

int find_empty_inode() {
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
int find_empty_dirent() {
	for (int i = 0; i < MAX_FILES; i++) {
		if (FS.inodebitmap[i] == 0) {
			return i;
		}
	}
	return -ENOMEM;
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
	
	// root directory 부터 recursively path 탐색
	int current_inode = ROOT;
	printf("find_inode: comp_count: %d, cur_inode: %d\n", components_count, current_inode);
	for (int i = 0; i < components_count; i++){
		printf("%s, ", path_components[i]);
	}
	printf("\n");


	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	for (int i = 0; i < components_count; i++) {
		current_inode = find_next_inode(path_components[i], current_inodeblock);
		if (current_inode < 0) {
			for (int j = 0; j < components_count; j++) free(path_components[j]);
			free(path_components);
			free(temp_path);
			return -ENOENT;
		}
		current_inodeblock = &FS.inodetable[current_inode];
	}

	return current_inode;
}

int find_parent_inode(const char *path) {
    // root 디렉터리인 경우
    if (strcmp(path, "/") == 0) {
        return -EPERM;
    }

    char *temp_path = strdup(path); // 경로 복사
    int components_count;
    char **path_components = split_path(temp_path, &components_count);

    int current_inode = ROOT;
    printf("find_parent: comp_count: %d, cur_inode: %d\n", components_count, current_inode);
	for(int i = 0; i < components_count; i++){
		printf("%s, ", path_components[i]);
	}
	printf("\n");
	
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	
    

	// 부모 디렉터리까지 탐색
    for (int i = 0; i < components_count - 1; i++) {
        current_inode = find_next_inode(path_components[i], current_inodeblock);

        if (current_inode < 0) {
            // 오류 처리: 디렉터리가 존재하지 않음
            for (int j = 0; j < components_count; j++) {
                free(path_components[j]);
            }
            free(path_components);
            free(temp_path);
            return -ENOENT;
        }

        current_inodeblock = &FS.inodetable[current_inode];
    }
	
    // 메모리 정리
    for (int j = 0; j < components_count; j++) {
        free(path_components[j]);
    }
	printf("free: path_comp\n");
    free(path_components);
	printf("free: path_comps\n");
    free(temp_path);
	printf("free: temp_path\n");

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
	
	memset(&FS, 0, sizeof(struct _FileSystem));
	printf("memset complete \n");
	
	FS.inodebitmap[ROOT] = 1;
	FS.databitmap[0] = 1;
	printf("bitmap complete \n");

	FS.inodetable[ROOT].datablock[0] = &FS.datablock[0];
	printf("datablock allocation \n");

	FS.inodetable[ROOT].datablock[0]->dirent[0].d_ino = 0;
	strcpy(FS.inodetable[ROOT].datablock[0]->dirent[0].d_name, ".");
	FS.inodetable[ROOT].datablock[0]->dirent[1].d_ino = 0;
	strcpy(FS.inodetable[ROOT].datablock[0]->dirent[1].d_name, "..");
	printf("datablock complete \n");
	
	FS.inodetable[ROOT].mode = S_IFDIR;
	FS.inodetable[ROOT].links_count = 2;
	FS.inodetable[ROOT].blocks_count = 1;
	FS.inodetable[ROOT].size = GET_SIZE(FS.inodetable[ROOT]);
	printf("metadata complete \n");
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
	printf("MAX_DIRENT: %ld\n", DIRENT_SIZE);
}

// getattr -> int
static int ot_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	(void) fi;
	printf("GETATTR start \n");
	printf("path: %s\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	printf("memset finished\n");
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
	char *temp_path = strdup(path); // string duplication
	int components_count;
	char **path_components = split_path(temp_path, &components_count);
	
	// root directory 부터 recursively path 탐색
	int current_inode = find_inode(path);
	printf("inode: %d\n", current_inode);
	if (current_inode < 0) {
		printf("GETATTR finish \n");
		return -ENOENT;
	}

	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
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
	printf("path: %s\n", path);

	memset(buf, 0, sizeof(void*));

	char *temp_path = strdup(path); // string duplication
	// char *base_name = basename(temp_path); 

	int current_inode = find_inode(path);
	printf("inode: %d\n", current_inode);

	if (current_inode < 0) {
		printf("READDIR FINISH \n");
		return -ENOENT;
	}
	
	// file list 생성
	// filler 함수 사용하여 buffer에 파일 이름 추가
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];

	if ((current_inodeblock->mode & S_IFDIR) != S_IFDIR)
		return -ENOTDIR;

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
	printf("OPEN START \n");
	printf("path: %s\n", path);

	// check file not found: return -ENOENT
	int current_inode = find_inode(path);
	printf("inode: %d\n", current_inode);
	if (current_inode < 0) {
		printf("OPEN FINISH \n");
		return -ENOENT;
	}

	// check permission: EACCES
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	printf("mode: %d\n", current_inodeblock->mode);
	if ((fi->flags & O_ACCMODE) == O_RDONLY){
		if ((current_inodeblock->mode & 04) == 04){
			printf("OPEN FINISH \n");
			return 0;
		}
	}
	else if ((fi->flags & O_ACCMODE) == O_RDONLY) {
		if ((current_inodeblock->mode & 02) == 02){
			printf("OPEN FINISH \n");
			return 0;
		}
	}
	
	else {
		if ((current_inodeblock->mode & 06) == 06){
			printf("OPEN FINISH \n");
			return 0;
		}
	}
	printf("OPEN FINISH \n");
	return -EACCES;
}

static int ot_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("READ start \n");
	printf("path: %s\n", path);
	size_t len;
	(void) fi;
	
	memset(buf, 0, size);

	int current_inode = find_inode(path);
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	
	len = current_inodeblock->size;
	
	memcpy(buf, current_inodeblock->datablock, size);
	// if (offset < len) {
	// 	if (offset + size > len)
	// 		size = len - offset;
	// 	memcpy(buf, current_inodeblock->datablock + offset, size);
	// }
	// else 
	// 	size = 0;

	printf("READ finish \n");
	return size;
}

static int ot_mkdir(const char *path, mode_t mode)
{
	printf("MKDIR start \n");
	printf("path: %s\n", path);

	// check parent inode
	int parent_inode = find_parent_inode(path);
	printf("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		printf("MKDIR finish \n");
		return -ENOENT;
	}
	struct st_INODE *parent_inodeblock = &FS.inodetable[parent_inode];
	
	// allocate new inode
	int current_inode = find_empty_inode();
	printf("inode: %d\n", current_inode);
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];
	// allocate new directory entry
	int current_datablock = find_empty_datablock();
	printf("datablock: %d\n", current_datablock);

	// allocate datablock in inode
	current_inodeblock->datablock[0] = &FS.datablock[current_datablock];
	printf("cur: inode datablock mapping: inode %d -> datablock %d\n", current_inode, current_datablock);
	
	// add ., .. in current datablock
	strcpy(current_inodeblock->datablock[0]->dirent[0].d_name, ".");
	current_inodeblock->datablock[0]->dirent[0].d_ino = current_inode;
	strcpy(current_inodeblock->datablock[0]->dirent[1].d_name, "..");
	current_inodeblock->datablock[0]->dirent[1].d_ino = parent_inode;
	printf("cur: datablock ., .. add\n");

	// link new inode in parent datablock
	int flag = false;
	for (int j = 2; j < DIRENT_SIZE; j++) {
		if (parent_inodeblock->datablock[0]->dirent[j].d_ino == 0){
			flag = true;
			parent_inodeblock->datablock[0]->dirent[j].d_ino = current_inode;
			char *temp_path = strdup(path); // string duplication
			char *base_name = basename(temp_path); 
			strcpy(parent_inodeblock->datablock[0]->dirent[j].d_name, base_name);
			printf("pinode: %d, dirent[%d].d_ino = %d, name: %s\n", parent_inode, j, current_inode, base_name);
			printf("pinode: %d, dirent[%d].d_ino = %d, name: %s\n", parent_inode, j, parent_inodeblock->datablock[0]->dirent[j].d_ino, parent_inodeblock->datablock[0]->dirent[j].d_name);
			break;
		}
	}
	if (flag) printf("parent: link cur\n");
	else printf("link failed!!!!!\n");

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
	current_inodeblock->mode = S_IFDIR;
	FS.databitmap[current_datablock] = 1;
	FS.inodebitmap[current_inode] = 1;

	current_inodeblock->blocks_count = 1;
	current_inodeblock->size = GET_SIZE((*current_inodeblock));
	current_inodeblock->links_count = 2;
	parent_inodeblock->links_count += 1;
	printf("metadata\n");

	printf("MKDIR finish \n");
	return 0;
}

static int ot_rmdir(const char *path)
{
	printf("RMDIR start \n");
	printf("path: %s\n", path);

	// check parent inode
	int parent_inode = find_parent_inode(path);
	printf("parent inode: %d\n", parent_inode);
	if (parent_inode < 0) {
		printf("RMDIR finish \n");
		return -ENOENT;
	}
	struct st_INODE *parent_inodeblock = &FS.inodetable[parent_inode];
	
	// allocate new inode
	int current_inode = find_inode(path);
	printf("cur_inode: %d\n", current_inode);
	if (current_inode < 0) {
		printf("RMDIR finish \n");
		return -ENOENT;
	}
	struct st_INODE *current_inodeblock = &FS.inodetable[current_inode];

	// check is directory
	if ((current_inodeblock->mode & S_IFDIR) != S_IFDIR) {
		printf("RMDIR finish\n");
		return -ENOTDIR;
	}

	// check directory is empty
	// dirent 전체를 scan.
	// -> size 혹은 block_count로 개선하면 좋을 것 같음
	for (int j = 2; j < DIRENT_SIZE; j++){
		if (current_inodeblock->datablock[0]->dirent[j].d_ino != 0) {
			printf("RMDIR finish\n");
			return -EEXIST;
		}
	}

	// memset datablock in inode
	memset(current_inodeblock->datablock[0], 0, sizeof(union u_DATABLOCK));
	current_inodeblock->datablock[0] = 0;
	
	// link new inode in parent datablock
	for (int j = 2; j < DIRENT_SIZE; j++) {
		if (parent_inodeblock->datablock[0]->dirent[j].d_ino == current_inode){
			parent_inodeblock->datablock[0]->dirent[j].d_ino = 0;
			strcpy(parent_inodeblock->datablock[0]->dirent[j].d_name, "");
			break;
		}
	}

	// set metadata
	memset(current_inodeblock, 0, sizeof(struct st_INODE));

	// datablock의 pointer를 사용하지 않고 datablock의 index를 사용하도록 코드수정 필요
	// FS.databitmap[current_datablock] = 1;
	FS.inodebitmap[current_inode] = 1;

	parent_inodeblock->links_count -= 1;
	printf("metadata\n");

	printf("RMDIR finish \n");
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

	/*
    .create		= ot_create,
	.write		= ot_write,
	.unlink		= ot_unlink,
	.utimens    = ot_utimens,
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
