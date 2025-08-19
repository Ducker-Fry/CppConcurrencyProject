#include <iostream>
#include <mutex>
#include <sstream>

// 线程安全的输出流
class ThreadSafeOutputStream {
private:
    std::mutex mutex_;
    std::ostream& os_;
    
public:
    ThreadSafeOutputStream(std::ostream& os) : os_(os) {}
    
    template<typename T>
    ThreadSafeOutputStream& operator<<(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        os_ << value;
        return *this;
    }
    
    // 提供操纵符支持
    ThreadSafeOutputStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        std::lock_guard<std::mutex> lock(mutex_);
        os_ << manip;
        return *this;
    }
};

//利用线程本地变量建立缓冲区和批量刷新机制，确保输出流的线程安全
extern std::mutex output_mutex;
extern thread_local std::ostringstream thread_local_buffer;
void flush_thread_local_buffer();

template<typename... Args>
void bufferd_out(Args&&... args) {
    (thread_local_buffer << ... << args);
    if (thread_local_buffer.tellp() > 1024)
    {
        flush_thread_local_buffer();
    }
}

// 线程退出前需手动刷新缓冲区（避免数据残留）
struct BufFlusher {
    ~BufFlusher() { flush_thread_local_buffer(); }
};
