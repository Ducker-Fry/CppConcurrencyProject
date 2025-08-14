#include"test.h"
#include<iostream>
#include<vector>
#include"threadsafe_outstream.h"


void test()
{
    // This function is intentionally left empty.
    // It serves as a placeholder for testing purposes.
    std::cout << "Test function called." << std::endl;
}

void test_threadsafequeue_global_mutex()
{
    ThreadSafeQueue<int> queue;
    std::vector<std::thread> producer(5);
    std::vector<std::thread> consumer(5);

    for (int i = 0; i < 5; ++i)
    {
        producer[i] = std::thread([&queue, i]() {
            for (int j = 0; j < 10; ++j)
            {
                queue.push(i * 10 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });
    }

    for (int i = 0; i < 5; ++i)
    {
        consumer[i] = std::thread([&queue]() {
            for (int j = 0; j < 10; ++j)
            {
                int value;
                if (queue.try_pop(value))
                {
                    std::cout << "Consumed: " << value << std::endl;
                }
                else
                {
                    std::cout << "Queue is empty, waiting..." << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            });
    }

    for (auto& t : producer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    for (auto& t : consumer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    std::cout << "All threads have finished execution." << std::endl;
}

void test_bounded_threadsafequeue()
{
    BoundedThreadSafeQueue<int, ThreadSafeQueue<int>> bounded_queue(10);
    std::vector<std::thread> producer(10);
    std::vector<std::thread> consumer(5);

    for (int i = 0; i < 10; ++i)
    {
        producer[i] = std::thread([&bounded_queue, i]() {
            for (int j = 0; j < 10; ++j)
            {
                bounded_queue.push(i * 10 + j);
                {
                    BufFlusher flusher; // 确保线程退出前刷新缓冲区
                    bufferd_out("Producer ", i, " produced: ", (i * 10 + j), "\n");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });
    }

    for (int i = 0; i < 5; ++i)
    {
        consumer[i] = std::thread([&bounded_queue, i]() {
            for (int j = 0; j < 20; ++j)
            {
                auto value = bounded_queue.wait_and_pop();
                if (value)
                {
                    BufFlusher flusher; // 确保线程退出前刷新缓冲区
                    bufferd_out("Consumer ", i, " consumed: ", *value, "\n");
                }
            }
            });
    }
    for (auto& t : producer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    for (auto& t : consumer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    std::cout << "All threads have finished execution." << std::endl;
}

void test_lock_free_queue()
{
    LockFreeQueue<int> lock_free_queue;
    std::vector<std::thread> producer(5);
    std::vector<std::thread> consumer(5);
    for (int i = 0; i < 5; ++i)
    {
        producer[i] = std::thread([&lock_free_queue, i]() {
            for (int j = 0; j < 10; ++j)
            {
                lock_free_queue.enqueue(i * 10 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });
    }
    for (int i = 0; i < 5; ++i)
    {
        consumer[i] = std::thread([&lock_free_queue]() {
            for (int j = 0; j < 10; ++j)
            {
                auto output = lock_free_queue.dequeue();
                if (output.has_value())
                {
                    int value = output.value();
                    if (value)
                    {
                        BufFlusher flusher; // 确保线程退出前刷新缓冲区
                        bufferd_out("Consumed: ", value, "\n");
                    }
                    else
                    {
                        BufFlusher flusher; // 确保线程退出前刷新缓冲区
                        std::cout << "Queue is empty, waiting..." << std::endl;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
            }
            });
    }
    for (auto& t : producer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    for (auto& t : consumer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    std::cout << "All threads have finished execution." << std::endl;
}

void test_threadsafe_priority_queue()
{
    ThreadSafePriorityQueue<int> priority_queue(10);
    std::vector<std::thread> producer(5);
    std::vector<std::thread> consumer(5);

    for (int i = 0; i < 5; ++i)
    {
        producer[i] = std::thread([&priority_queue, i]() {
            for (int j = 0; j < 10; ++j)
            {
                priority_queue.push(i * 10 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });
    }

    for (int i = 0; i < 5; ++i)
    {
        consumer[i] = std::thread([&priority_queue, i]() {
            for (int j = 0; j < 10; ++j)
            {
                int value;
                priority_queue.wait_and_pop(value);
                {
                    BufFlusher flusher; // 确保线程退出前刷新缓冲区
                    bufferd_out("Consumer", i, " consumed: ", value, "\n");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            });
    }

    for(auto& t : producer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    for(auto& t : consumer)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}
