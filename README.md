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
