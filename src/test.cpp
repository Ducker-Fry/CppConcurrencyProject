#include"test.h"
#include<iostream>
#include<vector>
#include"threadsafe_outstream.h"

// 全局线程安全输出流
ThreadSafeOutputStream safe_cout(std::cout);

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
                safe_cout << "Produced: " << (i * 10 + j) << std::endl;
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
                    safe_cout << "Consumer " << i << " consumed: " << *value << std::endl;
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
