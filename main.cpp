#include<iostream>
#include"test.h"


int main() 
{
    // Call the test function to demonstrate its functionality.
    test();

    // Call the test for the ThreadSafeQueue with global mutex.
    ThreadSafeQueue<int> queue;
    ThreadSafeQueueLinkedList::ThreadSafeQueue<int> linked_list_queue;
    //test_threadsafequeue_global_mutex(queue);
    linked_list_queue.push(42);
    std::cout<<linked_list_queue.empty();
    linked_list_queue.push(43);
    std::shared_ptr<int> value = linked_list_queue.try_pop();
    
    // Indicate successful execution of the program.
    return 0;
}