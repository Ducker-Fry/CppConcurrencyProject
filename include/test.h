#pragma once
#include"threadsafequeue.h"
void test();
template<typename T>
void test_threadsafequeue_global_mutex(AbstractThreadSafeQueue<T>& queue);
