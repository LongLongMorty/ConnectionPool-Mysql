#include <iostream>
#include <thread>
#include <vector>
#include "Connection.h"
#include "CommonConnectionPool.h"

using namespace std;

// 压力测试：不使用连接池
void testWithoutPool(int count) {
    for (int i = 0; i < count; ++i) {
        Connection conn;
        char sql[1024] = {0};
        sprintf(sql, "insert into user(name, age, sex) values('%s', %d, '%s')", "zhang san", 20, "male");
        
        // 每次都要进行 TCP 握手、MySQL 身份验证
        if (conn.connect("127.0.0.1", 3306, "pool_user", "PoolPassword123!", "my_project_db")) {
            conn.update(sql);
        }
    }
}

// 压力测试：使用连接池
void testWithPool(int count) {
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    for (int i = 0; i < count; ++i) {
        // 直接从池子里“借”连接，无需握手
        shared_ptr<Connection> sp = cp->getConnection();
        char sql[1024] = {0};
        sprintf(sql, "insert into user(name, age, sex) values('%s', %d, '%s')", "zhang san", 20, "male");
        
        if (sp) {
            sp->update(sql);
        }
    }
}

int main() {
    const int TOTAL_OP = 1000; // 测试 1000 次插入操作

    // --- 测试 1：单线程性能对比 ---
    cout << "--- Single Thread Test (" << TOTAL_OP << " ops) ---" << endl;

    clock_t start = clock();
    testWithoutPool(TOTAL_OP);
    clock_t end = clock();
    cout << "Without Pool: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;

    start = clock();
    testWithPool(TOTAL_OP);
    end = clock();
    cout << "With Pool   : " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;

    // --- 测试 2：多线程并发对比 (4 线程各执行 250 次) ---
    cout << "\n--- Multi-Thread Test (" << TOTAL_OP << " total ops) ---" << endl;
    
    start = clock();
    thread t1(testWithPool, 250);
    thread t2(testWithPool, 250);
    thread t3(testWithPool, 250);
    thread t4(testWithPool, 250);
    t1.join(); t2.join(); t3.join(); t4.join();
    end = clock();
    cout << "With Pool (4 threads): " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;

    return 0;
}