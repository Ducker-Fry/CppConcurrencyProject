#include<iostream>
#include"segmented_queue.h"
#include"test_segment_queue.h"

int main() 
{
    // Call the test function to demonstrate its functionality.
    test_performance<int, 5>();

    std::cout << "Test completed successfully." << std::endl;
    // Indicate successful execution of the program.
    return 0;
}