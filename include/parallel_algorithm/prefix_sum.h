#pragma once
#include <vector>
#include <functional> // 用于std::function
#include <stdexcept>
#include <thread>

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
    
    // 1. 确定线程数量
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;
    size_t min_per_thread = 25;
    size_t max_threads = (arr.size() + min_per_thread - 1) / min_per_thread;
    num_threads = std::min(num_threads, max_threads);

    // 2. 划分数据块并预分配内存
    size_t block_size = (arr.size() + num_threads - 1) / num_threads;
    std::vector<std::vector<T>> chunks;
    chunks.reserve(num_threads);
    
    // 预分配所有块的内存
    for (size_t i = 0; i < num_threads; ++i) {
        size_t start = i * block_size;
        size_t end = std::min(start + block_size, arr.size());
        if (start < arr.size()) {
            chunks.emplace_back(end - start + 1);
        }
    }

    std::vector<T> offset(num_threads, identity);
    std::vector<std::thread> threads;

    // 3. 计算每个块的前缀和
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            size_t start = i * block_size;
            size_t end = std::min(start + block_size, arr.size());
            if (start < arr.size()) {
                auto& chunk = chunks[i];
                chunk[0] = identity;
                for (size_t j = start; j < end; ++j) {
                    chunk[j - start + 1] = op(chunk[j - start], arr[j]);
                }
                offset[i] = chunk.back();
            }
        });
    }

    for (auto& t : threads) t.join();

    // 4. 计算偏移量（使用并行归约）
    for (size_t i = 1; i < num_threads; ++i) {
        offset[i] = op(offset[i - 1], offset[i]);
    }

    // 5. 合并结果（使用并行处理）
    for (size_t i = 1; i < num_threads; ++i) {
        auto& chunk = chunks[i];
        const T& prev_offset = offset[i - 1];
        // 使用并行for循环处理每个块
        #pragma omp parallel for
        for (size_t j = 1; j < chunk.size(); ++j) {
            chunk[j] = op(chunk[j], prev_offset);
        }
    }

    // 6. 拼接结果（使用连续内存）
    std::vector<T> result;
    result.reserve(arr.size() + 1);
    result.push_back(identity);
    
    // 预分配所有块的内存
    for (const auto& chunk : chunks) {
        if (!chunk.empty()) {
            result.insert(result.end(), 
                         std::make_move_iterator(chunk.begin() + 1), 
                         std::make_move_iterator(chunk.end()));
        }
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

