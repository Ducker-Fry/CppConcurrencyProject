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