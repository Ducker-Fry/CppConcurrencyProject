#include"threadsafe_outstream.h"
// 定义全局互斥锁
std::mutex output_mutex;

// 定义 thread_local 变量
thread_local std::ostringstream thread_local_buffer;

void flush_thread_local_buffer()
{
    std::lock_guard<std::mutex> lock(output_mutex);
    std::cout << thread_local_buffer.str();
    thread_local_buffer.str(""); // 清空缓冲区
    thread_local_buffer.clear(); // 重置状态标志
}
