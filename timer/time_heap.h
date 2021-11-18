#include <netinet/in.h>
#include <vector>
#include <queue>
#include <time.h>

#define BUFFER_SIZE 64

class heap_timer;

struct client_data
{
  sockaddr_in address;
  int sockfd;
  char buf[BUFFER_SIZE];
  heap_timer* timer;
};

class heap_timer
{
public:
  heap_timer(int delay)
  {
    expire = time(nullptr) + delay;
  }

public:
  time_t expire;
  void (*cb_func)(client_data *);
  client_data* user_data;
};

// 函数对象，优先队列中的第三个参数，实现最小堆
struct cmp{
  bool operator() (const heap_timer* a, const heap_timer* b)
  {
    return a->expire > b->expire;
  }
};

class time_heap
{
public:
  explicit time_heap();
  ~time_heap();

public:
  void add_timer(heap_timer* timer);    // 添加定时器
  void del_timer(heap_timer* timer);    // 删除定时器
  void tick();                          // 心搏函数
  heap_timer* Top();

private:
  // 使用优先队列实现最小时间堆
  std::priority_queue<heap_timer*, std::vector<heap_timer*>, cmp> timer_pqueue;
};

