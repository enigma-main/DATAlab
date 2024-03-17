---
aliases: 
type: 
tags:
  - OS/FileSystem
  - UserSpace
  - FUSE
date created: " 2024-01-26 16:23"
date modified:
---
---
### References 
https://en.wikipedia.org/wiki/Filesystem_in_Userspace
### Connections 
[[FUSE install]]
[[FUSE run]]
[[filebench|FUSE evaluate]]
[[File System Implementation]]
# Contents 
Filesystem in Userspace 라는 뜻

high-level API : 파일의 name, path를 이용
low-level API : 파일의 inode를 이용


# libfuse 
[[libfuse]]

Unix, Unix-like OS에서 kernel을 직접 수정하지 않고, User가 priviledge를 얻지 않고, 독자적으로 file system을 만들어 사용할 수 있는 소프트웨어 인터페이스
linux, window, mac 등에서 사용 가능
FUSE는 virtual file system이다. 


# Simple File System 
struct를 어떻게 만들어야 하지?

inodetable에 가장 유사한 것은 struct stat 

```C
struct Inodeblock {
    char filename[MAX_FILENAME_LENGTH];
    int start_blocks;
    int block_count;
    int permission;
    char owner[MAX_FILENAME_LENGTH];
    // int last_access; // tick, mytick, clock() 등으로 구현할 수 있으려나
    // n_link
} ;

 
struct Datablock {
    int data[BLOCK_SIZE];
} ;
  
struct {
    // superblock은 구현 x
    int inodebitmap[BLOCK_SIZE];
    int databitmap[BLOCK_SIZE];
    struct Inodeblock inodetable[MAX_FILES];
    struct Datablock datablock[MAX_BLOCKS];
    int file_count;
} FS;

struct stat {
	__dev_t st_dev;
	__ino_t st_ino;
	__nlink_t st_nlink;
	__mode_t st_mode;
	__uid_t st_uid;
	gid 
	st_rdev;
	st_size;
	st_blksize; (optional blocksize for i/o)
	st_blocks;
	st_atime; // last access
	st_atimensec;
	st_mtime; // last modify
	st_ctime; // last status change
}

```

### 질문
- directory 하나가 가질 수 있는 파일의 수는 몇 개 일까?
int 처럼 1024? 아니면 directory 크기를 고려해서? 아니면 direct pointer 수 13개?

- datablock regular file은 어떤 data type으로 값을 저장해야 하지?


- lock 필요하지 않을까. open 하고나서. open 때문에 lock을 걸면 dead lock 발생할 것 같
- datablock에 랜덤하게 저장되어 있을까? 순차적으로 저장되어 있을까?
- path는 상대주소일까 절대주소일까
	- find_inode 함수에서 current_inode 초기값을 root 혹은 현재 위치로 결정하려면 알아야 함
- 왜 inode ROOT = 2로 해야할까? inode 0과 1은 낭비되는 공간인가?
- superblock에는 무슨 데이터가 들어가야 하지?
- datablock의 data 크기, inode의 크기, directory entry의 크기, 그리고 각 변수의 개수는 왜 그렇게 정의되었지?
- int check_size(size_t size) {} <가 맞을까? <=가 맞을까?
- inode의 size는 어떻게 정의되어야 할까? 2번째 정의 채택
	- size는 block 개수로 계산한다. (block_count * 4096)
	- **size는 실제로 write한 값 (offset 개념) 으로 정의한다** 
- write, read 할 때 datablock에 invalid data가 있어도 blocks_count로 invalid 안 읽을 수 있을까?

#### History
libfuse
[[GNU 1]]
License

https://maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
