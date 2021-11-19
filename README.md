# XlaoTinyWebServer

## 这是什么

本项目参考游双的《Linux高性能服务器编程》和 qinguoyi 前辈的 **[ TinyWebServer](https://github.com/qinguoyi/TinyWebServer)**，自制实现一个 Linux 下 C++ 轻量级的 Web 服务器，该服务器拥有以下特性：

- 半同步/半反应堆线程池 + epoll（LT + ET）+ 同步 I/O 模拟 Proactor的并发模型。
- 使用主从状态机处理 http 请求，支持 GET 和 POST 请求。
- Web 实现注册、登录、查看图片和视频的功能。
- 使用日志系统记录服务器运行状态，日志系统支持同步/异步，异步使用循环数组实现。
- 使用定时器处理非活跃连接，定时器容器为时间堆。

目前该服务器已经部署上线，欢迎通过 `1.117.27.35:9777`访问 。

>  由于云服务器带宽较小，视频可能加载不出来XD

## 如何使用

### 使用前的环境配置

#### 服务器环境

- ArchLinux 5.15.2
- MariaDB 10.6.5

#### 浏览器环境

- Window、Linux
- Chrome
- FireFox

#### 1. 数据库的初始化

```
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    passwd char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, passwd) VALUES('name', 'passwd');
```

#### 2. 修改 main.cpp 中的 connPool->init 参数

```
connPool->init("localhost", "root", "yourpasswd", "databaseName", 3306, 8);
```

#### 3. 修改 http_conn.cpp 中的 doc_root 路径

```
const char* doc_root = "/home/acg/xlaoTinyWebServer/root"
```

#### 4. 自由组合 listenfd 和 connfd 的模式（LT/ET）和日志模式

```
//main.cpp:

//#define SYNLOG
#define ASYNLOG				//异步写日志

#define listenET			//listenfd为 ET
//#define listenLT		
---------------------------------------------
// http_conn.cpp

#define connfdET			//connfd为 ET
//#define connfdLT

#define listenET			//listenfd为 ET
//#define listenLT	
```



### 开启服务器

#### 1. 生成server

```
make server
```

> 由于项目使用了 C++11 语法，部分编译器默认不支持，如果执行以上操作失败，请在 makefile 中的 "g++" 后添加 "--std=c++11"，再次执行以上操作。

### 2. 启动 server

> 如果使用的 mysql 是 root 用户，需要在一下语句前面添加 "sudo"

```
./server port
```

#### 3. 浏览器

```
127.0.0.1:port
```



### 压力测试

#### ET + ET，11640 QPS

![](/home/acg/xlaoTinyWebServer/root/source/webbenchTest/ET+ET.png)


#### LT + LT，10677 QPS

![](/home/acg/xlaoTinyWebServer/root/source/webbenchTest/LT+LT.png)



## 感谢

游双编写的《Linux高性能服务器编程》。

@[qinguoyi](https://github.com/qinguoyi)的 TinyWebServer 项目和庖丁解牛分析。
