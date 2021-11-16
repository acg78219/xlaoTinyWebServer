//
// Created by acg on 11/12/21.
//
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

using namespace std;

Log::Log()
{
  m_count = 0;
  m_is_async = false;
}

Log::~Log()
{
  if(m_fp != NULL)
    fclose(m_fp);
}

// 异步写需要设置队列
bool Log::init(const char *file_name, int log_buf_size, unsigned int max_lines, int max_queue_size)
{
  // 使用队列则说明使用异步
  if(max_queue_size >= 1)
  {
    m_is_async = true;
    m_log_queue = new block_queue<string>(max_queue_size);    // 创建一个阻塞队列
    pthread_t tid;      // 线程 ID
    // 创建线程异步写日志，第三个参数是回调函数
    pthread_create(&tid, NULL, flush_log_thread, NULL);
  }

  m_log_buf_size = log_buf_size;
  m_buf = new char[m_log_buf_size];
  memset(m_buf, '\0', m_log_buf_size);
  m_log_max_lines = max_lines;

  time_t t = time(NULL);              // 获取 time_t 类型的当前时间
  struct tm *sys_tm = localtime(&t);        // 转换成当地时间
  struct tm my_tm = *sys_tm;

  // strrchr: 在 file_name 文件中搜索 '/' 最后一次出现的位置
  // 所以打印 p 就会显示文件名
  const char* p = strrchr(file_name, '/');
  char log_full_name[256] = {0};

  if(p == NULL) // 如果只是一个文件名: "myFile"
  {
    // snprintf(char* str, size_t size, const char* format, ...)
    // 将可变参数 ... 按照 format 格式化成字符串，然后复制到 str 中，大小为 size
    // log_full_name:"2021_01_01_myFile"， %02d 表示如果数字不足2位，则左边补0
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_yday + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
  }
  else  // 如果包含路径:"D://myDir//myFile"
  {
    // 因为文件命名要包含当前时间，所以需要分别获取路径名和文件名
    strcpy(log_name, p + 1);                           // p = /myFile，则 p + 1 = myFile
    strncpy(dir_name, file_name, p - file_name + 1);    // 获取 p 前面的路径名
    // log_full_name: "D://myDir//2021_01_01_myFile"
    snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
  }

  m_today = my_tm.tm_mday;

  m_fp = fopen(log_full_name, "a"); // a:append,追加到文件，如果文件不存在则创建
  if(m_fp == NULL)
  {
    return false;
  }
  return true;
}

// 写日志，主要逻辑函数
void Log::write_log(int level, const char *format, ...)
{
  struct timeval now = {0, 0};  // 秒、微秒
  gettimeofday(&now, NULL);   // 目前的时间放入 now 中
  time_t time = now.tv_sec;
  struct tm *sys_time = localtime(&time);
  struct tm my_time = *sys_time;
  char s[16] = {0};

  switch (level) {  // level 即日志的类型，在头文件中已经宏定义了
    case 0:
      strcpy(s, "[debug]:");
      break;
    case 1:
      strcpy(s, "[info]:");
      break;
    case 2:
      strcpy(s, "[warn]:");
      break;
    case 3:
      strcpy(s, "[error]:");
      break;
    default:
      strcpy(s, "[info]:");
      break;
  }

  // 写入 log，同时更新 m_count
  // 文件的打开是代码关键区
  m_mutex.lock();
  ++m_count;

  // 如果没有今天的日志 || 写入当前行数超出最大行数:
  // 需要新建一个文件夹同时打开它
  if(m_today != my_time.tm_mday || m_count % m_log_max_lines == 0)
  {
    char new_log[256] = {0};
    fflush(m_fp);   // 将缓冲区写入文件然后关闭文件
    fclose(m_fp);
    char tail[16] = {0};      // tail 记录完整日期

    snprintf(tail, 16, "%d_%02d_%02d_", my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday);

    if(m_today != my_time.tm_mday)  // 如果 m_today 过期（今天是新的一天而且还没有日志）
    {
      snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
      m_today = my_time.tm_mday;    // 更新 m_today
      m_count = 0;                  // 新的日志
    }
    else
    {
      // 日志文件太大，新建一个新的
      snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_log_max_lines);
    }
    m_fp = fopen(new_log, "a");
  }

  m_mutex.unlock();

  va_list vaList;             // 可变参数变量，指向参数的指针
  va_start(vaList, format);   // vaList 指向可变列表的地址，即 format

  string log_str;   // 最终写入文件的内容
  m_mutex.lock();

  // 写入的具体时间内容格式
  int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                   my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday,
                   my_time.tm_hour, my_time.tm_min, my_time.tm_sec, now.tv_usec, s);

  // 将可变参数 vaList 按照 format 的格式保存到 m_buf + n 的位置，大小为 m_log_buf_size - 1
  // 返回写入字符的个数，失败返回负值
  int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, vaList);

  m_buf[n + m] = '\n';
  m_buf[n + m + 1] = '\0';
  log_str = m_buf;

  m_mutex.unlock();

  // 异步写入，不用加锁，调用方法中有
  if(m_is_async && !m_log_queue->full())
    m_log_queue->push(log_str);

  else  // 同步写入，直接调用 fputs 写入 m_fp 文件
  {
    m_mutex.lock();
    fputs(log_str.c_str(), m_fp);
    m_mutex.unlock();
  }

  va_end(vaList);     // 关闭可变参数的获取
}

void Log::flush(void)
{
  m_mutex.lock();
  fflush(m_fp);
  m_mutex.unlock();
}
