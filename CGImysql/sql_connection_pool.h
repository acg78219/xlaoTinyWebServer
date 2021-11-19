//
// Created by acg on 11/11/21.
//

#ifndef XLAOTINYWEBSERVER_SQL_CONNECTION_POOL_H
#define XLAOTINYWEBSERVER_SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <errno.h>
#include <string.h>
#include <string>

// 数据库连接池需要线程同步
#include "../locker/locker.h"

using namespace std;

class connection_pool
{
public:
  MYSQL *getConnection();                     // 获取数据库连接
  bool releaseConnection(MYSQL* conn);        // 释放连接
  int getFreeConn();                          // 获得空闲的连接
  void destroyPool();                         // 销毁所有连接

  static connection_pool *getInstance();      // 单例模式

  void init(const string& url, const string& user, const string& passwd,
            const string& database, int port, unsigned int maxConn);

private:
  connection_pool();
  ~connection_pool();

private:
  unsigned int maxConn;     // 最大连接数
  unsigned int curConn;     // 当前已使用的连接数
  unsigned int freeConn;    // 当前空闲的连接数

private:
  locker lock;
  list<MYSQL *> connList;     // 连接池
  sem reserve;                // 信号量

private:
  string url;           // 主机地址
  string port;          // 数据库端口号
  string user;          // 登录用户名
  string passwd;        // 登录密码
  string database;      // 数据库名
};

class connectionRAII
{
public:
  connectionRAII(MYSQL **conn, connection_pool *connPool);
  ~connectionRAII();
private:
  MYSQL *connRAII;
  connection_pool *poolRAII;
};

#endif //XLAOTINYWEBSERVER_SQL_CONNECTION_POOL_H
