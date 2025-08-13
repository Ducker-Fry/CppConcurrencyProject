#include<mutex>
#include<thread>
#include<condition_variable>
#include<queue>
#include <memory>
#include <chrono>


/*
这个头文件主要实现一系列粗颗粒度的线程安全队列，为后面
的多线程编程提供基础设施。
*/

//定义线程安全队列的抽象接口
// 抽象接口：模板基类
template <typename T>
class AbstractThreadSafeQueue {
public:
    // 禁止拷贝（线程安全对象通常不可拷贝）
    AbstractThreadSafeQueue(const AbstractThreadSafeQueue&) = delete;
    AbstractThreadSafeQueue& operator=(const AbstractThreadSafeQueue&) = delete;

    AbstractThreadSafeQueue() = default;
    virtual ~AbstractThreadSafeQueue() = default;  // 虚析构函数，确保派生类析构被调用

    // 核心接口：纯虚函数
    virtual void push(T value) = 0;  // 入队（右值引用，支持移动）
    virtual bool try_pop(T& value) = 0;  // 尝试出队（非阻塞，失败返回false）
    virtual std::shared_ptr<T> try_pop() = 0;  // 尝试出队（返回智能指针）
    virtual void wait_and_pop(T& value) = 0;  // 阻塞等待出队
    virtual std::shared_ptr<T> wait_and_pop() = 0;  // 阻塞等待出队（返回智能指针）
    virtual bool empty() const = 0;  // 检查队列是否为空
    virtual size_t size() const = 0;  // 获取队列大小

    // 额外接口：可以根据需要添加
    //void try_pop_for(T& value, std::chrono::milliseconds timeout) = 0;  // 尝试出队，带超时
    //std::shared_ptr<T> try_pop_for(std::chrono::milliseconds timeout) = 0;  // 尝试出队，带超时，返回智能指针
};


/*
粗颗粒度的线程安全队列，采用全局互斥锁和条件变量实现
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

namespace ThreadSafeQueueLinkedList
{
    template<typename T>
    class ThreadSafeQueue : public AbstractThreadSafeQueue<T>
    {
    private:
        struct Node
        {
            T data;
            std::unique_ptr<Node> next;
            Node(T value) : data(std::move(value)), next(nullptr) {}
            Node() : next(nullptr) {} // 默认构造函数用于dummy节点
        };
        std::unique_ptr<Node> head_; //将头节点视为dummy节点，避免头尾指向同一节点的情况
        Node* tail_;
        std::condition_variable cond_var_;
        mutable std::mutex mutex_;

    public:
        ThreadSafeQueue()
            : head_(std::make_unique<Node>()), tail_(head_.get()) // 初始化头节点
        {
        }
        ~ThreadSafeQueue() = default;

        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void push(T value) override
        {
            auto new_node = std::make_unique<Node>(std::move(value));
            Node* new_tail = new_node.get();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                tail_->next = std::move(new_node);
                tail_ = new_tail; // 更新尾指针
            }
            cond_var_.notify_one(); // 通知等待线程有新元素可用
        }

        std::shared_ptr<T> try_pop() override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 若head->next为nullptr，说明队列无实际数据（只有dummy节点）
            if (!head_->next)
            {
                return nullptr;
            }

            // 获取头节点的下一个节点（实际数据节点）
            std::unique_ptr<Node> old_head_next = std::move(head_->next);
            // 提取数据（用shared_ptr返回，延长数据生命周期）
            std::shared_ptr<T> res = std::make_shared<T>(std::move(old_head_next->data));

            // 更新head指向old_head_next->next（即新的头节点的next）
            head_->next = std::move(old_head_next->next);

            // 若移动后head->next为nullptr（队列变空），更新tail指向head（dummy节点）
            if (!head_->next)
            {
                tail_ = head_.get();
            }

            return res;
        }

        bool try_pop(T& value) override
        {
            std::lock_guard<std::mutex> lock(mutex_);  // 注意：原代码中锁的模板参数错写为T，应改为std::mutex

            if (!head_->next)
            {
                // 队列为空时，不修改value，直接返回false
                return false;
            }

            // 1. 移动数据到value（确保value被赋值当且仅当成功出队）
            value = std::move(head_->next->data);

            // 2. 转移智能指针所有权，无需手动delete（避免二次释放）
            auto old_head_next = std::move(head_->next);  // 转移所有权到局部变量
            head_->next = std::move(old_head_next->next);  // 更新头节点的next

            // 3. 局部变量old_head_next离开作用域时，自动释放原节点内存
            return true;
        }

        std::shared_ptr<T> wait_and_pop() override
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this]() { return head_->next != nullptr; }); // 等待直到有元素
            return try_pop(); // 调用try_pop获取元素
        }

        void wait_and_pop(T& value) override
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this]() { return head_->next != nullptr; }); // 等待直到有元素
            try_pop(value); // 调用try_pop获取元素
        }

        bool empty() const override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return head_->next == nullptr; // 只有dummy节点时队列为空
        }

        size_t size() const override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            Node* current = head_.get();
            while (current->next)
            {
                count++;
                current = current->next.get();
            }
            return count;
        }
    };
}

namespace ThreadSafeQueueWithDoubleMutex
{
    template<typename T>
    class ThreadSafeQueue : public AbstractThreadSafeQueue<T>
    {
    private:
        struct Node
        {
            T data;
            std::unique_ptr<Node> next;
        };
        mutable std::mutex head_mutex_; // 保护头节点
        std::mutex tail_mutex_; // 保护尾节点
        std::unique_ptr<Node> head_; // 头节点
        Node* tail_; // 尾节点
        std::condition_variable cond_var_; // 条件变量

        Node* get_tail()
        {
            std::lock_guard<std::mutex> lock(tail_mutex_);
            return tail_;
        }

        std::unique_ptr<Node> pop_head()
        {
            std::lock_guard<std::mutex> lock(head_mutex_);
            if (head_->next == nullptr)
            {
                return nullptr; // 队列为空
            }
            std::unique_ptr<Node> old_head = std::move(head_->next);
            head_->next = std::move(old_head->next); // 更新头节点
            if (head_->next == nullptr) // 如果队列变空，更新尾节点
            {
                tail_ = head_.get();
            }
            return old_head; // 返回旧头节点
        }

    public:
        ThreadSafeQueue()
            : head_(std::make_unique<Node>()), tail_(head_.get()) // 初始化头节点
        {
        }
        ~ThreadSafeQueue() = default;

        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void push(T value) override
        {
            std::unique_ptr<Node> new_node = std::make_unique<Node>();
            new_node->data = std::move(value);
            Node* new_tail = new_node.get();
            {
                std::lock_guard<std::mutex> lock(tail_mutex_);
                tail_->next = std::move(new_node); // 将新节点链接到尾节点
                tail_ = new_tail; // 更新尾节点指针
            }
            cond_var_.notify_one(); // 通知等待线程有新元素可用
        }

        std::shared_ptr<T> try_pop() override
        {
            std::unique_ptr<Node> old_head = pop_head();
            if (!old_head)
            {
                return nullptr; // 队列为空
            }
            return std::make_shared<T>(std::move(old_head->data)); // 返回数据
        }

        bool try_pop(T& value) override
        {
            std::unique_ptr<Node> old_head = pop_head();
            if (!old_head)
            {
                return false; // 队列为空
            }
            value = std::move(old_head->data); // 移动数据到value
            return true; // 成功出队
        }

        std::shared_ptr<T> wait_and_pop() override
        {
            std::unique_lock<std::mutex> lock(head_mutex_);
            cond_var_.wait(lock, [this]() { return head_->next != nullptr; }); // 等待直到有元素
            return try_pop(); // 调用try_pop获取元素
        }

        void wait_and_pop(T& value) override
        {
            std::unique_lock<std::mutex> lock(head_mutex_);
            cond_var_.wait(lock, [this]() { return head_->next != nullptr; }); // 等待直到有元素
            try_pop(value); // 调用try_pop获取元素
        }

        bool try_pop_for(T& value, std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(head_mutex_);
            if (!cond_var_.wait_for(lock, timeout, [this]() { return head_->next != nullptr; }))
            {
                return false; // 超时或队列为空
            }
            return try_pop(value); // 调用try_pop获取元素
        }

        std::shared_ptr<T> try_pop_for(std::chrono::milliseconds timeout) 
        {
            std::unique_lock<std::mutex> lock(head_mutex_);
            if (!cond_var_.wait_for(lock, timeout, [this]() { return head_->next != nullptr; }))
            {
                return nullptr; // 超时或队列为空
            }
            return try_pop(); // 调用try_pop获取元素
        }

        bool empty() const override
        {
            std::lock_guard<std::mutex> lock(head_mutex_);
            return head_->next == nullptr; // 只有dummy节点时队列为空
        }

        std::size_t size() const override
        {
            std::lock_guard<std::mutex> lock(head_mutex_);
            size_t count = 0;
            Node* current = head_.get();
            while (current->next)
            {
                count++;
                current = current->next.get();
            }
            return count; // 返回节点数量
        }
    };
};

/*
改进地方：
1. 使用智能指针管理节点内存，避免手动释放。
2. size函数效率较低，遍历整个队列计算大小，可能需要优化。优化方向
   可以在push和pop时维护一个size原子变量，避免每次都遍历。
3. 超时等待的实现可以通过条件变量的wait_for函数来实现，
   目前的实现是阻塞等待，可能需要添加超时机制。我先实现一个。
4. 异常安全性：目前的实现没有考虑异常安全性，可能需要在push和pop中添加异常处理逻辑。
*/
