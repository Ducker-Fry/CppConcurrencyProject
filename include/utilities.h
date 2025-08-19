#include<atomic>

// A simple atomic counter class that provides thread-safe increment, get, and reset operations.
struct AtomicCounter 
{
    std::atomic<int> count{0};
    
    void add(int value = 1) noexcept;
    int get() const noexcept;
    void reset() noexcept;
};
