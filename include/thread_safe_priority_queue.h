#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include"abstract_threadsafe_queue.h"

template <
    typename T,
    typename Container = std::vector<T>,
    typename Compare = std::less<typename Container::value_type>
>
class ThreadSafePriorityQueue 
{
private:
    std::priority_queue<T, Container, Compare> queue_;  // 内部优先级队列
    mutable std::mutex mutex_;                          // 保护队列的互斥锁
    std::condition_variable cond_var_;                  // 通知队列状态变化
    const size_t max_size_;                             // 最大容量（0表示无界）
    bool is_bounded_;                                   // 是否为有界队列

public:
    // 构造函数：无界队列
    ThreadSafePriorityQueue()
        : max_size_(0), is_bounded_(false)
    {
    }

    // 构造函数：有界队列（指定最大容量）
    explicit ThreadSafePriorityQueue(size_t max_size)
        : max_size_(max_size), is_bounded_(true)
    {
        if (max_size == 0)
        {
            throw std::invalid_argument("Bounded queue must have max_size > 0");
        }
    }

    // 禁止拷贝（线程安全对象通常不可拷贝）
    ThreadSafePriorityQueue(const ThreadSafePriorityQueue&) = delete;
    ThreadSafePriorityQueue& operator=(const ThreadSafePriorityQueue&) = delete;

    // 允许移动（可选，根据需求开启）
    ThreadSafePriorityQueue(ThreadSafePriorityQueue&&) noexcept = default;
    ThreadSafePriorityQueue& operator=(ThreadSafePriorityQueue&&) noexcept = default;


    // 1. 入队操作（阻塞式）
    // 有界队列：队列满时阻塞，直到有空间
    // 无界队列：直接入队（可能无限增长）
    void push(const T& value) 
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 有界队列需等待空间
        if (is_bounded_)
        {
            cond_var_.wait(lock, [this] {
                return queue_.size() < max_size_;
                });
        }

        queue_.push(std::move(value));
        cond_var_.notify_one();  // 通知消费者有元素可用
    }

    // 移动版本的push（减少拷贝）
    void push(T&& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (is_bounded_)
        {
            cond_var_.wait(lock, [this] {
                return queue_.size() < max_size_;
                });
        }

        queue_.push(std::move(value));
        cond_var_.notify_one();
    }


    // 2. 入队操作（非阻塞式）
    // 成功返回true，队列满时返回false（仅对有界队列有效）
    bool try_push(const T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (is_bounded_ && queue_.size() >= max_size_)
        {
            return false;  // 队列满，入队失败
        }

        queue_.push(value);
        cond_var_.notify_one();
        return true;
    }

    bool try_push(T&& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (is_bounded_ && queue_.size() >= max_size_)
        {
            return false;
        }

        queue_.push(std::move(value));
        cond_var_.notify_one();
        return true;
    }


    // 3. 出队操作（阻塞式）
    // 队列为空时阻塞，直到有元素可用，返回优先级最高的元素
    void wait_and_pop(T& value) 
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待队列非空
        cond_var_.wait(lock, [this] {
            return !queue_.empty();
            });

        value = std::move(queue_.top());  // 获取优先级最高的元素
        queue_.pop();                     // 移除元素

        // 若为有界队列，出队后通知可能阻塞的生产者
        if (is_bounded_)
        {
            cond_var_.notify_one();
        }
    }

    // 返回智能指针（避免值类型的拷贝/移动，适用于大对象）
    std::shared_ptr<T> wait_and_pop() 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] {
            return !queue_.empty();
            });

        auto result = std::make_shared<T>(std::move(queue_.top()));
        queue_.pop();

        if (is_bounded_)
        {
            cond_var_.notify_one();
        }
        return result;
    }


    // 4. 出队操作（非阻塞式）
    // 成功返回true并获取元素，队列为空时返回false
    bool try_pop(T& value) 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
        {
            return false;  // 队列为空，出队失败
        }

        value = std::move(queue_.top());
        queue_.pop();

        if (is_bounded_)
        {
            cond_var_.notify_one();
        }
        return true;
    }

    std::shared_ptr<T> try_pop() 
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
        {
            return nullptr;
        }

        auto result = std::make_shared<T>(std::move(queue_.top()));
        queue_.pop();

        if (is_bounded_)
        {
            cond_var_.notify_one();
        }
        return result;
    }


    // 5. 队列状态查询
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

    // 仅对有界队列有效：返回剩余容量
    size_t remaining_capacity() const
    {
        if (!is_bounded_)
        {
            throw std::logic_error("Only bounded queue supports remaining_capacity()");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return max_size_ - queue_.size();
    }
};
