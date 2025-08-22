#pragma once
#include <type_traits> // 用于std::enable_if、std::void_t
#include <cstddef> // 用于std::nullptr_t
#include <algorithm>
#include <vector>
#include <functional>

/*
对于串行的merge_sort函数，要求自定义类型要实现<或者<=,==和默认的构造函数，否则无法实例化。
*/


// 辅助模板：检测T是否支持 a < b 操作
template <typename T, typename = void>
struct has_less_operator : std::false_type {};

// 偏特化：当 T 支持 a < b 时，为true_type
template <typename T>
struct has_less_operator<T,
    std::void_t<decltype(std::declval<T>() < std::declval<T>())>
    > : std::true_type {};

// 安全比较器：对支持<的类型正常比较，否则降级处理
template <typename T, typename Enable = void>
struct SafeComparator
{
    // 降级版本：T不支持<时启用，默认返回false（或按内存地址比较）
    bool operator()(const T& a, const T& b) const
    {
        // 选项1：始终返回false（视为a >= b，可能导致排序无序）
        // return false;

        // 选项2：按内存地址比较（至少保证排序稳定，但逻辑可能不符合预期）
        return &a < &b;
    }
};

// 特化版本：T支持<时启用，使用正常比较逻辑
template <typename T>
struct SafeComparator<T,
    std::enable_if_t<has_less_operator<T>::value>
>
{
    bool operator()(const T& a, const T& b) const
    {
        return a < b; // 使用T的<运算符
    }
};

// 归并排序中使用安全比较器（确保编译不报错）
template <typename T>
void merge_sort(std::vector<T>& arr)
{
    // 使用SafeComparator替代默认比较器
    using Comparator = SafeComparator<T>;
    merge_sort_with_comp(arr, Comparator());
}

template <typename T>
void merge_sort_iterative(std::vector<T>& arr)
{
    // 使用SafeComparator替代默认比较器
    using Comparator = SafeComparator<T>;
    merge_sort_iterative(arr, Comparator());
}

// 内部实现（带比较器参数）
template <typename T, typename Compare>
void merge_sort_with_comp(std::vector<T>& arr, Compare comp)
{
    if (arr.size() <= 1) return;
    std::vector<T> buffer(arr.size());
    merge_sort(arr.data(), arr.data() + arr.size(), buffer.data(), comp);
}

// 递归归并排序实现（省略细节，与之前类似，但使用传入的comp）
template <typename T, typename Compare>
void merge_sort(T* first, T* last, T* buffer, Compare comp)
{
    const size_t n = last - first;
    if (n <= 1) return;
    T* mid = first + n / 2;
    merge_sort(first, mid, buffer, comp);
    merge_sort(mid, last, buffer, comp);
    merge(first, mid, last, buffer, comp); // 合并时使用comp
}

// 合并函数（使用传入的比较器）
template <typename T, typename Compare>
void merge(T* first, T* mid, T* last, T* buffer, Compare comp)
{
    T* left = first;
    T* right = mid;
    T* out = buffer;

    while (left < mid && right < last)
    {
        if (!comp(*right, *left))
        { // 使用安全比较器
            *out++ = *left++;
        }
        else
        {
            *out++ = *right++;
        }
    }

    std::copy(left, mid, out);
    std::copy(right, last, out);
    std::copy(buffer, buffer + (last - first), first);
}

// 非递归归并排序实现（省略细节，与之前类似，但使用传入的comp）
template <typename T,typename Comp>
void merge_sort_iterative(std::vector<T>& arr,Comp comp)
{
    if (arr.empty()) return;
    const size_t n = arr.size();
    std::vector<T> buffer(n); // 临时缓冲区

    // 按子数组大小递增合并：1, 2, 4, ..., 直到超过n/2
    for (size_t block_size = 1; block_size < n; block_size *= 2)
    {
        // 遍历所有子数组对，合并相邻的两个块
        for (size_t left = 0; left < n; left += 2 * block_size)
        {
            size_t mid = std::min(left + block_size, n); // 左半部分结束位置
            size_t right = std::min(left + 2 * block_size, n); // 右半部分结束位置

            // 合并[left, mid)和[mid, right)
            merge(arr.data() + left, arr.data() + mid, arr.data() + right, buffer.data() + left,comp);
        }
    }
}
