# C++ 实现docker容器

### 简介
Docker的本质是使用LXC实现类似虚拟机的功能，进而节省的硬件资源提供给用户更多的计算资源。本项目将C++与Linux的Namespace及Control Group技术相结合，实现一个Docker容器。

### 项目设计内容
* Linux namespace, control group.
* Linux系统调用clone, chdir, chroot, sethostname, mount, execv等.
* C++ 命名空间, lambda表达式, STL, C/C++混合编译, Makefile.

### 效果
* 独立的文件系统.
* 网络访问的支持.
* 容器资源的限制，调优.

### Linux namespace 技术
在C++中，namespace 是关键字, 每一个 namespace 对不同代码的相同名称进行了隔离，只要 namespace 的名称不同，就能够让 namespace 中的代码名称相同，从而解决了代码名称冲突的问题。
Linux Namespace 则是 Linux 内核提供的一种技术，它为应用程序提供了一种资源隔离的方案，和 C++ 中的 namespace 有异曲同工之妙。PID、IPC、网络等系统资源本应该属于操作系统本身进行管理，但 Linux Namespace 则可以让这些资源的全局性消失，让一部分属于某个特定的 Namespace。

在 Docker 技术中，时常听说 LXC、操作系统级虚拟化这些名词，而 LXC 就是利用了 Namespace 这种技术实现了不同容器之前的资源隔离。利用 Namespace 技术，不同的容器内进程属于不同的 Namespace，彼此不相干扰。总的来说，Namespace 技术，提供了虚拟化的一种轻量级形式，使我们可以从不同方面来运行系统全局属性。

在 Linux 中，和 Namespace 相关的系统调用最重要的就是 clone()。 clone() 的作用是在创建进程时，将线程限制在某个 Namespace 中。

### 系统调用封装
Linux 系统调用都是由 C 语言写成的，我们要编写的是 C++ 相关的代码，为了让整套代码不是出于 C 和 C++ 混搭的风格，我们先对这些必要的 API 进行一层 C++ 形式的封装。
clone 和 fork 两个系统调用的功能非常类似，他们都是 linux 提供的创建进程的系统调用。
clone 的函数原型如下：
```cpp
int clone(int (*fn)(void *), void *child_stack, int flags, void *arg);
```
fork 在创建一个进程的时候，子进程会完全复制父进程的资源。而 clone 则比 fork 更加强大，因为 clone 可以有选择性的将父进程的资源复制给子进程，而没有复制的数据结构则通过指针的复制让子进程共享(arg)，具体要复制的资源，则可以通过 flags 进行指定，并返回子进程的 PID。

进程四个要素：
* 一段需要执行的程序.
* 进程自己的专用堆栈空间.
* 进程控制块（PCB）.
* 进程专有的 namespace.

|  namespace分类 | 系统调用参数 |
|  ----  | ----  |
|  UTS  |  CLONE_NEWUTS  |
| Mount | CLONE_NEWNS |
| PID  | CLONE_NEWPID |
| Network | CLONE_NEWNET |
从名字可以看出，CLONE_NEWNS 提供了文件系统相关的挂载，用于复制和文件系统相关的资源，CLONE_NEWUTS 则提供了主机名相关的设置，CLONE_NEWPID 提供了独立的进程空间支持，而 CLONE_NEWNET 则提供了网络相关支持。

```cpp
int execv(const char *path, char *const argv[]);
```
execv 可以通过传入一个 path 来执行 path 中的执行文件，这个系统调用可以让我们的子进程执行 /bin/bash 从而让整个容器保持运行。
```cpp
int sethostname(const char *name, size_t len);
```
从名字不难看出，这个系统调用能够设置我们的主机名，值得一提的是，由于 C 风格的字符串使用的是指针，不指定长度是无法直接从内部得知字符串长度的，这里 len 起到了获得字符串长度的作用。
```cpp
int chdir(const char *path);
```
任何一个程序，都会在某个特定的目录下运行。当我们需要访问资源时，就可以通过相对路径而不是绝对路径来访问相关资源。而 chdir 恰好提供给了我们一个便利之处，那就是可以改变我们程序的运行目录，从而达到某些不可描述的目的。

这个系统调用能够用于设置根目录
```cpp
int chroot(const char *path);
```
这个系统调用用于挂载文件系统，和 mount 这个命令能够达到相同的目的。
```cpp
int mount(const char *source, const char *target,
                 const char *filesystemtype, unsigned long mountflags,
                 const void *data);
```

Docker 网络原理
![image](https://user-images.githubusercontent.com/51261084/135478972-cb33a0bb-5acc-45ff-ae7d-37dccbabcd94.png)

Docker 容器之间的网络通信原理是通过一个网桥 docker0 来实现的。两个容器 container1 和 container2 彼此具备各自的网络设备 eth0，所有的网络请求，都将通过 eth0 向外进行传递。而由于容器生活在子进程中，因此为了让彼此的 eth0 能够互通，那么就需要多创建一对网络设备 veth1 和 veth2，让他们添加到网桥 docker0 上，这样当容器内部的 eth0 向外产生网络访问时，网桥会无条件进行转发，具备路由的功能，从而使容器之间获得网络获得通信的能力。

因此，我们编写的容器要具备网络通信的能力，我们先为容器创建一个需要使用的网桥。而为了方便起见，我们直接使用实验楼环境中的 docker0（也就是实验楼中安装好的 Docker 创建的网桥）

Makefile文件
```makefile
C = gcc
CXX = g++
C_LIB = network.c nl.c
C_LINK = network.o nl.o
MAIN = main.cpp
LD = -std=c++11
OUT = docker-run

all:
        make container
container:
        $(C) -c $(C_LIB)
        $(CXX) $(LD) -o $(OUT) $(MAIN) $(C_LINK)
clean:
        rm *.o $(OUT)

```

### 展示图
![image](https://user-images.githubusercontent.com/51261084/135480095-c43639c5-3d55-4647-b172-f413fd8ad69e.png)
![image](https://user-images.githubusercontent.com/51261084/135480276-d1a5a186-68b0-4738-ac02-86db1d35c1e2.png)
![image](https://user-images.githubusercontent.com/51261084/135481914-2bb0afa3-3cdc-4762-8661-805d544c4ad1.png)
![image](https://user-images.githubusercontent.com/51261084/135482028-1e1c3d63-220d-4b32-9eaa-da44dc8ee2b2.png)![image](https://user-images.githubusercontent.com/51261084/135482118-c96119b4-ef6e-4c3d-ab37-1dd5a4cab95f.png)
![image](https://user-images.githubusercontent.com/51261084/135482208-dcd3d12f-37a8-447e-b011-74d7cf1fa05b.png)
![image](https://user-images.githubusercontent.com/51261084/135482843-72d36f27-f4bd-4e8f-b321-ab090db0c1bc.png)

![image](https://user-images.githubusercontent.com/51261084/135483255-7a8e3f6e-17c5-46dc-84fa-2e3e89dbbce8.png)

![image](https://user-images.githubusercontent.com/51261084/135483390-bb649c23-5571-47d3-b49c-94f284eafb20.png)


