#include<iostream>
#include"segmented_queue.h"

int main() 
{
    // Call the test function to demonstrate its functionality.
    SegmentedQueue<int, 1024> seg_queue;
    for (int i = 0; i < 5; i++)
    {
        seg_queue.push(i);
        seg_queue.pop();
    }

    std::cout << "Test completed successfully." << std::endl;
    // Indicate successful execution of the program.
    return 0;
}