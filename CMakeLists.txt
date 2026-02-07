cmake_minimum_required(VERSION 3.10)
project(MysqlConnectionPool LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 寻找系统中的 MySQL 开发库与头文件
find_library(MYSQL_LIB mysqlclient)
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)

if (NOT MYSQL_LIB)
	message(FATAL_ERROR "mysqlclient library not found. Install MySQL client dev package.")
endif()

if (NOT MYSQL_INCLUDE_DIR)
	message(FATAL_ERROR "mysql headers not found (mysql/mysql.h). Install MySQL client dev package.")
endif()

# 构建连接池核心为库
add_library(mysql_connection_pool
	src/Connection.cpp
	src/CommonConnectionPool.cpp
)

target_include_directories(mysql_connection_pool
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
		${MYSQL_INCLUDE_DIR}
)
target_link_libraries(mysql_connection_pool
	PUBLIC
		${MYSQL_LIB}
		Threads::Threads
)

# 测试可执行程序
add_executable(test_main test/test_main.cpp)
target_link_libraries(test_main
	PRIVATE
		mysql_connection_pool
		Threads::Threads
)