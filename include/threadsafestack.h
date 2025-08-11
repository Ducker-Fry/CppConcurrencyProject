#include<exception>
#include<thread>
#include<mutex>
#include<stack>
#include<shared_mutex>

struct stack_empty : public std::exception
{
    const char* what() const noexcept override
    {
        return "Stack is empty";
    }
};

template<typename T>
class thread_safe_stack
{
private:
    std::stack<T> stack_;
    mutable std::shared_mutex mutex_;

public:
    thread_safe_stack() = default;
    thread_safe_stack(const thread_safe_stack&) = delete; // no copy
    thread_safe_stack& operator=(const thread_safe_stack&) = delete; // no assignment
    thread_safe_stack(thread_safe_stack&& other) noexcept
    {
        std::lock_guard<std::shared_mutex> lock(other.mutex_);
        stack_ = std::move(other.stack_);
    }
    thread_safe_stack& operator=(thread_safe_stack&& other) noexcept
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex_, other.mutex_); // 同时锁两个互斥量，避免死锁
            stack_ = std::move(other.stack_);
        }
        return *this;
    }
    ~thread_safe_stack() = default;

    void push(T value)
    { // 传值，在锁外完成拷贝/移动
        std::unique_lock<std::shared_mutex> lock(mutex_);
        stack_.push(std::move(value)); // 锁内仅执行移动构造（高效）
    }

    std::shared_ptr<T> pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stack_.empty())
        {
            throw stack_empty();
        }
        std::shared_ptr<T> value(std::make_shared<T>(std::move(stack_.top())));
        stack_.pop();
        return value;
    }

    void pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stack_.empty())
        {
            throw stack_empty();
        }
        value = std::move(stack_.top());
        stack_.pop();
    }

    bool try_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stack_.empty())
        {
            return false;
        }
        value = std::move(stack_.top());
        stack_.pop();
        return true;
    }

    bool try_pop(std::shared_ptr<T>& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stack_.empty())
        {
            return false;
        }
        value = std::make_shared<T>(std::move(stack_.top()));
        stack_.pop();
        return true;
    }

    bool empty() const
    {
        std::shared_lock<std::mutex> lock(mutex_);
        return stack_.empty();
    }
};
