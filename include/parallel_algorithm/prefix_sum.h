#pragma once
#include <vector>
#include <functional> // 用于std::function
#include <stdexcept>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>

// 通用前缀和计算函数
// 参数：
// - arr: 原数组
// - op: 二元操作符（输入两个元素，返回运算结果）
// - identity: 单位元（操作的初始值，如加法的0，乘法的1）
template <typename T>
std::vector<T> compute_prefix(
    const std::vector<T>& arr,
    std::function<T(const T&, const T&)> op,
    const T& identity
) {
    if (op == nullptr) {
        throw std::invalid_argument("Operation function cannot be null");
    }

    size_t n = arr.size();
    std::vector<T> prefix(n + 1);
    prefix[0] = identity; // 前缀和数组的起始值为单位元

    // 串行计算前缀和
    for (size_t i = 0; i < n; ++i) {
        prefix[i + 1] = op(prefix[i], arr[i]);
    }

    return prefix;
}

// 常用操作的便捷包装（可选）
namespace PrefixOps {
    // 加法操作（默认前缀和）
    template <typename T>
    std::function<T(const T&, const T&)> add() {
        return [](const T& a, const T& b) { return a + b; };
    }

    // 乘法操作
    template <typename T>
    std::function<T(const T&, const T&)> multiply() {
        return [](const T& a, const T& b) { return a * b; };
    }

    // 取最小值操作
    template <typename T>
    std::function<T(const T&, const T&)> min() {
        return [](const T& a, const T& b) { return std::min(a, b); };
    }

    // 取最大值操作
    template <typename T>
    std::function<T(const T&, const T&)> max() {
        return [](const T& a, const T& b) { return std::max(a, b); };
    }
}

//并发版本前缀和
template <typename T, typename Operation>
std::vector<T> parallel_prefix(const std::vector<T>& arr, Operation op, const T& identity) {
    if (arr.empty()) return { identity };
    
    // 1. 确定线程数量和任务粒度
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;
    size_t min_per_thread = 32;
    size_t max_threads = (arr.size() + min_per_thread - 1) / min_per_thread;
    num_threads = std::min(num_threads, max_threads);

    // 2. 使用连续内存布局
    struct Block {
        T* data;
        size_t size;
        T offset;
    };
    
    // 预分配连续内存
    std::vector<T> all_data(arr.size() + num_threads); // 额外空间存储偏移
    std::vector<Block> blocks;
    blocks.reserve(num_threads);
    
    // 初始化块
    size_t block_size = (arr.size() + num_threads - 1) / num_threads;
    for (size_t i = 0; i < num_threads; ++i) {
        size_t start = i * block_size;
        size_t end = std::min(start + block_size, arr.size());
        if (start < arr.size()) {
            blocks.push_back({all_data.data() + start, end - start, identity});
        }
    }

    // 3. 使用任务并行计算前缀和
    std::vector<std::future<void>> futures;
    std::mutex mtx;
    
    auto process_block = [&](size_t block_idx) {
        auto& block = blocks[block_idx];
        block.data[0] = arr[block.data - all_data.data()];
        
        // 计算块内前缀和
        for (size_t i = 1; i < block.size; ++i) {
            block.data[i] = op(block.data[i-1], arr[block.data - all_data.data() + i]);
        }
        
        // 使用原子操作更新偏移
        {
            std::lock_guard<std::mutex> lock(mtx);
            block.offset = block.data[block.size - 1];
        }
    };

    // 启动任务
    for (size_t i = 0; i < blocks.size(); ++i) {
        futures.push_back(std::async(std::launch::async, process_block, i));
    }

    // 等待所有任务完成
    for (auto& f : futures) f.wait();

    // 4. 计算全局偏移
    std::vector<T> global_offsets(blocks.size());
    for (size_t i = 1; i < blocks.size(); ++i) {
        blocks[i].offset = op(blocks[i-1].offset, blocks[i].offset);
    }

    // 5. 合并结果
    std::vector<T> result;
    result.reserve(arr.size() + 1);
    result.push_back(identity);
    
    // 处理第一个块
    if (!blocks.empty()) {
        result.insert(result.end(), blocks[0].data, blocks[0].data + blocks[0].size);
    }
    
    // 处理剩余块
    for (size_t i = 1; i < blocks.size(); ++i) {
        auto& block = blocks[i];
        T prev_offset = blocks[i-1].offset;
        
        // 计算当前块的结果
        std::vector<T> block_result(block.size);
        block_result[0] = op(block.data[0], prev_offset);
        for (size_t j = 1; j < block.size; ++j) {
            block_result[j] = op(block.data[j], prev_offset);
        }
        
        // 添加到最终结果
        result.insert(result.end(), block_result.begin(), block_result.end());
    }

    return result;
}

// Helper function: sequential prefix sum (for validation)
template <typename T, typename Operation>
std::vector<T> sequential_prefix(const std::vector<T>& arr, Operation op, const T& identity) {
    std::vector<T> result(arr.size() + 1);
    result[0] = identity;
    for (size_t i = 0; i < arr.size(); ++i) {
        result[i + 1] = op(result[i], arr[i]);
    }
    return result;
}

