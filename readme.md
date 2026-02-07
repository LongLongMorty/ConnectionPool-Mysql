# 🏊‍♂️ MySQL Connection Pool

![Language](https://img.shields.io/badge/language-C%2B%2B11-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux-blue.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)
![License](https://img.shields.io/badge/license-MIT-orange.svg)

**基于 C++11 实现的轻量级、高性能 MySQL 数据库连接池**。
通过复用数据库连接，显著减少 TCP 握手与 MySQL 认证的开销，在高并发场景下将 QPS 提升数倍。

---

## 🚀 关键技术点 (Key Technologies)

![C++](https://img.shields.io/badge/C++-11-blue)
![CMake](https://img.shields.io/badge/Tool-CMake-orange)
![MySQL](https://img.shields.io/badge/Database-MySQL-blue)

- **MySQL Connector/C**：底层数据库驱动
- **线程池模型**：生产者-消费者模型 Design Pattern
- **锁机制**：`mutex`, `unique_lock`, `condition_variable` 实现线程同步
- **C++11 特性**：`shared_ptr`, `lambda`, `atomic`, `chrono`
- **设计模式**：单例模式 (Lazy Singleton)

## 📖 项目背景

为了提高 MySQL 数据库（基于 C/S 设计）的访问瓶颈，除了在服务器端增加缓存服务器（例如 Redis）之外，增加**连接池**是提高 MySQL Server 访问效率的核心手段。

在高并发情况下，大量的 **TCP 三次握手**、**MySQL Server 连接认证**、**MySQL Server 关闭连接回收资源** 和 **TCP 四次挥手** 所耗费的性能时间也是很明显的。连接池通过**复用连接**来消除这一部分的性能损耗。

## ⚙️ 功能特性

连接池一般包含了数据库连接所用的IP地址、端口号、用户名和密码以及其它的性能参数。

- **初始连接量（initSize）**: 预创建连接，减少启动时的冷启动延迟。
- **最大连接量（maxSize）**: 动态扩容，应对突发流量，同时防止耗尽服务器 Socket 资源。
- **最大空闲时间（maxIdleTime）**: 自动回收长时间不用的空闲连接，节省资源。
- **连接超时时间（connectionTimeout）**: 获取连接时的等待超时控制，防止请求无限阻塞。
- **智能指针管理**: 使用 `shared_ptr` 自定义删除器，实现连接的自动归还而非销毁。

## 🛠️ 快速开始 (Build & Run)

### 1. 环境依赖 (Requirements)
- Linux (Tested on Ubuntu 20.04/22.04)
- G++ (Support C++11)
- CMake 3.x
- MySQL Client Development Library (`libmysqlclient-dev`)

```bash
# Ubuntu 安装依赖
sudo apt-get install libmysqlclient-dev
```

### 2. 编译与运行 (Compilation)

```bash
git clone https://github.com/your-repo/MySQLConnectionPool.git
cd MysqlConnectionPool

# 配置文件
# 确保项目根目录下有 mysql.ini 且配置正确

# 编译
mkdir build
cd build
cmake ..
make

# 运行测试
./test_main
```

### 3. 配置文件 (Configuration)

在项目根目录下创建 `mysql.ini`：

```ini
# 数据库连接池配置
ip=127.0.0.1
port=3306
username=pool_user
password=PoolPassword123!
dbname=my_project_db
initSize=10
maxSize=1024
maxIdleTime=60
# 连接超时时间(ms)
connectionTimeout=100
```

## 📊 性能测试 (Benchmark)

为了验证连接池在高并发场景下的性能优势，本项目进行了对比压力测试。
*测试数据: 5,000 次连续 `INSERT` 操作*

| 测试场景 | 耗时 (Time) | QPS (Queries Per Second) | 提升倍数 |
| :--- | :--- | :--- | :--- |
| **标准连接 (无连接池)** | 28.040 s | 178 | 1x (基准) |
| **单线程 (使用连接池)** | 12.883 s | 388 | ~2.2x |
| **4线程并发 (使用连接池)** | 4.696 s | 1064 | **~6.0x** |

> **结论**: 在并发环境下，连接池展现了巨大的性能优势，QPS 提升超过 5 倍。

## 📂 项目结构

```text
├── CMakeLists.txt      # CMake 构建文件
├── mysql.ini           # 配置文件
├── include/            # 头文件
│   ├── CommonConnectionPool.h
│   ├── Connection.h
│   └── public.h
├── src/                # 源代码
│   ├── CommonConnectionPool.cpp
│   └── Connection.cpp
└── test/               # 测试代码
    └── test_main.cpp
```

## 📄 开源许可证 (License)

本项目采用 **MIT License** 开源协议。
这意味着你可以自由地使用、复制、修改、合并、出版发行、散布、再授权及贩售软件的副本。

Copyright (c) 2026 Morty
