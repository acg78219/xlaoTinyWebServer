//
// Created by acg on 11/14/21.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>

#include "./locker/locker.h"
#include "./threadPool/threadPool.h"
#include "./timer/time_heap.h"
#include "./http/http_conn.h"
#include "./log/log.h"
#include "./CGImysql/sql_connection_pool.h"

#define MAX_FD 65536                // 最大文件描述符
#define MAX_EVENT_NUMBER 10000      // 最大事件数
#define TIMESLOT 5                  // 最小超时时间

//#define SYNLOG                      // 同步写日志
#define ASYNLOG                     // 异步写日志

#define listenfdET                  // 监听非阻塞ET
//#define listenfdLT                  // 监听阻塞LT

// 该三文件在 http_conn.cpp 中
extern int addfd(int epollfd, int fd, bool one_shot);
void removefd(int epollfd, int fd);
int setNonBlocking(int fd);

// 设置定时器相关的参数
static int pipefd[2];                 // 父子进程通信管道，传递信号
static time_heap timer_heap;
static int epollfd = 0;

// 信号处理函数
void sig_handler(int sig)
{
  // 保证重入性，记录原来的 errno
  // 重入性：中断后重新进入该函数，环境变量与之前一样
  int save_errno = errno;
  int msg = sig;

  // 传入信号的信号序号，但是 send 接受字符，转换一下
  send(pipefd[1], (char*)&msg, 1, 0);
  errno = save_errno;
}

// 设置信号函数
// 信号序号、回调函数、被打断的系统调用是否自动重新发起
void addsig(int sig, void(handler)(int), bool restart = true)
{
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = handler;
  if(restart)
    sa.sa_flags |= SA_RESTART;
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，不断定时触发 SIGALRM 信号
void timer_handler()
{
  timer_heap.tick();
  alarm(TIMESLOT);
}

// 定时器回调函数，删除非活跃的socket的注册事件，并关闭
void cb_func(client_data *user_data)
{
  // 1. 从内核事件表中删除事件
  epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
  assert(user_data);
  // 2. 关闭文件fd
  close(user_data->sockfd);
  // 3. 更新连接的用户
  http_conn::m_user_count--;
  LOG_INFO("close fd %d", user_data->sockfd);
  Log::get_instance()->flush();
}

void show_error(int connfd, const char* info)
{
  printf("%s", info);
  send(connfd, info, strlen(info), 0);
  close(connfd);
}

int main(int argc, char* argv[])
{
#ifdef ASYNLOG
  Log::get_instance()->init("ServerLog", 2000, 800000, 8);      // 异步写日志
#endif

#ifdef SYNLOG
  Log::get_instance()->init("ServerLog", 2000, 800000, 0);  // 同步写日志
#endif

  if(argc <= 1)
  {
    printf("you need to input ip、port\n", basename(argv[0]));
    return 1;
  }

  int port = atoi(argv[1]);

  addsig(SIGPIPE, SIG_IGN);

  // 创建数据库连接池
  connection_pool* connPool = connection_pool::getInstance();
  connPool->init("localhost", "root", "laoxinghaoTay57", "test", 3306, 8);

  // 创建线程池
  threadPool<http_conn> *pool = NULL;
  try {
    pool = new threadPool<http_conn>(connPool);
  }
  catch (...)
  {
    return 1;
  }

  http_conn* users = new http_conn[MAX_FD];
  assert(users);

  // 初始化数据库读取表
  users->init_mysql_result(connPool);

  int listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(listenfd >= 0);

  int ret = 0;
  struct sockaddr_in address;
  memset(&address, '\0', sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  int flag = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
  assert(ret >= 0);
  ret = listen(listenfd, 5);
  assert(ret >= 0);

  // 创建内核事件表
  epoll_event events[MAX_EVENT_NUMBER];
  epollfd = epoll_create(5);
  assert(epollfd != -1);

  // 将监听 fd 注册到内核事件表，不能是 one_shot
  addfd(epollfd, listenfd, false);
  http_conn::m_epollfd = epollfd;

  // 创建父子通信管道
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
  assert(ret != -1);
  setNonBlocking(pipefd[1]);    // 写管道不阻塞，写满直接返回errno
  addfd(epollfd, pipefd[0], false);   // 注册管道的读事件

  // 信号处理函数，只关注 alarm 和 ctrl + c 发送的信号
  addsig(SIGALRM, sig_handler, false);
  addsig(SIGTERM, sig_handler, false);
  bool stop_server = false;

  client_data* users_timer = new client_data[MAX_FD];

  bool timeout = false;
  alarm(TIMESLOT);        // 定时触发 alarm


  // 只要不发 SIGTERM，则一直执行下面的语句（服务器一直运行）
  while(!stop_server)
  {
    int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    if(number < 0 && errno != EINTR)
    {
      LOG_ERROR("%s", "epoll failure");
      break;
    }

    for(int i=0; i<number; ++i)
    {
      int sockfd = events[i].data.fd;

      // 如果是新到的客户连接
      if(sockfd == listenfd)
      {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
#ifdef listenfdLT
        int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len);
        if(connfd < 0 && errno != 11)
        {
          LOG_ERROR("%s:errno is:%d", "accept error", errno);
          continue;
        }
        if(http_conn::m_user_count >= MAX_FD)
        {
          show_error(connfd, "Internal server busy");
          LOG_ERROR("%s", "Internal server busy");
          continue;
        }
        // 将 connfd 注册到内核，同时初始化连接
        users[connfd].init(connfd, client_address);

        users_timer[connfd].address = client_address;
        users_timer[connfd].sockfd = connfd;
        auto timer = new heap_timer;
        timer->user_data = &users_timer[connfd];
        timer->cb_func = cb_func;
        timer->expire = curr + 3 * TIMESLOT;  // 超时绝对时间
        users_timer[connfd].timer = timer;
        timer_heap.add_timer(timer);
#endif

#ifdef listenfdET
        while(1)
          {
          int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len);
          if(connfd < 0)
          {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            break;
          }
          if(http_conn::m_user_count >= MAX_FD)
          {
            show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            continue;
          }
          users[connfd].init(connfd, client_address);

          users_timer[connfd].address = client_address;
          users_timer[connfd].sockfd = connfd;
          auto timer = new heap_timer;
          timer->user_data = &users_timer[connfd];
          timer->cb_func = cb_func;
          time_t curr = time(NULL);
          timer->expire = curr + 3 * TIMESLOT;  // 超时绝对时间
          users_timer[connfd].timer = timer;
          timer_heap.add_timer(timer);
          }
        continue;
#endif
      }
      // 连接关闭事件
      else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
      {
        // 服务器关闭连接，移除定时器
        auto timer = users_timer[sockfd].timer;
        timer->cb_func(&users_timer[sockfd]); // 删除连接，关闭fd

        if(timer)
          timer_heap.del_timer(timer);

      }

      // 主程序处理信号
      else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
      {
        int sig;
        char signals[1024];
        ret = recv(pipefd[0], signals, sizeof(signals), 0);
        if(ret == -1)
          continue;
        else if(ret == 0)
          continue;
        else
        {
          for(int i=0; i<ret; ++i)
          {
            switch(signals[i])
            {
              case SIGALRM:
              {
                timeout = true;
                break;
              }
              case SIGTERM:
                stop_server = true;
            }
          }
        }
      }

      // 处理客户连接上接受到的数据
      else if(events[i].events & EPOLLIN)
      {
        auto timer = users_timer[sockfd].timer;
        if(users[sockfd].read_once())
        {
          LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
          Log::get_instance()->flush();
          // 若检测到读事件，将该事件放入请求队列
          pool->append(users + sockfd);

          // 该连接活跃，更新定时器在链表中的位置
          if(timer)
          {
            time_t curr = time(NULL);
            timer->expire = curr + 3 * TIMESLOT;
            LOG_INFO("%s", "adjust timer once");
            Log::get_instance()->flush();
          }
        }
        else
        {
          timer->cb_func(&users_timer[sockfd]);
          if(timer)
            timer_heap.del_timer(timer);

        }
      }
      else if(events[i].events & EPOLLOUT)
      {
        auto timer = users_timer[sockfd].timer;
        if(users[sockfd].write())
        {
          LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
          Log::get_instance()->flush();

          // 活跃节点，更新定时器
          if(timer)
          {
            time_t curr = time(NULL);
            timer->expire = curr + 3 * TIMESLOT;
            LOG_INFO("%s", "adjust timer once");
            Log::get_instance()->flush();
          }
        }
        else
        {
          timer->cb_func(&users_timer[sockfd]);
          if(timer)
            timer_heap.del_timer(timer);

        }
      }
    }
    if(timeout)
    {
      // 超时则执行超时处理函数
      timer_handler();
      timeout = false;
    }
  }
  close(epollfd);
  close(listenfd);
  close(pipefd[1]);
  close(pipefd[0]);
  delete[] users;
  delete[] users_timer;
  delete pool;
  return 0;
}
