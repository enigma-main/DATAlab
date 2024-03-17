---
aliases: 
type: 
tags: 
date created: " 2024-01-26 16:31"
date modified:
---
---
### References 

### Connections 

# Contents 
### Implementation
제공된 코드 (hello.c)에 코드 작성. 필요한 경우 새로운 헤더파일이나 소스코드 파일 만들어서 사용해도 됨.
mkdir, init, open, read 등 주요한 기능(ot_oper에 주석처리 되어 있음)을 구현한 뒤 ot_oper에 해당하는 함수를 매칭해주면 됨.
(superblock, inode 등의 struct를 만들었다면, init 때 초기화하기 등)
ot_는 예시로 사용되었을 뿐, 원하는 문구로 바꿔서 사용해도 됨.

### Run
1. /mnt/에 directory를 하나 만든다 (MyFS 등)
2. 수정한 코드를 컴파일 한다. gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello -ug 
3. sudo -s 
4. ./hello /mnt/(만든 directory) -d 를 한다.
5. 새 터미널 창을 열어서 sudo -s 후 cd /mnt/(만든 directory)를 해서 directory에 들어간 후 구현한 기능을 확인해본다. (mkdir, ls 등) printf 등으로 로그를 기록해뒀다면 원래 켜놨던 터미널에서 돌아가는 과정을 확인할
