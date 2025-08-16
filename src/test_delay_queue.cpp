#include"test_delay_queue.h"
#include<iostream>

void test_delay_queue()
{
    DelayQueue<int> dq;
    // 入队一些元素，指定不同的延迟时间
    dq.push(1, std::chrono::seconds(3)); // 3秒后到期
    dq.push(2, std::chrono::seconds(1)); // 1秒后到期
    dq.push(3, std::chrono::seconds(5)); // 5秒后到期
    // 出队并打印元素，应该按到期时间顺序返回
    for (int i = 0; i < 3; ++i)
    {
        int value = dq.pop();
        std::cout << "Popped value: " << value << std::endl;
    }
}