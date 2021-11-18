//
// Created by acg on 11/18/21.
//

#include "time_heap.h"
#include "../log/log.h"

time_heap::time_heap() = default;

time_heap::~time_heap()
{
  while(!timer_pqueue.empty())
  {
    delete timer_pqueue.top();
    timer_pqueue.pop();
  }
}

void time_heap::add_timer(heap_timer *timer)
{
  if(!timer)  return;
  // emplace原地构造一个timer，然后根据仿函数插入到合适的位置
  timer_pqueue.emplace(timer);
}

void time_heap::del_timer(heap_timer *timer)
{
  if(!timer)  return;
  timer->cb_func = NULL;  // 只是置空回调函数，空间换时间
}

void time_heap::tick()
{
  auto curr = time(NULL);
  while(!timer_pqueue.empty())
  {
    LOG_INFO("%s", "timer tick");
    Log::get_instance()->flush();

    // 让堆顶元素和当前时间对比
    auto timer = timer_pqueue.top();
    // 没超时
    if(timer->expire > curr)
      break;
    // 超时则执行timer的回调函数,然后删除堆顶timer
    if(timer->cb_func)
      timer->cb_func(timer->user_data);
    timer_pqueue.pop();
  }
}

heap_timer* time_heap::Top()
{
  if(timer_pqueue.empty())
    return nullptr;
  return timer_pqueue.top();
}

