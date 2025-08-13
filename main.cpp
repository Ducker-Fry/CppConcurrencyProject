#include<iostream>
#include"test.h"

int main() 
{
    // Call the test function to demonstrate its functionality.
    test();
    test_bounded_threadsafequeue();

    std::cout << "Test completed successfully." << std::endl;
    // Indicate successful execution of the program.
    return 0;
}