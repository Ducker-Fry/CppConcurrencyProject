#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <optional>
#include <algorithm>
#include <memory>
#include <chrono>
#include <unordered_map>
#include<shared_mutex>

// 线程局部队列的包装器，包含队列、锁和非空状态
template <typename T, typename Container, typename Compare>
struct ThreadLocalQueue
{
    using QueueType = std::priority_queue<T, Container, Compare>;

    QueueType queue;
    std::recursive_mutex mutex;
    std::atomic<bool> is_non_empty{ false };  // 标记队列是否非空
    std::thread::id owner_id;  // 所属线程ID

    ThreadLocalQueue(std::thread::id id) : owner_id(id) {}

    // 推送元素并更新非空状态
    void push(const T& value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        bool was_empty = queue.empty();
        queue.push(value);
        if (was_empty)
        {
            is_non_empty.store(true, std::memory_order_release);
        }
    }

    void push(T&& value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        bool was_empty = queue.empty();
        queue.push(std::move(value));
        if (was_empty)
        {
            is_non_empty.store(true, std::memory_order_release);
        }
    }

    // 尝试弹出一个元素
    std::optional<T> try_pop()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty())
        {
            is_non_empty.store(false, std::memory_order_release);
            return std::nullopt;
        }

        T value = queue.top();
        queue.pop();

        if (queue.empty())
        {
            is_non_empty.store(false, std::memory_order_release);
        }

        return value;
    }

    // 批量窃取元素,将最多max_steal个元素移动到target队列中
    size_t steal(QueueType& target, size_t max_steal)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty())
        {
            is_non_empty.store(false, std::memory_order_release);
            return 0;
        }

        size_t stolen = 0;
        while (stolen < max_steal && !queue.empty())
        {
            target.push(queue.top());
            queue.pop();
            stolen++;
        }

        if (queue.empty())
        {
            is_non_empty.store(false, std::memory_order_release);
        }

        return stolen;
    }

    // 检查是否为空（不加锁，用于快速判断）
    bool empty_quick() const
    {
        return !is_non_empty.load(std::memory_order_acquire);
    }

    // 合并到目标队列
    void merge_to(QueueType& target)
    {
        std::lock_guard<std::mutex> lock(mutex);
        while (!queue.empty())
        {
            target.push(queue.top());
            queue.pop();
        }
        is_non_empty.store(false, std::memory_order_release);
    }

    std::size_t size()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!is_non_empty)
        {
            return 0;
        }

        return queue.size();
    }
};

template <
    typename T,
    typename Container = std::vector<T>,
    typename Compare = std::less<typename Container::value_type>
>
class HierarchicalPriorityQueue
{
private:
    using QueueType = std::priority_queue<T, Container, Compare>;
    using LocalQueueType = ThreadLocalQueue<T, Container, Compare>;

    // 线程局部队列指针（每个线程拥有一个）
    thread_local static inline std::unique_ptr<LocalQueueType> local_queue_;

    // 全局队列及同步机制
    QueueType global_queue_;
    mutable std::mutex global_mutex_;
    std::condition_variable global_cond_;

    // 非空局部队列列表及同步机制
    std::vector<LocalQueueType*> non_empty_local_queues_;
    mutable std::mutex non_empty_mutex_;

    // 所有线程的局部队列映射
    std::unordered_map<std::thread::id, LocalQueueType*> all_local_queues_;
    mutable std::mutex all_queues_mutex_;

    const size_t local_threshold_;      // 局部队列触发合并的阈值
    const size_t max_steal_;            // 批量窃取的最大数量
    const std::chrono::milliseconds wait_timeout_;  // 等待超时时间
    Compare comp_;

public:
    explicit HierarchicalPriorityQueue(
        size_t local_threshold = 100,
        size_t max_steal = 10,
        std::chrono::milliseconds wait_timeout = std::chrono::milliseconds(100)
    ) : local_threshold_(local_threshold),
        max_steal_(max_steal),
        wait_timeout_(wait_timeout)
    {
        // 初始化当前线程的局部队列
        init_local_queue();
    }

    ~HierarchicalPriorityQueue()
    {
        // 合并当前线程的局部队列到全局队列
        if (local_queue_)
        {
            std::lock_guard<std::mutex> global_lock(global_mutex_);
            local_queue_->merge_to(global_queue_);
        }

        // 从全局列表中移除当前线程的局部队列
        std::lock_guard<std::mutex> all_lock(all_queues_mutex_);
        auto it = all_local_queues_.find(std::this_thread::get_id());
        if (it != all_local_queues_.end())
        {
            remove_from_non_empty(it->second);
            delete it->second;
            all_local_queues_.erase(it);
        }
    }

    // 禁止拷贝，允许移动
    HierarchicalPriorityQueue(const HierarchicalPriorityQueue&) = delete;
    HierarchicalPriorityQueue& operator=(const HierarchicalPriorityQueue&) = delete;

    HierarchicalPriorityQueue(HierarchicalPriorityQueue&& other) noexcept
        : local_threshold_(other.local_threshold_),
        max_steal_(other.max_steal_),
        wait_timeout_(other.wait_timeout_),
        comp_(std::move(other.comp_))
    {
        // 移动全局队列（需加锁）
        std::lock_guard<std::mutex> global_lock(global_mutex_);
        global_queue_ = std::move(other.global_queue_);

        // 移动队列映射（需加锁）
        std::lock_guard<std::mutex> all_lock(all_queues_mutex_);
        all_local_queues_ = std::move(other.all_local_queues_);

        // 移动非空队列列表（需加锁）
        std::lock_guard<std::mutex> non_empty_lock(non_empty_mutex_);
        non_empty_local_queues_ = std::move(other.non_empty_local_queues_);
    }

    HierarchicalPriorityQueue& operator=(HierarchicalPriorityQueue&& other) noexcept
    {
        if (this != &other)
        {
            local_threshold_ = other.local_threshold_;
            max_steal_ = other.max_steal_;
            wait_timeout_ = other.wait_timeout_;
            comp_ = std::move(other.comp_);

            // 移动全局队列（需加锁）
            std::lock_guard<std::mutex> global_lock(global_mutex_);
            global_queue_ = std::move(other.global_queue_);

            // 移动队列映射（需加锁）
            std::lock_guard<std::mutex> all_lock(all_queues_mutex_);
            all_local_queues_ = std::move(other.all_local_queues_);

            // 移动非空队列列表（需加锁）
            std::lock_guard<std::mutex> non_empty_lock(non_empty_mutex_);
            non_empty_local_queues_ = std::move(other.non_empty_local_queues_);
        }
        return *this;
    }

    // 推送元素到局部队列,无需加锁，因为每个线程有自己的局部队列
    void push(const T& value)
    {
        init_local_queue();  // 确保局部队列已初始化

        local_queue_->push(value);

        // 检查是否需要合并到全局队列
        check_and_merge_local();

        // 如果队列刚从空变为非空，添加到非空列表
        if (local_queue_->size() == 1)
        {
            add_to_non_empty(local_queue_.get());
        }
    }

    void push(T&& value)
    {
        init_local_queue();  // 确保局部队列已初始化

        local_queue_->push(std::move(value));

        // 检查是否需要合并到全局队列
        check_and_merge_local();

        // 如果队列刚从空变为非空，添加到非空列表
        if (local_queue_->size() == 1)
        {
            add_to_non_empty(local_queue_.get());
        }
    }

    // 非阻塞弹出元素
    std::optional<T> try_pop()
    {
        // 步骤1：先检查自己的局部队列
        if (local_queue_ && !local_queue_->empty_quick())
        {
            if (auto val = local_queue_->try_pop())
            {
                // 如果队列已空，从非空列表中移除
                if (local_queue_->empty_quick())
                {
                    remove_from_non_empty(local_queue_.get());
                }
                return val;
            }
        }

        // 步骤2：检查全局队列
        {
            std::lock_guard<std::mutex> lock(global_mutex_);
            if (!global_queue_.empty())
            {
                T val = global_queue_.top();
                global_queue_.pop();
                return val;
            }
        }

        // 步骤3：尝试从其他线程的局部队列窃取
        return steal_from_others();
    }

    // 阻塞弹出元素
    T wait_and_pop()
    {
        while (true)
        {
            // 检查自己的局部队列
            if (local_queue_ && !local_queue_->empty_quick())
            {
                if (auto val = local_queue_->try_pop())
                {
                    if (local_queue_->empty_quick())
                    {
                        remove_from_non_empty(local_queue_.get());
                    }
                    return *val;
                }
            }

            // 检查全局队列
            {
                std::lock_guard<std::mutex> lock(global_mutex_);
                if (!global_queue_.empty())
                {
                    T val = global_queue_.top();
                    global_queue_.pop();
                    return val;
                }
            }

            // 尝试窃取
            if (auto val = steal_from_others())
            {
                return *val;
            }

            // 所有队列都为空，等待通知或超时
            std::unique_lock<std::mutex> lock(global_mutex_);
            global_cond_.wait_for(lock, wait_timeout_, [this] {
                // 检查全局队列
                if (!global_queue_.empty()) return true;

                // 检查自己的局部队列
                if (local_queue_ && !local_queue_->empty_quick()) return true;

                // 检查是否有其他非空局部队列
                std::lock_guard<std::mutex> ne_lock(non_empty_mutex_);
                return !non_empty_local_queues_.empty();
                });
        }
    }

    // 检查队列是否为空
    bool empty() const
    {
        // 检查自己的局部队列
        if (local_queue_ && !local_queue_->empty_quick())
        {
            return false;
        }

        // 检查全局队列
        std::lock_guard<std::mutex> lock(global_mutex_);
        if (!global_queue_.empty())
        {
            return false;
        }

        // 检查其他非空局部队列
        std::lock_guard<std::mutex> ne_lock(non_empty_mutex_);
        return non_empty_local_queues_.empty();
    }

    // 获取估计的元素数量
    size_t size() const
    {
        size_t count = 0;

        // 自己的局部队列
        if (local_queue_)
        {
            std::lock_guard<std::recursive_mutex> lock(local_queue_->mutex);
            count += local_queue_->queue.size();
        }

         // 全局队列
        {
            std::lock_guard<std::mutex> global_lock(global_mutex_);
            count += global_queue_.size();
        }  // 锁在这里自动释放

        // 其他局部队列（只是估计值，不加锁）
        std::lock_guard<std::mutex> all_lock(all_queues_mutex_);
        for (const auto& [id, queue] : all_local_queues_)
        {
            if (id == std::this_thread::get_id()) continue;
            if (!queue->empty_quick())
            {
                std::lock_guard<std::mutex> lock(queue->mutex);
                count += queue->queue.size();
            }
        }

        return count;
    }

private:
    // 初始化线程局部队列
    void init_local_queue()
    {
        if (!local_queue_)
        {
            auto id = std::this_thread::get_id();
            local_queue_ = std::make_unique<LocalQueueType>(id);

            // 添加到全局队列映射
            std::lock_guard<std::mutex> lock(all_queues_mutex_);
            all_local_queues_[id] = local_queue_.get();
        }
    }

    // 检查并合并局部队列到全局队列
    void check_and_merge_local()
    {
        if (!local_queue_) return;

        std::lock_guard<std::mutex> lock(local_queue_->mutex);
        if (local_queue_->queue.size() >= local_threshold_)
        {
            std::lock_guard<std::mutex> global_lock(global_mutex_);
            // 合并前先从非空列表移除
            remove_from_non_empty(local_queue_.get());
            // 执行合并
            local_queue_->merge_to(global_queue_);
            // 通知等待的消费者
            global_cond_.notify_one();
        }
    }

    // 将队列添加到非空列表
    void add_to_non_empty(LocalQueueType* queue)
    {
        std::lock_guard<std::mutex> lock(non_empty_mutex_);
        // 检查是否已在列表中
        auto it = std::find(non_empty_local_queues_.begin(),
            non_empty_local_queues_.end(),
            queue);
        if (it == non_empty_local_queues_.end())
        {
            non_empty_local_queues_.push_back(queue);
        }
    }

    // 将队列从非空列表移除
    void remove_from_non_empty(LocalQueueType* queue)
    {
        std::lock_guard<std::mutex> lock(non_empty_mutex_);
        auto it = std::find(non_empty_local_queues_.begin(),
            non_empty_local_queues_.end(),
            queue);
        if (it != non_empty_local_queues_.end())
        {
            non_empty_local_queues_.erase(it);
        }
    }

    // 从其他线程的局部队列窃取元素
    std::optional<T> steal_from_others()
    {
        std::vector<LocalQueueType*> candidates;

        // 复制当前非空队列列表（减少锁持有时间）
        {
            std::lock_guard<std::mutex> lock(non_empty_mutex_);
            if (non_empty_local_queues_.empty())
            {
                return std::nullopt;
            }
            candidates = non_empty_local_queues_;
        }

        // 尝试从每个候选队列窃取
        QueueType stolen_elements;  // 临时存储窃取的元素
        for (LocalQueueType* queue : candidates)
        {
            // 跳过自己的队列
            if (queue->owner_id == std::this_thread::get_id())
            {
                continue;
            }

            std::size_t stolen = 0;
            // 尝试批量窃取
            {
                std::lock_guard<std::recursive_mutex> lock(queue->mutex);
                size_t stolen = queue->steal(stolen_elements, max_steal_);
            }

            if (stolen > 0)
            {
                // 如果队列已空，从非空列表中移除
                if (queue->empty_quick())
                {
                    remove_from_non_empty(queue);
                }

                // 返回窃取到的最高优先级元素
                T result = stolen_elements.top();
                stolen_elements.pop();

                // 将剩余窃取的元素放入自己的局部队列
                if (local_queue_)
                {
                    std::lock_guard<std::mutex> lock(local_queue_->mutex);
                    while (!stolen_elements.empty())
                    {
                        local_queue_->queue.push(stolen_elements.top());
                        stolen_elements.pop();
                    }
                    // 如果自己的队列刚从空变为非空，添加到非空列表
                    if (local_queue_->queue.size() == stolen - 1)
                    {
                        add_to_non_empty(local_queue_.get());
                    }
                }

                return result;
            }
        }

        return std::nullopt;
    }
};

