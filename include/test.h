#pragma once
#include"threadsafequeue.h"
#include"bounded_threadsafe_queue.h"
#include"lock_free_queue.h"
#include"thread_safe_priority_queue.h"


void test();
void test_threadsafequeue_global_mutex();
void test_bounded_threadsafequeue();
void test_lock_free_queue();
void test_threadsafe_priority_queue();
