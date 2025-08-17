#include "batch_queue.h"
#include <thread>
#include <iostream>

int main()
{
    // 创建批量队列：最大批量100，最大等待时间50ms
    BatchQueue<int> q(100, std::chrono::milliseconds(50));

    // 生产者线程：批量入队
    std::thread producer([&]() {
        for (int i = 0; i < 1000; )
        {
            std::vector<int> batch;
            // 每次生成最多50个元素的批量
            for (int j = 0; j < 50 && i < 1000; ++j, ++i)
            {
                batch.push_back(i);
            }
            q.batch_push(std::move(batch)); // 批量入队（移动语义）
        }
        });

    // 消费者线程：批量出队
    std::thread consumer([&]() {
        size_t total = 0;
        while (total < 1000)
        {
            auto batch = q.batch_pop(); // 批量出队（阻塞模式）
            total += batch.size();
            std::cout << "Processed " << batch.size() << " elements (total: " << total << ")" << std::endl;
        }
        });

    producer.join();
    consumer.join();
    return 0;
}
