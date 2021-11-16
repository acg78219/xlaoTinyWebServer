//
// Created by acg on 11/11/21.
//

#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 多线程下线程同步包装类

// 封装信号量的类
class sem
{
public:
  sem()
  {
    // 初始化信号量，参数：sem_t* sem, int pshared, unsigned int value
    // pshared 为 0 表示信号量用于多个线程的同步
    // 成功返回 0, 失败返回 -1
    if(sem_init(&m_sem, 0, 0) != 0)
    {
      // 构造函数没有返回值，失败直接抛出异常
      throw std::exception();
    }
  }

  sem(int num)
  {
    if(sem_init(&m_sem, 0, num) != 0)
      throw std::exception();
  }

  ~sem()
  {
    sem_destroy(&m_sem);
  }

  // 等待信号量
  bool wait()
  {
    // 阻塞函数，如果 sem_value > 0，将其 -1 并返回
    // 如果 sem_value = 0，则一直阻塞等待其 > 0，然后 -1 并返回
    return sem_wait(&m_sem) == 0;
  }

  // 增加信号量
  bool post()
  {
    // 信号量 + 1,并且唤醒其他等待该信号量的线程
    return sem_post(&m_sem) == 0;
  }

private:
  sem_t m_sem;    // sem_t: 信号量的数据结构
};

// 封装互斥锁的类
class locker
{
public:
  locker()
  {
    if(pthread_mutex_init(&m_mutex, NULL) != 0)
      throw std::exception();
  }

  ~locker()
  {
    pthread_mutex_destroy(&m_mutex);
  }

  bool lock()
  {
    // 阻塞调用，拿不到锁会一直等待
    return pthread_mutex_lock(&m_mutex) == 0;
  }

  bool unlock()
  {
    return pthread_mutex_unlock(&m_mutex) == 0;
  }

  pthread_mutex_t* get()
  {
    return &m_mutex;
  }
private:
  pthread_mutex_t m_mutex;
};

// 封装条件变量的类
// 条件变量需要配合互斥锁一起使用，而互斥锁通过参数传递使用
class cond {
public:
  cond() {
    if(pthread_cond_init(&m_cond, NULL) != 0)
      throw std::exception();
  }

  ~cond() {
    pthread_cond_destroy(&m_cond);
  }

  // 等待条件变量
  bool wait(pthread_mutex_t *m_mutex) {
    int ret = 0;
    ret = pthread_cond_wait(&m_cond, m_mutex);
    return ret == 0;
  }

  // pthread_cond_timewait 的封装函数，指定时间内 wait
  bool timeWait(pthread_mutex_t * m_mutex, struct timespec t)
  {
    int ret = 0;
    ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
    return ret == 0;
  }

  // 唤醒等待该条件变量的线程
  bool signal()
  {
    return pthread_cond_signal(&m_cond) == 0;
  }

  bool broadcast()
  {
    return pthread_cond_broadcast(&m_cond) == 0;
  }
private:
  pthread_cond_t m_cond;
};

#endif //LOCKER_H
