#pragma once
#include <string>
#include <queue>
#include "Connection.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
using namespace std;
class ConnectionPool
{
public:
	// 获取单例实例 (线程安全)
	static ConnectionPool *getConnectionPool();
	// 关键：禁用拷贝构造和赋值操作
	ConnectionPool(const ConnectionPool &) = delete;
	ConnectionPool &operator=(const ConnectionPool &) = delete;
	// 给外部提供接口，从连接池中获取一个可用的空闲连接
	shared_ptr<Connection> getConnection();

private:
	string _ip;							// mysql的ip地址
	unsigned short _port;				// mysql的端口号 3306
	string _username;					// mysql登录用户名
	string _password;					// mysql登录密码
	string _dbname;						// 连接的数据库名称
	int _initSize;						// 连接池的初始连接量
	int _maxSize;						// 连接池的最大连接量
	int _maxIdleTime;					// 连接池最大空闲时间
	int _connectionTimeout;				// 连接池获取连接的超时时间
	queue<Connection *> _connectionQue; // 存储mysql连接的队列
	mutex _queueMutex;					// 维护连接队列的线程安全互斥锁
	atomic_int _connectionCnt;			// 记录连接所创建的connection连接的总数量
	condition_variable _cv_consumer;	// 专用于消费者的等待与通知
	condition_variable _cv_producer;	// 专用于生产者的等待与通知
	// 构造
	ConnectionPool();
	// 运行在独立的线程中，专门负责生产新连接
	void produceConnectionTask();
	// 扫描超过maxIdleTime时间的空闲连接，进行对于的连接回收
	void scannerConnectionTask();
	// 从配置文件中加载配置项
	bool loadConfigFile();
	// 在头文件中增加心跳处理函数的声明
	void checkConnectionTask();
};