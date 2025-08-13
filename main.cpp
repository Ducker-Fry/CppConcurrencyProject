#include<iostream>
#include"test.h"


int main() 
{
    // Call the test function to demonstrate its functionality.
    test();

    // Call the test for the ThreadSafeQueue with global mutex.
    test_threadsafequeue_global_mutex();
    // Indicate successful execution of the program.
    return 0;
}