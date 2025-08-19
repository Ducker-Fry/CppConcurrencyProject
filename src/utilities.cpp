#include"utilities.h"

 void AtomicCounter::add(int value) noexcept {
    count.fetch_add(value, std::memory_order_relaxed);
}

 int AtomicCounter::get() const noexcept {
    return count.load(std::memory_order_relaxed);
}

 void AtomicCounter::reset() noexcept {
    count.store(0, std::memory_order_relaxed);
}