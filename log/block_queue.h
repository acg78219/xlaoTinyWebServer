//
// Created by acg on 11/12/21.
//

#ifndef XLAOTINYWEBSERVER_BLOCK_QUEUE_H
#define XLAOTINYWEBSERVER_BLOCK_QUEUE_H

// 循环数组实现的阻塞队列

#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "../locker/locker.h"

using namespace  std;

template <class T>
class block_queue
{
public:
  block_queue(int max_size = 1000)
  {
    if(max_size <= 0)
      exit(-1);

    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
  }

  ~block_queue()
  {
    m_mutex.lock();
    if(m_array != NULL)
      delete [] m_array;

    m_mutex.unlock();
  }

  // 禁止生成默认的拷贝和赋值函数
  block_queue(const block_queue&)=delete;
  block_queue& operator=(const block_queue&)=delete;

  void clear()
  {
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
  }

  bool full()
  {
    m_mutex.lock();
    if(m_size >= m_max_size)
    {
      m_mutex.unlock();
      return true;
    }
    m_mutex.unlock();
    return false;
  }

  bool empty()
  {
    m_mutex.lock();
    if(m_size == 0)
    {
      m_mutex.unlock();
      return true;
    }
    m_mutex.unlock();
    return false;
  }

  // 队首，队尾元素返回是否有元素，而元素保存在传进来的 value 的引用中
  bool front(T &value)
  {
    m_mutex.lock();
    if(m_size == 0)   //  不能使用 empty 函数，因为有一次加锁解锁的操作
    {
      m_mutex.unlock();
      return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
  }

  bool back(T &value)
  {
    m_mutex.lock();
    if(m_size == 0)
    {
      m_mutex.unlock();
      return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
  }

  int size()
  {
    int ret = 0;

    m_mutex.lock();
    ret = m_size;
    m_mutex.unlock();

    return ret;
  }

  int max_size()
  {
    int ret = 0;

    m_mutex.lock();
    ret = m_max_size;
    m_mutex.unlock();

    return ret;
  }

  // push 的逻辑（生产者）：
  // 生产者往队列中 push 新元素，同时广播唤醒所有使用队列的线程
  bool push(const T &item)
  {
    m_mutex.lock();

    if(m_size >= m_max_size)
    { // 当前元素大于队列容量，唤醒进程去处理任务，而插入新元素失败
      m_cond.broadcast();
      m_mutex.unlock();
      return false;
    }

    // 只有当前队列元素没有超过 max_size，就可以一直插入
    // 循环队列，在一个固定数组中去利用 % 实现循环
    // 队列的 push 是尾插法
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    ++m_size;
    m_cond.broadcast();     // 唤醒进程

    m_mutex.unlock();
    return true;
  }

  // pop 的逻辑（消费者）：
  // 对于消费者，pop 有两种情况：
  // 1. 队列没有元素 —— 线程会加入线程对待队列，然后等待条件变量
  // 2. 队列有元素 —— 线程抢锁然后取出首元素
  bool pop(T& item)
  {
    m_mutex.lock();
    // while 而不是 if 的原因：
    // A,B 共同抢占一个资源，此时有一个资源来了，生产者发送信号
    // A,B 同时执行完 wait（此时的 m_size 不等于 0，即判断条件不满足）
    // 如果 B 抢到了锁——消耗资源——释放锁
    // 而对于 A 来说，此时的他 wait 成功了，就是执行接下来的语句，if 语句块之后的逻辑操作，
    // 但是这个资源已经被 B 消耗了，所以 A 再操作这个资源是不存在的
    // 所以应该使用 while，即使 A wait 成功了也要重新判断队列是否为空
    while(m_size <= 0)
    {
      // 在将线程加入线程等待队列时(cond_wait())，正在加入的线程是持有锁的，原因：
      // 如果加入过程中释放锁，则其他正在队列的线程可能此时接受到信号，开始抢锁使用资源，但是这是 A 还没进入队列一起抢资源，他必不可能抢到，
      // 所以对于还没有加入队列的 A 线程来说是不公平
      // 而 A 会在加入队列后释放锁（在这个过程中资源信号不会发生），然后大家一起公平等待资源信号
      if(!m_cond.wait(m_mutex.get()))
      {
        // 封装的 wait 的返回值是 bool
        // false 表示 wait 失败，因为此时已经抢到锁，所以要先释放锁，然后返回 false
        m_mutex.unlock();
        return false;
      }
    }

    // 以下是 wait 成功的线程处理的代码
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    --m_size;
    m_mutex.unlock();
    return true;
  }

private:
  locker m_mutex;
  cond m_cond;

  T* m_array;
  int m_size;
  int m_max_size;
  int m_front;
  int m_back;
};

#endif //XLAOTINYWEBSERVER_BLOCK_QUEUE_H
