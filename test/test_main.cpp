#include <iostream>
#include <thread>
#include <vector>
#include <chrono> // 使用高精度计时
#include <iomanip> // 用于格式化输出
#include "Connection.h"
#include "CommonConnectionPool.h"

using namespace std;
using namespace std::chrono;

// 辅助函数：获取当前时间点
inline steady_clock::time_point now() { return steady_clock::now(); }

// 辅助函数：计算耗时（秒）
inline double get_elapsed_time(steady_clock::time_point start, steady_clock::time_point end) {
    return duration_cast<milliseconds>(end - start).count() / 1000.0;
}

// 压力测试：不使用连接池
void testWithoutPool(int count) {
    for (int i = 0; i < count; ++i) {
        Connection conn;
        char sql[1024] = {0};
        sprintf(sql, "insert into user(name, age, sex) values('%s', %d, '%s')", "zhang san", 20, "male");
        
        if (conn.connect("127.0.0.1", 3306, "pool_user", "PoolPassword123!", "my_project_db")) {
            conn.update(sql);
        } else {
             // std::cerr << "Connect failed" << endl;
        }
    }
}

// 压力测试：使用连接池
void testWithPool(int count) {
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    for (int i = 0; i < count; ++i) {
        shared_ptr<Connection> sp = cp->getConnection();
        if (sp) {
            char sql[1024] = {0};
            sprintf(sql, "insert into user(name, age, sex) values('%s', %d, '%s')", "zhang san", 20, "male");
            sp->update(sql);
        }
    }
}

void printResult(string label, int total_op, double time_sec) {
    cout << left << setw(25) << label 
         << " | Time: " << setw(8) << fixed << setprecision(3) << time_sec << "s"
         << " | QPS: " << setw(10) << (int)(total_op / time_sec) << endl;
}

int main() {
    // 增加样本量，数据会更稳定，建议测试 5000 次以上
    const int TOTAL_OP = 5000; 

    cout << "================ Database Connection Pool Benchmark ================" << endl;
    cout << "Total Operations: " << TOTAL_OP << endl;
    cout << "--------------------------------------------------------------------" << endl;

    // --- 测试 1：不使用连接池 (单线程) ---
    auto start = now();
    testWithoutPool(TOTAL_OP);
    auto end = now();
    printResult("Standard (No Pool)", TOTAL_OP, get_elapsed_time(start, end));

    // --- 测试 2：使用连接池 (单线程) ---
    start = now();
    testWithPool(TOTAL_OP);
    end = now();
    printResult("Pool (Single-Thread)", TOTAL_OP, get_elapsed_time(start, end));

    // --- 测试 3：使用连接池 (多线程并发) ---
    // 模拟真实高并发，开启 4 个线程
    int thread_num = 4;
    int op_per_thread = TOTAL_OP / thread_num;
    
    start = now();
    vector<thread> threads;
    for(int i = 0; i < thread_num; ++i) {
        threads.emplace_back(testWithPool, op_per_thread);
    }
    for(auto &t : threads) t.join();
    end = now();
    
    printResult("Pool (4-Threads)", TOTAL_OP, get_elapsed_time(start, end));
    cout << "--------------------------------------------------------------------" << endl;

    return 0;
}