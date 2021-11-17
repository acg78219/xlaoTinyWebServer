//
// Created by acg on 11/16/21.
//

#ifndef XLAOTINYWEBSERVER_TIME_HEAP_H
#define XLAOTINYWEBSERVER_TIME_HEAP_H

#include <netinet/in.h>
#include <time.h>
#include <vector>

#include "../log/log.h"

#define BUFFER_SIZE 64
// 使用最小堆来使用时间器容量
class heap_timer;

struct client_data
{
  sockaddr_in address;
  int sockfd;
  //char buf[BUFFER_SIZE];
  heap_timer* timer;
};

// 定时器类
class heap_timer
{
public:
//    heap_timer(int delay)
//    {
//      expire = time(NULL) + delay;
//    }
public:
  time_t expire;                  // 定时器生效的绝对时间
  void (*cb_func)(client_data *);   // 定时器的回调函数
  client_data* user_data;         // 用户数据
};

class time_heap
{
public:
  ~time_heap()
  {
    timer_vec.clear();
  }
public:
  // 添加定时器（上浮）
  void add_timer(heap_timer* timer)
  {
    if(!timer)
      return;

    timer_vec.push_back(timer);
    int hole = timer_vec.size();                  // 从堆底开始上浮
    int parent = 0;
    for(; hole>0; hole=parent)
    {
      parent = (hole-1)/2;                            // 父节点
      if(timer_vec[parent]->expire <= timer->expire)   // 如果父节点比timer节点先超时，那么已经找到合适的位置，不用上浮
        break;
      timer_vec[hole] = timer_vec[parent];            // 上浮操作，父子交换
    }
    timer_vec[hole] = timer;                          // 将定时器加入到堆节点
  }

  // 移除顶部的定时器,调用私有的核心下沉操作
  void pop_timer()
  {
    if(timer_vec.empty())
      return;
    if(timer_vec.front())
    {
      std::swap(timer_vec[0], timer_vec[timer_vec.size()-1]);         // 将堆顶与堆底交换,然后删除堆底达到删除的操作
      timer_vec.pop_back();
      sink(0);
    }
  }

  // 删除中间的定时器
  void del_timer(heap_timer *timer)
  {
    if(!timer)
      return;

    timer->cb_func = NULL;            // 只是置空回调函数，空间换时间
  }

  // 获得堆顶定时器（即将超时的定时器）
  heap_timer* top() const
  {
    if(timer_vec.empty())
      return NULL;
    return timer_vec.front();
  }

  // 新搏函数，每次调用都会有至少有一个定时器被删除，效率更高
  void tick()
  {
    if(timer_vec.empty())
      return;
    LOG_INFO("%s", "timer tick");
    Log::get_instance()->flush();

    heap_timer* temp = timer_vec[0];      // temp 是堆顶元素
    time_t curr = time(NULL);       // 获得当前时间，与定时器比较看是否超时
    while(!timer_vec.empty()){
      if(!temp)
        break;

      if(temp->expire > curr)             // 如果堆顶没有超时
        break;

      if(timer_vec[0]->cb_func)
      {
        timer_vec[0]->cb_func(timer_vec[0]->user_data);     // 执行定时器的回调函数
      }
      // 删除超时定时器，更新新的堆顶定时器
      pop_timer();
      temp = timer_vec[0];
    }
  }

private:
  // 下沉的核心操作
  void sink(int hole)
  {
    heap_timer* temp = timer_vec[hole];
    int child = 0;

    // 只需要对非叶子节点做下沉操作
    for(;((hole*2+1) <= (timer_vec.size()-1)); hole=child)
    {
      child = hole * 2 + 1;     // 左子节点
      if((child < (timer_vec.size()-1)) && (timer_vec[child+1]->expire < timer_vec[child]->expire))
        ++child;                      // 必须找到最小的子节点（左还是右）

      if(timer_vec[child]->expire < temp->expire)
        timer_vec[hole] = timer_vec[child];
      else
        break;
    }
    timer_vec[hole] = temp;
  }

private:
  std::vector<heap_timer*>timer_vec;       // 存放定时器的vector容器
};
#endif //XLAOTINYWEBSERVER_TIME_HEAP_H
