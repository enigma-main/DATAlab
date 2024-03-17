---
aliases: 
type: Summary
tags:
  - OS/FileSystem
  - OS/OSTEP
date created: " 2023-12-27 16:43"
date modified:
---
---
### References 
OSTEP
### Connections
[[extents-based approach]]
[[linked-based approach]]
[[pre-allocation]]
[[B-Tree]]
[[25 File System Implementation]]
# Contents 
Keyword
- How to Implement File System 
- What on-disk structure are needed 
- Track, Access method

## 40.1 The Way To Think
What data structure needed?
How access?

## 40.2 Overall Organization 
Block: Disk를 작은 사이즈로 나눈 것 (4KB)
Secter: Disk를 큰 사이즈로 나눈 것

data region: user data가 저장된 data block이 위치한 disk region 
inode table: metadata가 저장된 inode가 위치한 disk region 
allocation structure: inode bitmap, data bitmap 등 allocation 구현에 필요한 free space를 관리하는 disk region. free list로도 구현 가능
superblock: file system의 metadata가 저장된 block 

## 40.3 File Organization: The Inode 
inode: index node의 줄임말. metadata (length, permissions, location, name, access time 등)
inode 시작 위치, inode size, block size, sector size


