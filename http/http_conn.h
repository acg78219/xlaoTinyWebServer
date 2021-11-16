//
// Created by acg on 11/12/21.
//

#ifndef XLAOTINYWEBSERVER_HTTP_CONN_H
#define XLAOTINYWEBSERVER_HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"

// 使用有限状态机实现的 http 连接处理类
class http_conn
{
public:
  static const int FILENAME_LEN = 200;            // 文件名的最大长度
  static const int READ_BUFFER_SIZE = 2048;       // 读缓冲区的大小
  static const int WRITE_BUFFER_SIZE = 1024;      // 写缓冲区的大小

  // http 请求的方法，目前只实现 GET 和 POST
  enum METHOD
  {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
  };

  // 解析客户请求时，主状态机的状态
  enum CHECK_STATE
  {
    CHECK_STATE_REQUESTLINE = 0,    // 当前正在处理请求行
    CHECK_STATE_HEADER,             // 当前正在处理头部行
    CHECK_STATE_CONTENT             // 当前正在处理内容行
  };

  // 处理 http 请求可能的结果
  enum HTTP_CODE
  {
    NO_REQUEST,               // 请求不完整，继续读取
    GET_REQUEST,              // 获得完整请求
    BAD_REQUEST,              // 请求有语法错误
    NO_RESOURCE,              // 没有请求的资源
    FORBIDDEN_REQUEST,        // 客户没有权限请求该资源
    FILE_REQUEST,             // 文件请求
    INTERNAL_ERROR,           // 服务器内部错误
    CLOSED_CONNECTION         // 客户断开连接
  };

  // 行的读取状态，从状态机
  enum LINE_STATUS
  {
    LINE_OK = 0,              // 读取到完成的行
    LINE_BAD,                 // 行出错
    LINE_OPEN                 // 行数据不完整
  };

public:
  http_conn(){}
  ~http_conn(){}

public:
  void init(int sockfd, const sockaddr_in& addr);   // 初始化新接受的连接
  void close_conn(bool real_close = true);          // 关闭连接
  void process();                                   // 处理客户请求
  bool read_once();                                 // 非阻塞读
  bool write();                                     // 非阻塞写
  sockaddr_in *get_address() { return &m_address;}   // 返回地址
  void init_mysql_result(connection_pool *connPool);

private:
  void init();                                      // 初始化连接
  HTTP_CODE process_read();                         // 解析 http 请求
  bool process_write(HTTP_CODE ret);                // 填充 http 应答

  // 下面一组函数用来被 process_read 调用解析 http 请求
  HTTP_CODE parse_request_line(char* text);
  HTTP_CODE parse_headers(char* text);
  HTTP_CODE parse_content(char* text);
  HTTP_CODE do_request();
  char* get_line() {return m_read_buf + m_start_line;};
  LINE_STATUS parse_line();

  // 下面一组函数被 process_write 调用填充 http 应答
  void unmap();
  bool add_response(const char* format, ...);
  bool add_content(const char* content);
  bool add_status_line(int status, const char* title);
  bool add_headers(int content_length);
  bool add_content_type();
  bool add_content_length(int content_length);
  bool add_linger();
  bool add_blank_line();

public:
  // 所有 socket 上的事件都被注册到同一个 epoll 内核事件表中
  static int m_epollfd;
  static int m_user_count;      // 统计用户数量
  MYSQL *mysql;

private:
  int m_sockfd;                           // 本 http 连接的 socket
  sockaddr_in m_address;                  // 对方的 socket 地址

  char m_read_buf[READ_BUFFER_SIZE];      // 读缓冲区
  int m_read_idx;                         // 读缓冲区中已经读入的数据的最后一个字节的下一个位置
  int m_checked_idx;                      // 当前正在分析的字符在区中的位置
  int m_start_line;                       // 当前正在解析的行的起始位置
  char m_write_buf[WRITE_BUFFER_SIZE];    // 写缓冲区
  int m_write_idx;                        // 写区待发送的字节数

  CHECK_STATE m_check_state;              // 主状态机的状态
  METHOD m_method;                        // 请求的类型

  // 客户请求的目标文件的完整路径, doc_root + m_url
  char m_real_file[FILENAME_LEN];
  char* m_url;                             // 客户请求的目标文件的文件名
  char* m_version;                         // http 协议版本号
  char* m_host;                            // 主机号
  int m_content_len;                       // 请求消息体的长度
  bool m_linger;                            // 请求是否保持连接

  char* m_file_address;                     // 目标文件的地址
  // 文件的状态，包括是否存在、是否为目录、是否可读、文件大小等
  struct stat m_file_stat;
  struct iovec m_iv[2];                       // io 内存块,有一个数据指针和数据长度
  int m_iv_count;                             // 被写内存块的数量

  int cgi;                                    // post 的时候才启用
  char* m_string;                             // 存储消息体数据
  int bytes_to_send;
  int bytes_have_send;
};

#endif //XLAOTINYWEBSERVER_HTTP_CONN_H
