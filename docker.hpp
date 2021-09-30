#include <sys/wait.h>   // waitpid
#include <sys/mount.h>  // mount
#include <fcntl.h>      // open
#include <unistd.h>     // execv, sethostname, chroot, fchdir
#include <sched.h>      // clone
#include <net/if.h>     // if_nametoindex
#include <arpa/inet.h>  // inet_pton
#include "network.h"

#include <cstring>
#include <string>

#define STACK_SIZE (512 * 512) //子进程空间大小

namespace docker{
  typedef int proc_statu;
  proc_statu proc_err = -1;
  proc_statu proc_exit = 0;
  proc_statu proc_wait = 1;

  /* 容器配置结构体 */
  struct container_config{
    std::string host_name;   // 主机名
    std::string root_dir;    // 容器根目录
    std::string ip;          // 容器 IP
    std::string bridge_name; // 网桥名
    std::string bridge_ip;   // 网桥 IP
  };

  /* 容器类 */
  class container {
  private:
    typedef int process_pid;      // 增强可读性
    char child_stack[STACK_SIZE]; // 子进程栈
    container_config config;      // 容器配置

    // 保存容器网络设备, 用于删除
    char *veth1;
    char *veth2;

    void start_bash() {
      std::string bash = "/bin/bash"; // 定义bash路径
      char *c_bash = new char[bash.length()+1]; // +1 用于存放 '\0'
      strcpy(c_bash, bash.c_str());
    
      char* const child_args[] = { c_bash, NULL };
      execv(child_args[0], child_args); // 在子进程中执行 /bin/bash
      delete []c_bash;
    }

    void set_hostname(){
      sethostname(this->config.host_name.c_str(), this->config.host_name.length());
    }

    void set_rootdir() {

      // chdir 系统调用, 切换到某个目录下
      chdir(this->config.root_dir.c_str());

      // chrrot 系统调用, 设置根目录, 因为刚才已经切换过当前目录
      // 故直接使用当前目录作为根目录即可
    chroot(".");
    }

    // 设置独立的进程空间
    void set_procsys() {
      // 挂载 proc 文件系统
      mount("none", "/proc", "proc", 0, nullptr);
      mount("none", "/sys", "sysfs", 0, nullptr);
    }
    
    void set_network() {
      int ifindex = if_nametoindex("eth0");
      struct in_addr ipv4;
      struct in_addr bcast;
      struct in_addr gateway;

      // IP 地址转换函数，将 IP 地址在点分十进制和二进制之间进行转换
      inet_pton(AF_INET, this->config.ip.c_str(), &ipv4);
      inet_pton(AF_INET, "255.255.255.0", &bcast);
      inet_pton(AF_INET, this->config.bridge_ip.c_str(), &gateway);
 
      // 配置 eth0 IP 地址
      lxc_ipv4_addr_add(ifindex, &ipv4, &bcast, 16);

      // 激活 lo
      lxc_netdev_up("lo");

      // 激活 eth0
      lxc_netdev_up("eth0");

      // 设置网关
      lxc_ipv4_gateway_add(ifindex, &gateway);

      // 设置 eth0 的 MAC 地址
      char mac[18];
      new_hwaddr(mac);
      setup_hw_addr(mac, "eth0");
    }

  public:
    container(container_config &config) { // 构造函数
      this->config = config;
    }
    
    ~container(){
      // 退出时删除虚拟网络设备
      lxc_netdev_delete_by_name(veth1);
      lxc_netdev_delete_by_name(veth2);
    }

    void start(){
      char veth1buf[IFNAMSIZ] = "shiyanlou0X";
      char veth2buf[IFNAMSIZ] = "shiyanlou0X";
      // 创建一对网络设备, 一个用于加载到宿主机, 另一个用于转移到子进程容器中
      veth1 = lxc_mkifname(veth1buf); // lxc_mkifname 这个 API 在网络设备名字后面至少需要添加一个 "X" 来支持随机创建虚拟网络设备
      veth2 = lxc_mkifname(veth2buf); // 用于保证网络设备的正确创建, 详见 network.c 中对 lxc_mkifname 的实现
      lxc_veth_create(veth1, veth2);

      // 设置veth1的MAC地址
      setup_private_host_hw_addr(veth1);

      // 将veth1 添加到网桥中
      lxc_bridge_attach(config.bridge_name.c_str(), veth1);
      
      // 激活veth1
      lxc_netdev_up(veth1);

      auto setup = [](void* args)->int{
        auto _this = reinterpret_cast<container *>(args);

        // 对容器进行相关配置
        _this->set_hostname();
        _this->set_rootdir();
        _this->set_procsys();
        _this->set_network(); // 容器内部配合网络配置
        _this->start_bash();
        // ...

        return proc_wait;
      };

      auto child_pid = clone(setup, child_stack, 
                      CLONE_NEWUTS|
                      CLONE_NEWNS|
                      CLONE_NEWPID|
                      CLONE_NEWNET|
                      SIGCHLD,      // 子进程退出时会发出信号给父进程
                      this);

      // 将 veth2 转移到容器内部, 并命名为 eth0
      lxc_netdev_move_by_name(veth2, child_pid, "eth0");

      waitpid(child_pid, nullptr, 0); // 等待子进程的退出
    }
  };
}
