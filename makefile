server: main.cpp ./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/block_queue.h ./log/log.cpp ./log/log.h ./threadPool/threadPool.h ./timer/list_timer.h
	g++ -g -o server main.cpp ./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/block_queue.h ./log/log.cpp ./log/log.h ./threadPool/threadPool.h ./timer/list_timer.h -lpthread -lmysqlclient

clean:
	rm -r server