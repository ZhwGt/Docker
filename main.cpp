#include "docker.hpp"
#include <iostream>

int main(int argc, char** argv){
//int main(void){
  std::cout << "...start container" << std::endl;

  docker::container_config config;
  config.host_name = "zhanghaowei";
  config.root_dir  = "./fs";

  // 配置网络参数
  config.ip          = "172.17.0.100"; // 容器 IP
  config.bridge_name = "docker0";      // 宿主机网桥
  config.bridge_ip   = "172.17.0.1";   // 宿主机网桥 IP
  
  // 配置容器
  // ...
  
  docker::container container(config); // 根据 config 构造容器
  container.start();                     // 启动容器
  std::cout << "stop container..." << std::endl;
  return 0;
}
