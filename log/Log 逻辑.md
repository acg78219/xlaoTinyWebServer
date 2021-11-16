# Log 逻辑

## 1. 初始化 Log

>  Log::log(file_name, log_buf_size, max_lines, queue_size)

工作：初始化文件名、缓冲区大小、日志文件最大行数、队列大小。

### 主要关键点

- 根据队列大小判断是否开启异步写日志。
- 修饰 full_log_name：
  - 根据传入的 file_name 获取目录名、文件名。
  - 根据当前时间，按照**“目录名 + 时间 + 文件名”**修饰 full_log_name 变量。
- 打开 full_log_name 文件，记录文件描述符 m_fp。



## 2. 写日志 Log

> Log::write_log(int level, const char* format, ...)

工作：将数据写入日志

### 主要关键点

- 根据 level 判断日志文件的类型（debug/info/warn...）。
- 判断**是不是没有今天的日志 || 写入的日志文件已经满了**：
  - 如果满足其中一个，就新建一个日志文件。
  - 如果不满足，则继续写入原来的日志文件。
- 用一个 string 变量保存写入日志的内容：
  - 根据可变参数的函数获取可变参数（写入内容），然后按照一定的格式赋值给 string 变量。
- 根据是否开启异步，选择写入日志方式：
  - 异步则将 string 变量放入阻塞队列中。
  - 同步直接写入文件。