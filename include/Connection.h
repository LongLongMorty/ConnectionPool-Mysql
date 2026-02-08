#pragma once
#include <mysql/mysql.h>
#include <string>
#include <chrono>
using namespace std;

/*
实现MySQL数据库的操作
*/
class Connection
{
public:
	// 初始化数据库连接
	Connection();
	// 释放数据库连接资源
	~Connection();
	// 连接数据库
	bool connect(string ip,
				 unsigned short port,
				 string user,
				 string password,
				 string dbname);
	// 更新操作 insert、delete、update
	bool update(string sql);
	// 查询操作 select
	MYSQL_RES *query(string sql);

	void refreshAliveTime() { _alivetime = std::chrono::high_resolution_clock::now(); }
	// 返回空闲了多少毫秒 (ms)
	long long getAliveeTime() const
	{
		auto end = std::chrono::high_resolution_clock::now();
		// 计算差值并转换成毫秒
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - _alivetime).count();
	}
	// 发送一个轻量级的心跳包，返回 true 表示连接可用
	bool ping();

private:
	MYSQL *_conn;															// 表示和MySQL Server的一条连接
	std::chrono::time_point<std::chrono::high_resolution_clock> _alivetime; // 记录空闲开始时间
};