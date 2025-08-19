#include"utilities.h"
#include<gtest/gtest.h>

// Test case for default initialization
TEST(AtomicCounterTest, DefaultInitialization) {
    AtomicCounter counter;
    EXPECT_EQ(counter.get(), 0);
}

// Test case for add operation
TEST(AtomicCounterTest, AddOperation) {
    AtomicCounter counter;
    counter.add(5);
    EXPECT_EQ(counter.get(), 5);
    
    counter.add();
    EXPECT_EQ(counter.get(), 6);
    
    counter.add(-3);
    EXPECT_EQ(counter.get(), 3);
}

// Test case for get operation
TEST(AtomicCounterTest, GetOperation) {
    AtomicCounter counter;
    EXPECT_EQ(counter.get(), 0);
    
    counter.add(10);
    EXPECT_EQ(counter.get(), 10);
    
    counter.add(5);
    EXPECT_EQ(counter.get(), 15);
}

// Test case for reset operation
TEST(AtomicCounterTest, ResetOperation) {
    AtomicCounter counter;
    counter.add(10);
    EXPECT_EQ(counter.get(), 10);
    
    counter.reset();
    EXPECT_EQ(counter.get(), 0);
}

// Test case for concurrent access
TEST(AtomicCounterTest, ConcurrentAccess) {
    AtomicCounter counter;
    const int numThreads = 10;
    const int incrementsPerThread = 1000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&counter, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                counter.add();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter.get(), numThreads * incrementsPerThread);
    
    // Test reset after concurrent access
    counter.reset();
    EXPECT_EQ(counter.get(), 0);
}

// Test case for negative values
TEST(AtomicCounterTest, NegativeValues) {
    AtomicCounter counter;
    counter.add(-5);
    EXPECT_EQ(counter.get(), -5);
    
    counter.add(10);
    EXPECT_EQ(counter.get(), 5);
    
    counter.add(-3);
    EXPECT_EQ(counter.get(), 2);
}

// Test case for large values
TEST(AtomicCounterTest, LargeValues) {
    AtomicCounter counter;
    const int largeValue = std::numeric_limits<int>::max();
    counter.add(largeValue - 1);
    EXPECT_EQ(counter.get(), largeValue - 1);
    
    // This should not overflow, as std::atomic handles it
    counter.add(1);
    EXPECT_EQ(counter.get(), largeValue);
    
    // Adding more should wrap around (two's complement behavior)
    counter.add(1);
    EXPECT_EQ(counter.get(), std::numeric_limits<int>::min());
}

// Test case for no-throw guarantee
TEST(AtomicCounterTest, NoThrowGuarantee) {
    AtomicCounter counter;
    EXPECT_NO_THROW(counter.add());
    EXPECT_NO_THROW(counter.add(5));
    EXPECT_NO_THROW(counter.add(-3));
    EXPECT_NO_THROW(counter.get());
    EXPECT_NO_THROW(counter.reset());
}
