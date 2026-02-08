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
    // 启动一个新的线程，作为连接的生产者
    std::thread produce([this]()
                        { this->produceConnectionTask(); });

    // 将线程分离，让其在后台独立运行
    produce.detach();
    // 启动心跳/扫描线程
    std::thread scannerThread([this]()
                              { this->checkConnectionTask(); });
    scannerThread.detach();
}
void ConnectionPool::produceConnectionTask()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(_queueMutex);

        // 只要当前的空闲连接数少于我们要求的初始连接数，生产者就起来干活
        while (_connectionQue.size() >= _initSize)
        {
            _cv_producer.wait(lock); // 否则生产者就睡觉
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
        _cv_consumer.notify_all();
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

    // 等待逻辑：while 防止虚假唤醒
    while (_connectionQue.empty())
    {
        if (cv_status::timeout == _cv_consumer.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
        {
            if (_connectionQue.empty())
            {
                LOG("获取空闲连接超时了!");
                return nullptr;
            }
        }
    }

    // 先取出原始指针并弹出队列
    Connection *p = _connectionQue.front();
    _connectionQue.pop();

    // 消费后立即检查并通知生产者补货
    if (_connectionQue.size() < _initSize)
    {
        _cv_producer.notify_all(); // 叫生产者起来维持 initSize 的水位
    }

    // 包装并返回，注意 Lambda 捕获和刷新时间
    return shared_ptr<Connection>(p, [this](Connection *pcon)
                                  {
                                      unique_lock<mutex> lock(this->_queueMutex);
                                      pcon->refreshAliveTime(); // 必须刷新！标记该连接开始进入“空闲状态”
                                      this->_connectionQue.push(pcon);
                                      this->_cv_consumer.notify_all(); // 归还后通知可能正在 wait 的其他消费者
                                  });
}
void ConnectionPool::scannerConnectionTask()
{
    for (;;)
    {
        // 定时检查
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        // 扫描并收集需要清理的连接
        unique_lock<mutex> lock(_queueMutex);

        // 用一个容器暂存要删的指针，避免在锁内销毁连接
        vector<Connection *> toDelete;

        while (_connectionQue.size() > _initSize)
        {
            Connection *p = _connectionQue.front();

            if (p->getAliveeTime() >= (_maxIdleTime * 1000))
            {
                _connectionQue.pop();
                _connectionCnt--; // 别忘了 atomic 的自减
                toDelete.push_back(p);
            }
            else
            {
                // 队头是最老的，如果它都没超时，后面的肯定也没超时
                break;
            }
        }
        // 释放锁
        lock.unlock();

        // 在锁外销毁连接，不影响并发性能
        for (Connection *p : toDelete)
        {
            delete p;
        }
    }
}
// 实现心跳检测巡逻任务
void ConnectionPool::checkConnectionTask() {
    for (;;) {
        // 每隔一段时间执行一次
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        // 获取当前待检测的连接数量快照
        // 我们不在这里持有长锁，而是记录下需要处理多少个连接
        unique_lock<mutex> lock(_queueMutex);
        int check_count = _connectionQue.size();
        lock.unlock(); // 立即释放锁！

        while (check_count > 0) {
            // 取连接 (占锁时间极短) 
            unique_lock<mutex> loopLock(_queueMutex);
            if (_connectionQue.empty()) {
                // 如果队列空了（被消费者拿光了），直接停止本轮检测
                break;
            }
            Connection* p = _connectionQue.front();
            _connectionQue.pop();
            check_count--; 
            
            loopLock.unlock(); // 关键：拿出连接后立即解锁

            // 执行耗时操作 (无锁并行) 
            // 此时 p 是线程私有的，不需要锁
            bool isTimeout = (p->getAliveeTime() >= (_maxIdleTime * 1000));
            // 关键优化使得网络IO不再阻塞主线程
            bool isAlive = p->ping();  

            // 处理结果 (再次短时间加锁) 
            if (!isAlive) {
                delete p;
                _connectionCnt--;
            }
            else if (isTimeout && _connectionCnt > _initSize) {
                // 虽然活着，但空闲超时且连接数富余，销毁缩容
                delete p;
                _connectionCnt--;
            }
            else {
                // 连接健康，归还到队列（重新入队到尾部）
                unique_lock<mutex> pushLock(_queueMutex);
                _connectionQue.push(p);
            }
        }
        
        // 扫描完一轮，如果连接数不足（可能刚才销毁了一些），通知生产者补货
        unique_lock<mutex> endLock(_queueMutex);
        if (_connectionCnt < _initSize) {
            _cv_producer.notify_all();
        }
    }
}