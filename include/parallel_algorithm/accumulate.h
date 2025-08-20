#pragma once
#include <utility>
#include <functional>
#include <iterator>
#include <type_traits> // 用于std::enable_if、std::void_t

// 辅助模板：检测二元操作op是否可调用（C++11兼容版）
template<typename T, typename ElementType, typename BinaryOp, typename = void>
struct is_valid_op : std::false_type {};

// 偏特化：当op(init, element)合法时，为true_type
template<typename T, typename ElementType, typename BinaryOp>
struct is_valid_op<T, ElementType, BinaryOp,
    typename std::enable_if<
    std::is_convertible<
    decltype(std::declval<BinaryOp>()(std::declval<T>(), std::declval<ElementType>())),
    T
    >::value
    >::type
> : std::true_type
{
};

// 主实现：当op可调用时启用
template<typename InputIt, typename T, typename BinaryOp,
    typename std::enable_if<
    is_valid_op<T, typename std::iterator_traits<InputIt>::value_type, BinaryOp>::value,
    int
    >::type = 0
>
T my_accumulate(InputIt first, InputIt last, T init, BinaryOp op)
{
    T result = std::move(init);
    for (; first != last; ++first)
    {
        result = op(std::move(result), *first);
    }
    return result;
}

template<typename InputIt, typename T>
T my_accumulate(InputIt first, InputIt last, T init)
{
    // 如果没有提供二元操作，则使用默认加法
    T result = std::move(init);
    for(; first != last; ++first)
    {
        result += *first; // 默认使用加法
    }

    return result;
}

// 备选实现：当op不可调用时启用，直接返回init
template<typename InputIt, typename T, typename BinaryOp,
    typename std::enable_if<
    !is_valid_op<T, typename std::iterator_traits<InputIt>::value_type, BinaryOp>::value,
    int
    >::type = 0
>
T my_accumulate(InputIt first, InputIt last, T init, BinaryOp op)
{
    // 模板实例化失败时，直接返回初始值
    return init;
}

