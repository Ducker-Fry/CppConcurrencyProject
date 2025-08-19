#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <array>

// 单个段的结构：包含固定大小的缓冲区和局部锁
template <typename T, size_t SEGMENT_SIZE = 1024>
struct Segment
{
    std::array<T, SEGMENT_SIZE> buffer;  // 存储元素的缓冲区
    std::mutex mtx;                      // 局部锁：仅保护当前段
    size_t start = 0;                    // 段内有效元素的起始索引
    size_t end = 0;                      // 段内有效元素的结束索引（下一个插入位置）

    // 判断段是否为空
    bool empty() const { return start == end; }

    // 判断段是否已满
    bool full() const { return (end + 1) % SEGMENT_SIZE == start; }  // 预留1个空位区分满/空

    // 入队：向段尾添加元素（需外部加锁）
    bool push(const T& val)
    {
        if (full()) return false;
        buffer[end] = val;
        end = (end + 1) % SEGMENT_SIZE;
        return true;
    }

    // 入队：移动语义（需外部加锁）
    bool push(T&& val)
    {
        if (full()) return false;
        buffer[end] = std::move(val);
        end = (end + 1) % SEGMENT_SIZE;
        return true;
    }

    // 出队：从段头取出元素（需外部加锁）
    std::optional<T> pop()
    {
        if (empty()) return std::nullopt;
        T val = std::move(buffer[start]);
        start = (start + 1) % SEGMENT_SIZE;
        return val;
    }

    // 获取段内元素数量（需外部加锁）
    size_t size() const
    {
        if (end >= start) return end - start;
        return (SEGMENT_SIZE - start) + end;
    }
};

// 分段锁队列
template <typename T, size_t SEGMENT_SIZE>
class SegmentedQueue
{
private:
    using SegmentType = Segment<T, SEGMENT_SIZE>;

    std::vector<std::unique_ptr<SegmentType>> segments_;  // 存储所有段
    std::atomic<size_t> head_segment_ = 0;                // 头部段索引（出队操作的当前段）
    std::atomic<size_t> tail_segment_ = 0;                // 尾部段索引（入队操作的当前段）
    mutable std::mutex global_mtx;                        // 全局锁：仅用于段的创建/销毁和索引调整
    std::condition_variable not_empty_;                   // 用于出队阻塞等待

    // 获取指定索引的段（若不存在则创建）
    std::unique_ptr<SegmentType>& get_segment(size_t idx)
    {
        std::lock_guard<std::mutex> lock(global_mtx);
        if (idx >= segments_.size())
        {
            segments_.resize(idx + 1);
        }
        if (!segments_[idx])
        {
            segments_[idx] = std::make_unique<SegmentType>();
        }
        return segments_[idx];
    }

    // 尝试推进头部段索引（当头部段为空时）
    void advance_head()
    {
        size_t current_head = head_segment_.load();
        auto& current_segment = get_segment(current_head);
        std::lock_guard<std::mutex> lock(current_segment->mtx);

        // 若当前段为空，尝试推进头部索引
        if (current_segment->empty())
        {
            head_segment_.compare_exchange_strong(current_head, current_head + 1);
        }
    }

public:
    SegmentedQueue()
    {
        // 初始化第一个段
        get_segment(0);
    }

    // 入队：向队列尾部添加元素（线程安全）
    void push(const T& val)
    {
        while (true)
        {
            size_t current_tail = tail_segment_.load();
            auto& tail_seg = get_segment(current_tail);
            std::lock_guard<std::mutex> lock(tail_seg->mtx);

            // 尝试在当前尾部段入队
            if (tail_seg->push(val))
            {
                not_empty_.notify_one();  // 通知等待的出队线程
                return;
            }

            // 当前段已满，尝试推进尾部索引（创建新段）
            tail_segment_.compare_exchange_strong(current_tail, current_tail + 1);
        }
    }

    // 入队：移动语义（线程安全）
    void push(T&& val)
    {
        while (true)
        {
            size_t current_tail = tail_segment_.load();
            auto& tail_seg = get_segment(current_tail);
            std::lock_guard<std::mutex> lock(tail_seg->mtx);

            if (tail_seg->push(std::move(val)))  // 移动值到段中
            {
                not_empty_.notify_one();
                return;  // 确保移动后不再访问 val
            }

            // 当前段已满，创建临时副本，然后推进尾部索引
            T temp_val = std::move(val);
            tail_segment_.compare_exchange_strong(current_tail, current_tail + 1);
            val = std::move(temp_val);  // 将值移回，继续尝试
        }
    }

    // 出队：从队列头部取出元素（阻塞等待直到有元素）
    T pop()
    {
        while (true)
        {
            size_t current_head = head_segment_.load();
            auto& head_seg = get_segment(current_head);
            std::unique_lock<std::mutex> lock(head_seg->mtx);

            // 等待当前段非空（或有新段创建）
            not_empty_.wait(lock, [&]() {
                // 检查当前段是否有元素，或头部索引已推进（需重新获取段）
                if (!head_seg->empty()) return true;
                return head_segment_.load() != current_head;
                });

            // 重新检查头部索引（可能已被其他线程推进）
            if (head_segment_.load() != current_head)
            {
                continue;  // 头部已推进，重新循环处理新段
            }

            // 从当前段出队
            if (auto val = head_seg->pop())
            {
                // 若段为空，尝试推进头部索引（供其他线程快速跳过空段）
                if (head_seg->empty()&&head_segment_ != tail_segment_)
                {
                    head_segment_.compare_exchange_strong(current_head, current_head + 1);
                }
                return std::move(*val);
            }
        }
    }

    // 尝试出队：非阻塞，若无元素返回nullopt
    std::optional<T> try_pop()
    {
        size_t current_head = head_segment_.load();
        auto& head_seg = get_segment(current_head);
        std::lock_guard<std::mutex> lock(head_seg->mtx);

        if (auto val = head_seg->pop())
        {
            if (head_seg->empty())
            {
                head_segment_.compare_exchange_strong(current_head, current_head + 1);
            }
            return std::move(*val);
        }

        // 尝试推进头部索引（可能有新段）
        advance_head();
        return std::nullopt;
    }

    // 估算队列大小（非精确值，用于参考）
    size_t approximate_size() const
    {
        std::lock_guard<std::mutex> lock(global_mtx);
        size_t total = 0;
        size_t head = head_segment_.load();
        size_t tail = tail_segment_.load();

        // 遍历有效段并累加大小
        for (size_t i = head; i <= tail; ++i)
        {
            if (i >= segments_.size() || !segments_[i]) continue;
            std::lock_guard<std::mutex> seg_lock(segments_[i]->mtx);
            total += segments_[i]->size();
        }
        return total;
    }

    // 判断队列是否为空（非精确，可能有延迟）
    bool empty() const
    {
        return approximate_size() == 0;
    }
};
