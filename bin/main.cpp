#include "../lib/MemoryPoolAllocator.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <list>
#include <fstream>

std::array<bucket, 2> buckets{bucket(8, 100000000), bucket(24, 100000000)};


int main() {
    MemoryPoolAllocator<int, 2> alloc(buckets);
    std::vector<int, MemoryPoolAllocator<int, 2>> custom_vector(alloc);
    std::vector<int> standard_vector;
    std::ofstream file("list_test.csv");
    for (int i = 1; i <= 100000000; i = i * 10) {
        // custom
        file << i << ",";
        auto custom_start = std::chrono::high_resolution_clock::now();
        for (int _ = 0; _ < i; ++_) {
            custom_vector.push_back(1);
        }
        auto custom_stop = std::chrono::high_resolution_clock::now();
        auto custom_duration = duration_cast<std::chrono::microseconds>(custom_stop - custom_start);
        file << custom_duration.count();
        file << ",";
        // standard
        auto standard_start = std::chrono::high_resolution_clock::now();
        for (int _ = 0; _ < i; ++_) {
            standard_vector.push_back(1);
        }
        auto standard_stop = std::chrono::high_resolution_clock::now();
        auto standard_duration = duration_cast<std::chrono::microseconds>(standard_stop - standard_start);
        file << standard_duration.count() << "\n";
    }

    return 0;
}