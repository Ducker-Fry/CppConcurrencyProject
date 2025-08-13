#pragma once
#include<mutex>
#include<thread>
#include<condition_variable>
#include<queue>
#include <memory>
#include <chrono>


/*
作为线程安全队列的抽象接口，定义了基本的操作，作为整个队列家族的 “契约”。
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
