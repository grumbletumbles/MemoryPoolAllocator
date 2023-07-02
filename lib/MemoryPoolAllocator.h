#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>


// A memory pool is split into buckets, each one
// of which is split in chunks(blocks) of fixed size
// Allocator is aware of a single memory pool
// A memory pool has multiple buckets
class bucket {
public:
    const size_t BlockSize;
    const size_t BlockCount;
    bucket(size_t block_size, size_t block_count)
        : BlockSize(block_size)
        , BlockCount(block_count) {
        const auto data_size = BlockCount * BlockSize;
        data_ = static_cast<uint8_t*>(malloc(data_size));
        const auto ledger_size = 1 + ((BlockCount - 1) / 8);
        ledger_ = static_cast<uint8_t*>(malloc(ledger_size));
        std::memset(data_, 0, data_size);
        std::memset(ledger_, 0, ledger_size);
    }

    ~bucket() {
        free(data_);
        free(ledger_);
    }

    bool belongs(void* ptr) const {
        return (data_ <= ptr) && (ptr < data_ + BlockSize * BlockCount);
    }

    void* allocate(size_t bytes) {
        // how many blocks we need
        const auto n = 1 + ((bytes - 1) / BlockSize);
        const auto index = find_contiguous_blocks(n);
        if (index == BlockCount) {
            return nullptr;
        }
        set_used(index, n);
        return data_ + (index * BlockSize);
    }

    void deallocate(void* ptr, size_t bytes) {
        const auto p = static_cast<uint8_t*>(ptr);
        const auto distance = static_cast<size_t>(p - data_);
        // find block #
        const auto index = distance / BlockSize;
        // how many blocks to free
        const auto n = 1 + ((bytes - 1) / BlockSize);
        set_free(index, n);
    }

private:
    // returns BlockCount when there are no such blocks
    size_t find_contiguous_blocks(size_t n) const {
        size_t count = 0;
        size_t shift = 0;
        size_t begin_index = 0;
        bool found = false;
        const auto ledger_size = 1 + ((BlockCount - 1) / 8);
        for (int i = 0; i < ledger_size; ++i) {
            uint8_t cur = ledger_[i];
            for (int j = 7; j>= 0; --j) {
                if (!(cur & (1 << j))) {
                    count++;
                    if (count == n) {
                        found = true;
                        break;
                    }
                } else {
                    count = 0;
                    begin_index = i;
                    shift = (8 - j) % 8;
                }
            }
            if (found) {
                return 8 * begin_index + shift;
            }
        }
        return BlockCount;
    }

    void set_used(size_t index, size_t n) {
        size_t byte_index = index / 8;
        size_t bit_index = index % 8;
        size_t bits_remaining = n;
        uint8_t mask = (1 << (8 - bit_index)) - 1;
        // need to set less bits than until the end of current byte
        if (bits_remaining < 8 - bit_index) {
            mask -= (1 << (8 - bit_index - bits_remaining)) - 1;
        }
        ledger_[byte_index] |= mask;
        bits_remaining -= std::min<size_t>(n, 8 - bit_index);

        while(bits_remaining > 0) {
            byte_index++;
            mask = 0xFF;
            size_t bits_to_set = std::min<size_t>(bits_remaining, 8);
            if (bits_to_set < 8) {
                mask -= (1 << (8 - bits_to_set)) - 1;
            }
            ledger_[byte_index] |= mask;
            bits_remaining -= bits_to_set;
        }
    }

    void set_free(size_t index, size_t n) {
        size_t byte_index = index / 8;
        size_t bit_index = index % 8;
        size_t bits_remaining = n;
        uint8_t mask = (1 << (8 - bit_index)) - 1;
        // need to set less bits than until the end of current byte
        if (bits_remaining < 8 - bit_index) {
            mask -= (1 << (8 - bit_index - bits_remaining)) - 1;
        }
        mask = ~mask;
        ledger_[byte_index] &= mask;
        bits_remaining -= std::min<size_t>(n, 8 - bit_index);

        while(bits_remaining > 0) {
            byte_index++;
            mask = 0xFF;
            size_t bits_to_set = std::min<size_t>(bits_remaining, 8);
            if (bits_to_set < 8) {
                mask -= (1 << (8 - bits_to_set)) - 1;
            }
            mask = ~mask;
            ledger_[byte_index] &= mask;
            bits_remaining -= bits_to_set;
        }
    }

    uint8_t* data_;
    uint8_t* ledger_;
};

// used to determine from which bucket to allocate memory
// so that wasted memory is minimal
struct info {
    size_t index{0};
    size_t block_count{0};
    size_t waste{0};

    bool operator<(const info& other) const {
        return (waste == other.waste) ? block_count < other.block_count : waste < other.waste;
    }
};

template<typename T, size_t bucket_count>
class MemoryPoolAllocator {
public:
    typedef T                   value_type;
    typedef value_type*         pointer;

    template<typename U>
    struct rebind{ using other = MemoryPoolAllocator<U, bucket_count>; };

    MemoryPoolAllocator(std::array<bucket, bucket_count>& pool) : pool_(pool) {};

    template<typename U>
    MemoryPoolAllocator(const MemoryPoolAllocator<U, bucket_count>& other) : pool_(other.pool_) {}

    template<typename U>
    MemoryPoolAllocator& operator+(const MemoryPoolAllocator& other) {
        pool_ = other.pool_;
        return *this;
    }

    pointer allocate(size_t bytes) {
        std::array<info, bucket_count> options;
        size_t index = 0;
        for (const auto& bucket : pool_) {
            options[index].index = index;
            if (bucket.BlockSize >= bytes) {
                options[index].waste = bucket.BlockSize - bytes;
                options[index].block_count = 1;
            } else {
                const auto n = 1 + ((bytes - 1) / bucket.BlockSize);
                const auto mem_required = n * bucket.BlockSize;
                options[index].waste = mem_required - bytes;
                options[index].block_count = n;
            }
            ++index;
        }

        std::sort(options.begin(), options.end());

        for (const auto& opt : options) {
            if (auto ptr = pool_[opt.index].allocate(bytes); ptr != nullptr) {
                return static_cast<pointer>(ptr);
            }
        }
        throw std::bad_alloc{};
    }

    void deallocate(pointer ptr, size_t n) {
        for (auto& bucket : pool_) {
            if (bucket.belongs(static_cast<void*>(ptr))) {
                bucket.deallocate(ptr, n);
                return;
            }
        }
    }

private:
    std::array<bucket, bucket_count>& pool_;
};