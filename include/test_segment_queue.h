#pragma once
#include"segmented_queue.h"

template <typename T, size_t SEG_SIZE>
void test_single_thread_basic();


template <typename T, size_t SEG_SIZE>
void test_multi_thread_concurrent();


template <typename T, size_t SEG_SIZE>
void test_performance();
