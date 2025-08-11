#include<mutex>
#include<thread>
#include<condition_variable>
#include<queue>

/*
这个头文件主要实现一系列粗颗粒度的线程安全队列，为后面
的多线程编程提供基础设施。
*/


/*
粗颗粒度的线程安全队列，采用全局互斥锁和条件变量实现
*/

template<typename T>
class ThreadSafeQueue
{
private:
    std::queue<T> queue_; // 存储数据的队列
    mutable std::mutex mutex_; // 互斥锁，保护队列
    std::condition_variable cond_var_; // 条件变量，用于通知等待线程
public:
    //构造和析构函数
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // 禁止拷贝构造和赋值
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    //允许移动构造和赋值
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
    }
    ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept
    {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mutex_);
            std::lock_guard<std::mutex> lock2(other.mutex_);
            queue_ = std::move(other.queue_);
        }
        return *this;
    }

    //公共成员函数
    // 添加元素到队列
    void push(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_var_.notify_one(); // 通知一个等待线程
    }

    // 从队列中获取元素
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false; // 队列为空
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true; // 成功获取元素
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr; // 队列为空
        }
        auto value = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return value; // 成功获取元素
    }

    // 阻塞地获取元素，如果队列为空则等待
    bool wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); }); // 等待直到队列不为空
        value = std::move(queue_.front());
        queue_.pop();
        return true; // 成功获取元素
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); }); // 等待直到队列不为空
        auto value = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return value; // 成功获取元素
    }

    // 检查队列是否为空
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty(); // 返回队列是否为空
    }

    // 获取队列的大小
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size(); // 返回队列的大小
    }
};
