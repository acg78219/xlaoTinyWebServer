//
// Created by acg on 11/12/21.
//

#ifndef XLAOTINYWEBSERVER_LOG_H
#define XLAOTINYWEBSERVER_LOG_H

#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log
{
public:
  // 懒汉单例模式:
  // 第一次需要的时候才会生成单例对象
  // c++11中是线程安全的：
  // 如果当变量在初始化的时候，并发同时进入声明语句，并发线程将会阻塞等待初始化结束
  static Log *get_instance()
  {
    static Log instance;
    return &instance;
  }

  // 异步写日志
  static void* flush_log_thread(void* args)
  {
    Log::get_instance()->async_write_log();
  }

  // 日志的初始化，包括文件名、缓冲区大小、最大行数、最长日志队列
  bool init(const char* file_name, int log_buf_size = 8192, unsigned int max_lines = 5000000, int max_queue_size = 0);

  void write_log(int level, const char* format, ...);

  void flush(void);

  // 既然是单例模式，则不允许通过拷贝和赋值运算符去复制出一个新对象
  Log(const Log&)=delete;
  Log& operator=(const Log&)=delete;



private:
  // 单例模式,构造和析构都私有化
  Log();
  ~Log();

  // 异步写入日志
  void* async_write_log()
  {
    string single_log;
    // 从阻塞队列中取出一个日志 string，写入文件
    while(m_log_queue->pop(single_log))
    {
      m_mutex.lock();
      fputs(single_log.c_str(), m_fp);        // 第二个参数，指向 FILE 的指针
      m_mutex.unlock();
    }
  }


private:
  char dir_name[128];                         // 路径名
  char log_name[128];                         // log 文件名
  unsigned int m_log_max_lines;               // 日志最大行数
  int m_log_buf_size;                         // 日志缓冲区大小
  unsigned int m_count;                       // 日志行数记录
  int m_today;                                // 按天分类，记录今天
  FILE *m_fp;                                 // 打开 log 的文件指针
  char *m_buf;
  block_queue<string> *m_log_queue;           // 阻塞队列
  bool m_is_async;                            // 是否开启异步
  locker m_mutex;                             // 需要互斥锁
};

// 定义日志文件类型的四个宏
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif //XLAOTINYWEBSERVER_LOG_H
