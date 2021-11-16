//
// Created by acg on 11/11/21.
//

#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>

#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
  this->curConn = 0;
  this->freeConn = 0;
}

connection_pool *connection_pool::getInstance()
{
  static connection_pool connPool;
  return &connPool;
}

// 连接池的初始化
void connection_pool::init(const string& url, const string& user, const string& passwd,
                           const string& database, int port, unsigned int maxConn)
{
  printf("2");
  this->url = url;
  this->user = user;
  this->passwd = passwd;
  this->database = database;
  this->port = port;

  // 连接数据库，保证只有一个线程执行
  lock.lock();
  for(int i=0; i<maxConn; ++i)
  {
    MYSQL *conn = NULL;             // 创建一个 mysql 连接变量
    conn = mysql_init(conn);        // 分配、初始化 mysql 对象

    if(conn == NULL)
    {
      printf("Error:%s\n", mysql_error(conn));
      exit(1);
    }

    // 尝试与主机上的 mysql 建立连接
    conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), database.c_str(), port, NULL, 0);

    if(conn == NULL)
    {
      printf("Error:%s\n", mysql_error(conn));
      exit(1);
    }
//
//    if(!mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), database.c_str(), port, NULL, 0))
//    {
//      std::cout << mysql_error(conn) << "\t" << mysql_error(conn) << std::endl;
//    }

    // 连接已经建立，将初始化好的连接放进连接池中
    connList.push_back(conn);
    ++freeConn;       // 空闲连接增加
  }

  reserve = sem(freeConn);    // 创建一个信号量
  this->maxConn = freeConn;
  lock.unlock();
}

// 当有请求时，从连接池中返回一个可用链接，同时更新使用和空闲链接
MYSQL* connection_pool::getConnection()
{
  if(connList.size() == 0)
    return NULL;

  MYSQL *conn = NULL;

  reserve.wait();

  lock.lock();
  conn = connList.front();    // 获得连接池的第一个连接

  connList.pop_front();   // 更新连接池和连接变量
  --freeConn;
  ++curConn;

  lock.unlock();
  return conn;
}

// 释放当前使用的连接,重新放回连接池中
bool connection_pool::releaseConnection(MYSQL *conn)
{
  if(conn == NULL)
    return false;

  lock.lock();

  connList.push_back(conn);
  ++freeConn;
  --curConn;

  lock.unlock();
  reserve.post();
  return true;
}

// 销毁连接池
void connection_pool::destroyPool()
{
  lock.lock();

  if(connList.size() > 0)
  {
    list<MYSQL *>::iterator it;
    for(it = connList.begin(); it != connList.end(); ++it)
    {
      auto conn = *it;
      mysql_close(conn);    // 关闭所有与数据库的连接
    }
    curConn = 0;
    freeConn = 0;
    connList.clear();
  }

  lock.unlock();
}

// 当前空闲的连接
int connection_pool::getFreeConn()
{
  return this->freeConn;
}

connection_pool::~connection_pool()
{
  destroyPool();
}

connectionRAII::connectionRAII(MYSQL **conn, connection_pool *connPool)
{
  *conn = connPool->getConnection();
  connRAII = *conn;        // 数据库连接实例
  poolRAII = connPool;    // 连接池
}

connectionRAII::~connectionRAII()
{
  // 释放实例
  poolRAII->releaseConnection(connRAII);
}