#pragma once
#include <utility>
#include <functional>
#include <iterator>
#include <type_traits> // 用于std::enable_if、std::void_t
#include <algorithm>
#include <thread>
#include <numeric>
#include <string>

// 1. 单位元获取模板（默认需要用户显式提供，或为标准操作特化）
template <typename BinaryOp, typename T>
struct identity_element
{
    // 若未找到特化，编译报错（提示用户提供单位元）
    static_assert(std::is_same_v<BinaryOp, BinaryOp>,
        "No identity element defined for this operation. Provide explicit identity.");
    static T get() { return T(); }
};

// 2. 为标准操作提供单位元特化
template <typename T>
struct identity_element<std::plus<T>, T>
{
    static T get() { return T(0); } // 加法单位元：0
};

template <typename T>
struct identity_element<std::multiplies<T>, T>
{
    static T get() { return T(1); } // 乘法单位元：1
};

template <>
struct identity_element<std::plus<std::string>, std::string>
{
    static std::string get() { return ""; } // 字符串拼接单位元：空字符串
};


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
    for (; first != last; ++first)
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


//AI给的并行版本实现
template<typename InputIt, typename T, typename BinaryOp>
T parallel_accumulate(InputIt first, InputIt last, T init, BinaryOp op)
{
    // 计算范围大小
    auto length = std::distance(first, last);
    if (length == 0) return init; // 如果范围为空，直接返回初始值
    // 使用线程数来划分任务
    const size_t min_per_thread = 25; // 每个线程至少处理的元素数
    const size_t max_threads = (length + min_per_thread - 1) / min_per_thread; // 计算最大线程数
    const size_t num_threads = std::min(max_threads, static_cast<size_t>(std::thread::hardware_concurrency())); // 获取硬件支持的线程数
    if (num_threads == 0)  return init; // 直接返回初始值，如果没有可用线程
    auto block_size = length / num_threads;


    std::vector<T> results(num_threads);
    // 启动多个线程进行并行计算
    std::vector<std::thread> threads;
    // 获取当前操作的单位元（局部初始值）
    const T local_init = identity_element<BinaryOp, T>::get();
    for (unsigned i = 0; i < num_threads; ++i)
    {
        auto block_start = first + i * block_size;
        auto block_end = (i == num_threads - 1) ? last : block_start + block_size;
        threads.emplace_back([block_start, block_end, &results, i, op, local_init]()
            {
                /*
                * 使用条件分支是因为T()作为初始值时，可能会导致结果不正确,例如如果T是int类型，初始值为0，乘法操作会导致结果为0
auto distance = std::distance(block_start, block_end);
                if (distance == 1) results[i] = *block_start; // 只有一个元素，直接赋值
                else if (distance == 2)
                {
                    results[i] = op(*block_start, *(block_start + 1)); // 两个元素，直接计算
                }
                else
                {
                    results[i] = std::accumulate(block_start + 1, block_end, *block_start, op); // 多元素：用第一个元素作为初始值，累加剩余元素
                }

                */

                //使用单位元模板优化
                results[i] = std::accumulate(block_start, block_end, local_init, op);
            });
    }
    // 等待所有线程完成
    for (auto& t : threads)
    {
        t.join();
    }
    // 合并结果
    if (results.size() == 1) return results[0]; // 如果只有一个线程，直接返回结果
    return my_accumulate(results.begin(), results.end(), init, op);
}

template<typename InputIt, typename T>
T parallel_accumulate(InputIt first, InputIt last, T init)
{
    auto op = [](T a, T b) { return a + b; }; // 默认加法操作
    // 如果没有提供二元操作，则使用默认加法
    return parallel_accumulate(first, last, init, op);
}


// 并行accumulate实现
template <typename RandomIt, typename T, typename BinaryOp>
T parallel_accumulate(RandomIt first, RandomIt last, T init, BinaryOp op,
    size_t num_threads)
{
    // 1. 处理空范围或单线程场景（直接调用串行版本）
    const size_t total_elements = std::distance(first, last);
    if (total_elements == 0) return init;
    if (num_threads == 0) num_threads = 1; // 避免线程数为0
    num_threads = std::min(num_threads, total_elements); // 线程数不超过元素数
    const size_t effective_threads = std::min(num_threads, static_cast<size_t>(std::thread::hardware_concurrency()));

    // 2. 划分每个线程处理的子范围
    const size_t block_size = total_elements / effective_threads;
    std::vector<T> local_results(effective_threads); // 存储每个线程的局部结果
    std::vector<std::thread> threads;
    threads.reserve(effective_threads);

    // 3. 启动线程处理子范围
    RandomIt current = first;
    for (size_t i = 0; i < effective_threads; ++i)
    {
        RandomIt block_last = current;
        if (i == effective_threads - 1)
        {
            block_last = last; // 最后一个线程处理剩余所有元素
        }
        else
        {
            std::advance(block_last, block_size);
        }

        // 每个线程计算局部累加结果
        threads.emplace_back(
            [=, &local_results, &op](size_t thread_idx) {
                // 对当前子块调用串行accumulate，初始值为T()（需确保T可默认构造）
                local_results[thread_idx] = std::accumulate(current, block_last, T(), op);
            },
            i // 传递线程索引
        );

        current = block_last;
    }

    // 4. 等待所有线程完成
    for (auto& t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    // 5. 合并所有局部结果（用初始值init开始，对local_results累加）
    return std::accumulate(local_results.begin(), local_results.end(), init, op);
}

