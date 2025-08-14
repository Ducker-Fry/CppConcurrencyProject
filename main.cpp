#include<iostream>
#include"test.h"

int main() 
{
    // Call the test function to demonstrate its functionality.
    test();
    test_threadsafe_priority_queue();

    std::cout << "Test completed successfully." << std::endl;
    // Indicate successful execution of the program.
    return 0;
}