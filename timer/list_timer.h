//
// Created by acg on 11/14/21.
//

#ifndef XLAOTINYWEBSERVER_LIST_TIMER_H
#define XLAOTINYWEBSERVER_LIST_TIMER_H

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../log/log.h"

class util_timer;

struct client_data
{
  sockaddr_in address;
  int sockfd;
  util_timer* timer;
};

class util_timer
{
public:
  util_timer():prev(NULL),next(NULL){}
public:
  time_t expire;                      // 任务的超时时间
  void (*cb_func)(client_data *);     // 任务回调函数
  client_data* user_data;             // 回调函数处理的客户数据
  util_timer* prev;                   // 前一个定时器
  util_timer* next;                   // 下一个定时器
};

class sort_timer_list
{
public:
  sort_timer_list():head(NULL),tail(NULL){}

  ~sort_timer_list()
  {
    util_timer* temp = head;
    while(temp)
    {
      head = temp->next;
      delete temp;
      temp = head;
    }
  }

  // 将目标定时器添加到有序链表中
  void add_timer(util_timer* timer)
  {
    if(!timer)
      return;
    // 如果链表为空，直接加入，并且更新头尾
    if(!head)
    {
      head = tail = timer;
      return;
    }
    // 有序链表，从时间小-->时间大排列，如果加入的定时器比第一个还小，将它设置为头节点
    if(timer->expire < head->expire)
    {
      timer->next = head;
      head->prev = timer;
      head = timer;
      return;
    }
    // 如果定时器要插入链表中间，则比较麻烦，需要逐个比较，放在另一个函数中
    add_timer(timer, head);
  }

  // 当某个定时任务发生变化，调整其定时器在链表中的位置
  void adjust_timer(util_timer* timer)
  {
    if(!timer)
      return;

    util_timer* temp = timer->next;

    // 如果被调整的定时器位于尾部，或者相对于下一个节点来说本来就有序，不用调整
    if(!temp || (timer->expire < temp->expire))
      return;

    // 如果是头节点，则可以将其取出，然后重新加入
    if(timer == head)
    {
      // 取出 timer
      head = head->next;
      head->prev = NULL;
      timer->next = NULL;
      // 重新加入
      add_timer(timer, head);
    }
    // 如果定时器在中间，则取出它，然后在它后面的位置找到合适的地方
    else
    {
      // 从链表中删除该节点
      timer->prev->next = temp;
      temp->prev = timer->prev;
      // 插入到后面的位置
      add_timer(timer, temp);
    }
  }

  void del_timer(util_timer* timer)
  {
    if(!timer)
      return;

    // 如果链表中只有 timer 一个节点
    if(timer == head && timer == tail)
    {
      delete timer;
      head = NULL;
      tail = NULL;
      return;
    }

    // 如果是头节点
    if(timer == head)
    {
      head = head->next;
      head->prev = NULL;
      delete timer;
      return;
    }

    // 如果是尾节点
    if(timer == tail)
    {
      tail = tail->prev;
      tail->next = NULL;
      delete timer;
      return;
    }

    // 如果是中间节点
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
  }

  // 主线程收到SIGALRM信号后，前往链表中处理过期的时间器，也就是执行这个函数(tick)
  void tick()
  {
    if(!head)
      return;

    LOG_INFO("%s", "timer tick");
    Log::get_instance()->flush();

    time_t curr = time(NULL);     // 获得当前时间
    util_timer *temp = head;
    // 处理逻辑：从头节点开始遍历，直到遇到第一个没超时的定时器
    while(temp)
    {
      // 使用绝对时间，小于说明没到，即没有超时
      if(curr < temp->expire)
        break;

      // 如果超时，调用回调函数，同时删除该定时器
      temp->cb_func(temp->user_data);
      head = temp->next;    // temp之前的都是超时的
      if(head)
        head->prev = NULL;
      delete temp;
      temp = head;
    }
  }

private:
  // 辅助函数，将timer从list_head的下一个节点开始通过比较插入到正确的位置
  void add_timer(util_timer* timer, util_timer* list_head)
  {
    auto prev = list_head;
    auto curr = prev->next;
    while(curr)
    {
      // 如果找到合适的位置
      if(timer->expire < curr->expire)
      {
        prev->next = timer;
        timer->prev = prev;
        timer->next = curr;
        curr->prev = timer;
        break;
      }
      prev = curr;        // 遍历下一个节点
      curr = curr->next;
    }
    // 如果插入位置为尾部
    if(!curr)
    {
      prev->next = timer;
      timer->prev = prev;
      timer->next = NULL;
      tail = timer;
    }
  }

private:
  util_timer* head;
  util_timer* tail;
};

#endif //XLAOTINYWEBSERVER_LIST_TIMER_H
