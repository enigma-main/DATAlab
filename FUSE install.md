---
aliases: 
type: 
tags:
  - debug
  - install
  - FUSE
date created: " 2024-01-26 16:25"
date modified:
---
---
### References 
https://github.com/libfuse/libfuse?tab=readme-ov-file
### Connections 
[[FUSE]] 
# Contents 
### Guide
해당 페이지 참고해서 설치 (아래 설명해뒀으나 이해 안 될시 연락)
https://github.com/libfuse/libfuse/releases 서 파일 받고 압축 푼 뒤
```shell
$ tar xzf fuse-X.Y.Z.tar.gz; cd fuse-X.Y.Z
$ mkdir build; cd build
$ meson setup ..
$ ninja
$ sudo python3 -m pytest test/ (안되면 패스)
$ sudo ninja install
```

### Debug
ninja까지 실행했고 build 했는데 중간에 이런 문제 발생했는데 build는 잘 되었다고 해서 무시하고 넘어감
```Shell
$ meson setup .. The Meson build system Version: 0.61.2 Source dir: /home/today/datalab/fuse-3.16.2 Build dir: /home/today/datalab/fuse-3.16.2/build Build type: native build Project name: libfuse3 Project version: 3.16.2 C compiler for the host machine: ccache cc (gcc 11.4.0 "cc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0") C linker for the host machine: cc ld.bfd 2.38 Host machine cpu family: x86_64 Host machine cpu: x86_64 Checking for function "fork" : YES Checking for function "fstatat" : YES Checking for function "openat" : YES Checking for function "readlinkat" : YES Checking for function "pipe2" : YES Checking for function "splice" : YES Checking for function "vmsplice" : YES Checking for function "posix_fallocate" : YES Checking for function "fdatasync" : YES Checking for function "utimensat" : YES Checking for function "copy_file_range" : YES Checking for function "fallocate" : YES Checking for function "setxattr" : YES Checking for function "iconv" : YES Checking whether type "struct stat" has member "st_atim" : YES Checking whether type "struct stat" has member "st_atimespec" : NO Message: Compiler warns about unused result even when casting to void Message: Enabling versioned libc symbols Message: Compiler supports symver attribute Configuring fuse_config.h using configuration Configuring libfuse_config.h using configuration Run-time dependency threads found: YES Library iconv found: NO Library dl found: YES Library rt found: YES Did not find pkg-config by name 'pkg-config' Found Pkg-config: NO Did not find CMake 'cmake' Found CMake: NO Run-time dependency udev found: NO (tried pkgconfig and cmake) ../util/meson.build:26: WARNING: could not determine udevdir, udev.rules will not be installed C++ compiler for the host machine: ccache c++ (gcc 11.4.0 "c++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0") C++ linker for the host machine: c++ ld.bfd 2.38 Build targets in project: 29 Found ninja-1.10.2 at /home/today/anaconda3/bin/ninja
```
```Shell
ksh@localhost:~/datalab/fuse-3.16.2/build$ meson setup ..
The Meson build system
Version: 0.61.2
Source dir: /home/ksh/datalab/fuse-3.16.2
Build dir: /home/ksh/datalab/fuse-3.16.2/build
Build type: native build
Project name: libfuse3
Project version: 3.16.2
C compiler for the host machine: cc (gcc 11.3.0 "cc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0")
C linker for the host machine: cc ld.bfd 2.38
Host machine cpu family: x86_64
Host machine cpu: x86_64
Checking for function "fork" : YES
Checking for function "fstatat" : YES
Checking for function "openat" : YES
Checking for function "readlinkat" : YES
Checking for function "pipe2" : YES
Checking for function "splice" : YES
Checking for function "vmsplice" : YES
Checking for function "posix_fallocate" : YES
Checking for function "fdatasync" : YES
Checking for function "utimensat" : YES
Checking for function "copy_file_range" : YES
Checking for function "fallocate" : YES
Checking for function "setxattr" : YES
Checking for function "iconv" : YES
Checking whether type "struct stat" has member "st_atim" : YES
Checking whether type "struct stat" has member "st_atimespec" : NO
Message: Compiler warns about unused result even when casting to void
Message: Enabling versioned libc symbols
Message: Compiler supports symver attribute
Configuring fuse_config.h using configuration
Configuring libfuse_config.h using configuration
Run-time dependency threads found: YES
Library iconv found: NO
Library dl found: YES
Library rt found: YES
Did not find pkg-config by name 'pkg-config'
Found Pkg-config: NO
Did not find CMake 'cmake'
Found CMake: NO
Run-time dependency udev found: NO (tried pkgconfig and cmake)
../util/meson.build:26: WARNING: could not determine udevdir, udev.rules will not be installed
C++ compiler for the host machine: c++ (gcc 11.3.0 "c++ (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0")
C++ linker for the host machine: c++ ld.bfd 2.38
Build targets in project: 29

Found ninja-1.10.1 at /usr/bin/ninja
```
