#include "CommonConnectionPool.h"
#include "public.h"

ConnectionPool::ConnectionPool()
{
    // 加载配置项了
    if (!loadConfigFile())
    {
        return;
    }
    // 创建初始数量的连接
    for (int i = 0; i < _initSize; ++i)
    {
        Connection *p = new Connection();
        p->connect(_ip, _port, _username, _password, _dbname);
        _connectionQue.push(p);
        _connectionCnt++;
    }
    // 启动一个新的线程，作为连接的生产者 linux thread => pthread_create
    std::thread produce([this]()
                        { this->produceConnectionTask(); });

    // 将线程分离，让其在后台独立运行
    produce.detach();
}
void ConnectionPool::produceConnectionTask()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(_queueMutex);

        // 只要当前的空闲连接数少于我们要求的初始连接数，生产者就起来干活
        while (_connectionQue.size() >= _initSize)
        {
            cv.wait(lock); // 否则生产者就睡觉
        }

        // 走到这里说明队列空了，且被唤醒了
        if (_connectionCnt < _maxSize)
        {
            Connection *p = new Connection();
            if (p->connect(_ip, _port, _username, _password, _dbname))
            {
                p->refreshAliveTime();
                _connectionQue.push(p);
                _connectionCnt++;
            }
        }

        // 通知消费者：现在有连接可以用了
        cv.notify_all();
    }
}
// 线程安全的懒汉单例函数接口
ConnectionPool *ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool; // lock和unlock
    return &pool;
}
bool ConnectionPool::loadConfigFile()
{
    FILE *pf = fopen("mysql.ini", "r");
    if (pf == nullptr)
    // 尝试在 build 目录下寻找r)
    {
        LOG("mysql.ini file is not exist!");
        return false;
    }

    char line[1024];
    while (fgets(line, 1024, pf) != nullptr)
    {
        string str = line;
        // 1. 处理注释和空行
        if (str.empty() || str[0] == '#' || str[0] == '\n' || str[0] == '\r')
            continue;

        // 2. 查找等号
        size_t idx = str.find('=');
        if (idx == string::npos)
            continue;

        // 3. 提取 Key
        string key = str.substr(0, idx);

        // 4. 提取 Value 并清理末尾的 \r, \n 或空格
        size_t endidx = str.find_first_of("\r\n", idx);
        string value = str.substr(idx + 1, endidx - idx - 1);

        // 核心：处理可能存在的 key/value 前后空格 (Trim)
        // 简单的去掉前后空格能极大提高配置文件的容错率

        if (key == "ip")
            _ip = value;
        else if (key == "port")
            _port = atoi(value.c_str());
        else if (key == "username")
            _username = value;
        else if (key == "password")
            _password = value;
        else if (key == "dbname")
            _dbname = value;
        else if (key == "initSize")
            _initSize = atoi(value.c_str());
        else if (key == "maxSize")
            _maxSize = atoi(value.c_str());
        else if (key == "maxIdleTime")
            _maxIdleTime = atoi(value.c_str());
        else if (key == "connectionTimeOut")
            _connectionTimeout = atoi(value.c_str());
    }
    fclose(pf);
    return true;
}
shared_ptr<Connection> ConnectionPool::getConnection()
{
    unique_lock<mutex> lock(_queueMutex);

    // 1. 等待逻辑：while 防止虚假唤醒
    while (_connectionQue.empty())
    {
        if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
        {
            if (_connectionQue.empty())
            {
                LOG("获取空闲连接超时了!");
                return nullptr;
            }
        }
    }

    // 2. 先取出原始指针并弹出队列
    Connection *p = _connectionQue.front();
    _connectionQue.pop();

    // 3. 消费后立即检查并通知生产者补货
    if (_connectionQue.size() < _initSize)
    {
        cv.notify_all(); // 叫生产者起来维持 initSize 的水位
    }

    // 4. 包装并返回，注意 Lambda 捕获和刷新时间
    return shared_ptr<Connection>(p, [this](Connection *pcon)
                                  {
                                      unique_lock<mutex> lock(this->_queueMutex);
                                      pcon->refreshAliveTime(); // 必须刷新！标记该连接开始进入“空闲状态”
                                      this->_connectionQue.push(pcon);
                                      this->cv.notify_all(); // 归还后通知可能正在 wait 的其他消费者
                                  });
}
void ConnectionPool::scannerConnectionTask()
{
    for (;;)
    {
        // 1. 定时检查
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        // 2. 扫描并收集需要清理的连接
        unique_lock<mutex> lock(_queueMutex);
        while (_connectionCnt > _initSize)
        {
            Connection *p = _connectionQue.front();

            // 假设 getAliveeTime() 返回的是该连接已经空闲的毫秒数
            if (p->getAliveeTime() >= (_maxIdleTime * 1000))
            {
                _connectionQue.pop();
                _connectionCnt--;
                delete p; // 释放连接（如果追求极致性能，建议移到锁外）
            }
            else
            {
                // 队头是最老的，如果它都没超时，后面的肯定也没超时
                break;
            }
        }
    }
}