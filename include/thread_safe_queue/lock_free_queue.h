#include<memory>
#include<atomic>
#include<optional>

//基于Michael-Scott算法的无锁队列实现，使用C++11的原子操作
template<typename T>
class LockFreeQueue
{
private:
	struct Node
	{
		T data;
        std::atomic<Node*> next;
        Node(const T& value) : data(value), next(nullptr) {}
	};

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    LockFreeQueue()
    {
        Node* dummy = new Node(T()); // 创建一个哨兵节点
        head.store(dummy);
        tail.store(dummy);
    }

    ~LockFreeQueue()
    {
        while (dequeue()) {} // 清空队列并释放内存
        delete tail.load(); // 删除哨兵节点
    }

    // 禁止拷贝构造和赋值
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // 允许移动构造和赋值
    LockFreeQueue(LockFreeQueue&& other) noexcept
    {
        if (this != &other)
        {
            head.store(other.head.load());
            tail.store(other.tail.load());
            other.head.store(nullptr);
            other.tail.store(nullptr);

        }
        return this;
    }
    LockFreeQueue& operator=(LockFreeQueue&& other) noexcept
    {
        if (this != &other)
        {
            head.store(other.head.load());
            tail.store(other.tail.load());
            other.head.store(nullptr);
            other.tail.store(nullptr);
        }
        return *this;
    }

    // 添加元素到队列
    void enqueue(const T& value)
    {
        Node* newNode = new Node(value);
        Node* old_tail = tail.load();

        // 尝试将新节点添加到队列的尾部
        while (true)
        {
            if(tail.compare_exchange_weak(
                old_tail, newNode,
                std::memory_order_release,
                std::memory_order_relaxed))
            {
                old_tail->next.store(newNode); // 将新节点链接到旧尾节点
                return; // 成功添加元素
            }
            else
            {
                // 如果尾节点已被其他线程修改，重新获取尾节点
                old_tail = tail.load();
            }
        }
    }

    // 从队列中获取元素
    std::optional<T> dequeue()
    {
        Node* old_head = head.load(std::memory_order_relaxed);
        Node* old_head_next = old_head->next.load(std::memory_order_acquire);

        // 如果队列为空（哨兵节点的next为nullptr）
        if (old_head_next == nullptr)
        {
            return std::nullopt;
        }

        // 尝试更新头指针
        if (head.compare_exchange_strong(
            old_head,
            old_head_next,
            std::memory_order_release,
            std::memory_order_relaxed))
        {
            // 成功获取元素
            //通过将old_head_next的data移动到value中，让他又变成不存储数据的哨兵节点
            T value = std::move(old_head_next->data);
            delete old_head; // 释放旧的头节点（哨兵节点）
            return value;
        }

        // 如果CAS失败，返回nullopt让调用者重试
        return std::nullopt;
    }
};

// 基于数组的无锁队列实现，使用C++11的原子操作
template <typename T>
class LockFreeArrayQueue {
private:
    T* buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    const size_t capacity;
public:
    LockFreeArrayQueue(size_t cap) : buffer(new T[cap]), head(0), tail(0), capacity(cap) {}
    ~LockFreeArrayQueue() { delete[] buffer; }
    bool enqueue(T item) {
        size_t newTail = (tail.load() + 1) % capacity;
        if (newTail == head.load()) { // 队列满
            return false;
        }
        while (true) {
            size_t currTail = tail.load();
            if (currTail == newTail) { // 队列满，或tail被其他线程更新
                continue;
            }
            if (tail.compare_exchange_weak(currTail, newTail)) {
                buffer[currTail] = item;
                return true;
            } // CAS失败，重试
        }
    }
    bool dequeue(T& item) {
        if (head.load() == tail.load()) { // 队列空
            return false;
        }
        while (true) {
            size_t currHead = head.load();
            size_t newHead = (currHead + 1) % capacity;
            if (currHead == tail.load()) { // 队列空，或head被其他线程更新
                continue;
            }
            if (head.compare_exchange_weak(currHead, newHead)) {
                item = buffer[currHead];
                return true;
            } // CAS失败，重试
        }
    }
};