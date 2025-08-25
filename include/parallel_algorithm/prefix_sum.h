#pragma once
#include <vector>
#include <functional> // 用于std::function
#include <stdexcept>

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
