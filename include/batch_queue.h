#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <optional>
#include <algorithm>
#include <cassert>

template <typename T>
class BatchQueue
{
private:
    using LockGuard = std::lock_guard<std::mutex>;
    using UniqueLock = std::unique_lock<std::mutex>;
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;

    std::vector<T> buffer_;       // 底层缓冲区（连续内存，优化批量操作）
    mutable std::mutex mtx_;      // 保护缓冲区的互斥锁
    std::condition_variable cv_;  // 条件变量，用于阻塞等待元素
    const size_t max_batch_size_; // 最大批量大小（batch_pop的上限）
    const Duration max_wait_time_; // 批量出队的最大等待时间（避免无限阻塞）

public:
    // 构造函数：指定最大批量大小和最大等待时间
    explicit BatchQueue(size_t max_batch_size = 1024, Duration max_wait_time = Duration(100))
        : max_batch_size_(max_batch_size), max_wait_time_(max_wait_time)
    {
        assert(max_batch_size > 0 && "Max batch size must be positive");
    }

    // 批量入队：一次性插入多个元素（支持左值和右值容器）
    void batch_push(const std::vector<T>& elements)
    {
        if (elements.empty()) return; // 空批量无需处理
        LockGuard lock(mtx_);
        buffer_.insert(buffer_.end(), elements.begin(), elements.end());
        cv_.notify_one(); // 通知等待的出队线程
    }

    void batch_push(std::vector<T>&& elements)
    {
        if (elements.empty()) return;
        LockGuard lock(mtx_);
        // 移动元素，避免拷贝（仅当元素支持移动语义）
        buffer_.insert(buffer_.end(),
            std::make_move_iterator(elements.begin()),
            std::make_move_iterator(elements.end()));
        cv_.notify_one();
    }

    // 单个元素入队（兼容普通队列接口）
    void push(const T& element)
    {
        LockGuard lock(mtx_);
        buffer_.push_back(element);
        cv_.notify_one();
    }

    void push(T&& element)
    {
        LockGuard lock(mtx_);
        buffer_.push_back(std::move(element));
        cv_.notify_one();
    }

    // 批量出队（阻塞模式）：最多获取max_batch_size_个元素，若不足则等待至超时或有新元素
    std::vector<T> batch_pop()
    {
        UniqueLock lock(mtx_);
        // 等待条件：缓冲区非空 或 超时（防止永久阻塞）
        cv_.wait_for(lock, max_wait_time_, [this]() { return !buffer_.empty(); });

        return extract_batch(); // 提取批量元素
    }

    // 批量出队（非阻塞模式）：立即返回当前可用的元素（最多max_batch_size_个）
    std::vector<T> try_batch_pop()
    {
        LockGuard lock(mtx_);
        return extract_batch();
    }

    // 批量出队（超时模式）：等待指定时间，若超时则返回已有元素
    std::vector<T> batch_pop_for(Duration wait_time)
    {
        UniqueLock lock(mtx_);
        cv_.wait_for(lock, wait_time, [this]() { return !buffer_.empty(); });
        return extract_batch();
    }

    // 获取当前队列中的元素数量
    size_t size() const
    {
        LockGuard lock(mtx_);
        return buffer_.size();
    }

    // 判断队列是否为空
    bool empty() const
    {
        LockGuard lock(mtx_);
        return buffer_.empty();
    }

    // 清空队列
    void clear()
    {
        LockGuard lock(mtx_);
        buffer_.clear();
    }

private:
    // 从缓冲区提取最多max_batch_size_个元素（需在已加锁状态下调用）
    std::vector<T> extract_batch()
    {
        if (buffer_.empty())
        {
            return {};
        }

        // 确定提取的元素数量（不超过最大批量）
        size_t take = std::min(buffer_.size(), max_batch_size_);
        std::vector<T> result;
        result.reserve(take); // 预分配内存，避免多次扩容

        // 移动元素到结果中（减少拷贝）
        auto it = buffer_.begin() + take;
        std::move(buffer_.begin(), it, std::back_inserter(result));
        // 移除已提取的元素（剩余元素保留在缓冲区）
        buffer_.erase(buffer_.begin(), it);

        return result;
    }
};
