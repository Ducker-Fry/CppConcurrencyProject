#include"abstract_threadsafe_queue.h"

/*
粗颗粒度的线程安全队列，采用全局互斥锁和条件变量实现,也是最简单的实现方式,最基础的线程安全的队列。
*/
template<typename T>
class ThreadSafeQueue : public AbstractThreadSafeQueue<T>
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
        cond_var_.notify_all(); // 通知等待线程有新元素可用
        //至于为什么不用notify_one()，因为如果wait_and_pop()线程抛出异常，那么刚push进来的元素仍停留在队列中，等待处理。
    }

    // 从队列中获取元素
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false; // 队列为空
        }
        value = std::move(queue_.front());
        if(value)
            queue_.pop();
        else
            return false; // 如果value是空的，返回false
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
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); }); // 等待直到队列不为空
        value = std::move(queue_.front());
        queue_.pop();
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

/*
还是全局锁，但是使用std::shared_ptr来管理队列元素，
*/
namespace ThreadSafeQueueWithSharedPtr
{
    template<typename T>
    class ThreadSafeQueue : public AbstractThreadSafeQueue<T>
    {
    private:
        mutable std::mutex mutex_;
        std::condition_variable wait_condition_;
        std::queue<std::shared_ptr<T>> queue_;

    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void wait_and_pop(T& value)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wait_condition_.wait(lock, [this] { return !queue_.empty(); });
            value = std::move(*queue_.front());
            queue_.pop();
        }

        bool try_pop(T& value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
                return false;
            value = std::move(*queue_.front());
            queue_.pop();
            return true;
        }

        std::shared_ptr<T> try_pop()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
                return nullptr;
            auto value = queue_.front();
            queue_.pop();
            return value;
        }

        std::shared_ptr<T> wait_and_pop()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wait_condition_.wait(lock, [this] { return !queue_.empty(); });
            auto value = queue_.front();
            queue_.pop();
            return value;
        }

        void push(T value)
        {
            std::shared_ptr<T> ptr = std::make_shared<T>(std::move(value));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push(ptr);
            }
            wait_condition_.notify_one();
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }
    };
};
