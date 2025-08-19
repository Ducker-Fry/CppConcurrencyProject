#pragma once
#include "hierarchical_priority_queue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <algorithm>


void test_single_thread();

void test_multi_thread();

void test_performance();

void test_all();
