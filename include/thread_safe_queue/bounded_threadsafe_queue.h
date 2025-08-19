#include"abstract_threadsafe_queue.h"
#include<atomic>

template<typename T, typename Queue>
class BoundedThreadSafeQueue : public AbstractThreadSafeQueue<T>
{
private:
    Queue queue_;                  // 底层无界队列
    mutable std::mutex mutex_;     // 保护整个有界队列的锁
    std::condition_variable not_empty_cv_;  // 队列不空条件变量
    std::condition_variable not_full_cv_;   // 队列不满条件变量
    const size_t max_size_;        // 最大容量
    size_t current_size_ = 0;      // 当前队列大小(内置计数，避免调用底层size())

public:
    explicit BoundedThreadSafeQueue(size_t max_size)
        : max_size_(max_size)
    {
        if (max_size == 0)
        {
            throw std::invalid_argument("Max size must be greater than 0");
        }
    }

    // 阻塞式入队
    void push(T value) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用内置current_size_判断，避免调用底层size()
        not_full_cv_.wait(lock, [this]() {
            return current_size_ < max_size_;
            });

        // 先入队，成功后再更新计数(保证异常安全)
        queue_.push(std::move(value));
        current_size_++;  // 同步更新当前大小
        not_empty_cv_.notify_one();  // 通知消费者有数据
    }

    // 非阻塞式入队
    bool try_push(T value) 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (current_size_ >= max_size_)
        {
            return false;  // 队列已满
        }

        queue_.push(std::move(value));
        current_size_++;
        not_empty_cv_.notify_one();
        return true;
    }

    // 非阻塞式出队(返回智能指针)
    std::shared_ptr<T> try_pop() override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (current_size_ == 0)
        {
            return nullptr;  // 队列为空
        }

        auto result = queue_.try_pop();
        if (result)
        {  // 确保出队成功
            current_size_--;
            not_full_cv_.notify_one();  // 通知生产者有空间
        }
        return result;
    }

    // 阻塞式出队(返回智能指针)
    std::shared_ptr<T> wait_and_pop() override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用内置current_size_判断，避免调用底层empty()
        not_empty_cv_.wait(lock, [this]() {
            return current_size_ > 0;
            });

        auto result = queue_.wait_and_pop();
        current_size_--;
        not_full_cv_.notify_one();
        return result;
    }

    // 非阻塞式出队(通过引用返回)
    bool try_pop(T& value) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (current_size_ == 0)
        {
            return false;
        }

        bool success = queue_.try_pop(value);
        if (success)
        {
            current_size_--;
            not_full_cv_.notify_one();
        }
        return success;
    }

    // 阻塞式出队(通过引用返回)
    void wait_and_pop(T& value) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this]() {
            return current_size_ > 0;
            });

        queue_.wait_and_pop(value);
        current_size_--;
        not_full_cv_.notify_one();
    }

    // 直接返回内置计数，O(1)复杂度
    size_t size() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_size_;
    }

    // 通过内置计数判断，O(1)复杂度
    bool empty() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_size_ == 0;
    }
};
