#include <iostream>
#include <cstdio>
#include "CommonConnectionPool.h"

using namespace std;

int main() 
{
    ConnectionPool* pool = ConnectionPool::getConnectionPool();
    cout << (pool->loadConfigFile() ? "loadConfigFile ok" : "loadConfigFile failed") << endl;
    return 0;
}