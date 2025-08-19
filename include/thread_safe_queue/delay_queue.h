#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <algorithm>
#include <memory>

// 延迟队列中的元素包装类：包含实际数据和到期时间
template <typename T>
struct DelayElement
{
    T data;  // 实际数据
    std::chrono::steady_clock::time_point expire_time;  // 到期时间（绝对时间）

    // 用于优先队列的比较器（小顶堆：最早到期的元素在堆顶）
    bool operator<(const DelayElement& other) const
    {
        // 注意：std::priority_queue是大顶堆，此处反转比较逻辑实现小顶堆
        return expire_time > other.expire_time;
    }
};

// 延迟队列类
template <typename T>
class DelayQueue
{
private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    std::priority_queue<DelayElement<T>> queue_;  // 优先队列（按到期时间排序）
    mutable std::mutex mtx_;                     // 保护队列的互斥锁
    std::condition_variable cv_;                 // 条件变量，用于等待元素到期

public:
    // 入队：添加元素，指定延迟时间（相对时间，如5秒后到期）
    void push(const T& data, Duration delay)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // 计算绝对到期时间（当前时间 + 延迟时间）
        TimePoint expire_time = Clock::now() + delay;
        queue_.push({ data, expire_time });
        // 唤醒可能等待的消费者（若新元素是最早到期的，需重新计算等待时间）
        cv_.notify_one();
    }

    // 入队：支持移动语义
    void push(T&& data, Duration delay)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        TimePoint expire_time = Clock::now() + delay;
        queue_.push({ std::move(data), expire_time });
        cv_.notify_one();
    }

    // 出队：阻塞等待，直到有元素到期并返回（返回值包含数据）
    T pop()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        while (true)
        {
            // 检查堆顶元素是否到期
            if (!queue_.empty())
            {
                const auto& top = queue_.top();
                // 若已到期，取出并返回
                if (Clock::now() >= top.expire_time)
                {
                    T data = std::move(const_cast<DelayElement<T>&>(top).data);  // 安全移动（即将弹出）
                    queue_.pop();
                    return data;
                }
                // 未到期：等待到到期时间
                cv_.wait_until(lock, top.expire_time);
            }
            else
            {
                // 队列为空：无限期等待，直到有元素入队
                cv_.wait(lock);
            }
        }
    }

    // 尝试出队：非阻塞，若有到期元素则返回，否则返回nullopt
    std::optional<T> try_pop()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty())
        {
            return std::nullopt;
        }
        const auto& top = queue_.top();
        if (Clock::now() >= top.expire_time)
        {
            T data = std::move(const_cast<DelayElement<T>&>(top).data);
            queue_.pop();
            return data;
        }
        return std::nullopt;  // 无到期元素
    }

    // 获取队列中最早到期的元素的剩余延迟时间（若队列为空，返回nullopt）
    std::optional<Duration> next_delay() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty())
        {
            return std::nullopt;
        }
        TimePoint now = Clock::now();
        const auto& top = queue_.top();
        if (top.expire_time <= now)
        {
            return Duration::zero();  // 已到期
        }
        return top.expire_time - now;  // 剩余延迟
    }

    // 清空队列
    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!queue_.empty())
        {
            queue_.pop();
        }
    }

    // 队列是否为空
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    // 队列中元素数量
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
};
