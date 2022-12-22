/////////////////////////////////////////////////////////////////////////////////////
//
// Created by Ricardo Romero on 08/11/22.
// Copyright (c) 2022 Ricardo Romero.  All rights reserved.
//

#include "../mempool/include/fixpool.hpp"
#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <iostream>
#include <random>

TEST_CASE("Initialize Memory pool", "[initialize]")
{
    using namespace Catch::Matchers;
    constexpr auto intSize = sizeof(int);

    SECTION("Throws chunk fitting ")
    {
        CHECK_THROWS_WITH((pool::fixed_memory_pool<int, intSize * 8>(5)), ContainsSubstring("must fit"));
    }
    SECTION("Throws sizeof(chunk) < sizeof(void *)")
    {
        CHECK_THROWS_WITH((pool::fixed_memory_pool<int, intSize * 8>(2)), ContainsSubstring("at least"));
    }
    SECTION("Throws block alignment")
    {
        CHECK_THROWS_WITH((pool::fixed_memory_pool<int, 4096 + 128>(8)), ContainsSubstring("block"));
    }
    SECTION("Block Alignment")
    {
        pool::fixed_memory_pool<int, 4096> pool(8);
#if defined(__APPLE__)
        const auto alignment = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
        REQUIRE(alignment == pool.get_block_alignment());

        if (alignment == 0)
            REQUIRE(pool.was_block_alignment_defaulted());
        else
            REQUIRE(!pool.was_block_alignment_defaulted());
    }
}

TEST_CASE("Memory free inside block")
{
    using namespace Catch::Matchers;
    pool::fixed_memory_pool<int, 4096> pool(8);
    int *i0 = new int;
    CHECK_THROWS_WITH(pool.release(i0), ContainsSubstring("does not belong"));
    delete i0;
}

TEST_CASE("Memory data integrity and release")
{
    pool::fixed_memory_pool<int, 4096> pool(8);
    int *i0 = pool.alloc();

    REQUIRE(i0 != nullptr);
    *i0 = 0x6989aabb;

    int *i1 = i0;
    CHECK(*i1 == 0x6989aabb);

    CHECK_NOTHROW(pool.release(i0));
    CHECK(i0 == nullptr);
}

TEST_CASE("Arguments passed to object via alloc")
{
    struct args
    {
        explicit args(uint64_t i0, uint64_t i1, uint64_t i2, std::string s) :
            _i0 { i0 },
            _i1 { i1 },
            _i2 { i2 },
            _s { std::move(s) }
        {
        }

        uint64_t _i0;
        uint64_t _i1;
        uint64_t _i2;

        std::string _s;
    };

    pool::fixed_memory_pool<args, 4096> pool(8);
    args *a0 = pool.alloc(0x45ull, 0x32ull, 0x10ull, "test string");

    REQUIRE(a0->_i0 == 0x45ull);
    REQUIRE(a0->_i1 == 0x32ull);
    REQUIRE(a0->_i2 == 0x10ull);
    REQUIRE(a0->_s == "test string");

    args *a1 = pool.alloc(0x4454ull, 0x31232ull, 0x123320ull, "test second string");

    CHECK(a1 != a0);

    REQUIRE(a1->_i0 == 0x4454ull);
    REQUIRE(a1->_i1 == 0x31232ull);
    REQUIRE(a1->_i2 == 0x123320ull);
    REQUIRE(a1->_s == "test second string");

    CHECK_NOTHROW(pool.release(a0));
    CHECK_NOTHROW(pool.release(a1));
    CHECK(a0 == nullptr);
    CHECK(a1 == nullptr);
}

TEST_CASE("Check block count and value integrity across multiple allocations and blocks")
{
    pool::fixed_memory_pool<uint64_t, 4096> pool(8);

    std::vector<std::pair<uint64_t *, uint64_t>> addressMap;
    for (uint64_t a = 0; a < 2048; ++a)
    {
        uint64_t *ptr = pool.alloc(a);
        CHECK(*ptr == a);
        addressMap.emplace_back(ptr, a);

        for (const auto &[p, v] : addressMap)
        {
            // CHECK THAT VALUES HAVE NOT BEEN OVERWRITTEN
            REQUIRE(*p == v);
        }
    }
    REQUIRE(pool.block_count() == 4);

    // Release the first 512 values
    for (int i = 0; i < 512; ++i)
    {
        auto iter     = addressMap.begin();
        auto *pointer = iter->first;
        CHECK_NOTHROW(pool.release(pointer));
        addressMap.erase(addressMap.begin());
    }
    REQUIRE(pool.block_count() == 3);

    for (const auto &[p, v] : addressMap)
    {
        // CHECK THAT VALUES HAVE NOT BEEN OVERWRITTEN
        REQUIRE(*p == v);
    }
}

TEST_CASE("Information integrity")
{
    pool::fixed_memory_pool<uint64_t, 4096> pool(8);

    auto avai_space = 4096ull;
    auto used_space = 0ull;

    auto avai_chunks = 512ull;
    auto used_chunks = 0ull;

    for (uint64_t a = 0; a < 512; ++a)
    {
        uint64_t *ptr = pool.alloc(a);
        CHECK(*ptr == a);

        avai_space -= 8;
        used_space += 8;

        --avai_chunks;
        ++used_chunks;

        CHECK(pool.available_chunks_in_block(ptr) == avai_chunks);
        CHECK(pool.used_chunks_in_block(ptr) == used_chunks);
        CHECK(pool.available_space_in_block(ptr) == avai_space);
        CHECK(pool.used_space_in_block(ptr) == used_space);
    }
    REQUIRE(pool.block_count() == 1);
}

TEST_CASE("Free list integrity")
{
    constexpr size_t chunkSize = 8;
    // These parameters make 8 chunks
    pool::fixed_memory_pool<uint8_t, 4096 * 5> pool(chunkSize);

    uint8_t *beg = pool.block_address(nullptr);

    constexpr size_t elements = 4096 * 5 / chunkSize;
    std::array<uint8_t *, elements + 1> addresses {};

    for (size_t i = 0; i < elements; ++i)
        addresses[i] = beg + (chunkSize * i);
    addresses[elements] = nullptr;

    auto freeList = pool.dump_free_list(addresses[0]);

    SECTION("Empty list with no previous allocations made")
    {
        // These will check the free list integrity of an empty list without any allocations previously made
        // If allocations were made, there are no guarantees this check will pass the test
        auto index = 0ull;
        for (const auto &[free, next] : freeList)
        {
            REQUIRE(free == addresses[index]);
            REQUIRE(next == addresses[++index]);
        }
    }

    SECTION("Sequential allocation")
    {
        // Allocate
        for (size_t i = 0; i < elements; ++i)
        {
            // The allocator allocates sequentially not randomly so these check is valid
            auto *p = pool.alloc();
            REQUIRE(p == addresses[i]);
        }

        freeList = pool.dump_free_list(addresses[0]);
        REQUIRE(freeList.empty());
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<size_t> index(0, elements - 1);

    SECTION("Free list integrity. one-element released")
    {
        for (size_t i = 0; i < elements; ++i)
        {
            // The allocator allocates sequentially not randomly so these check is valid
            [[maybe_unused]] auto *p = pool.alloc();
        }

        for (int i = 0; i < 1024; ++i)
        {
            const auto delIndex = index(mt);

            // On other circumstances, this should be considered unsafe because the original pointer will still have the address.
            // however, once released the value at the address is going to be set to the freelist, making the original pointer unsafe to use

            auto *prevRelease  = addresses[delIndex];
            auto *checkAddress = prevRelease;
            pool.release(prevRelease);

            freeList = pool.dump_free_list(addresses[0]);

            REQUIRE(freeList.size() == 1);
            REQUIRE(prevRelease == nullptr);
            REQUIRE(freeList[0].first == checkAddress);
            REQUIRE(freeList[0].second == nullptr);

            // Reallocate so the pool can have only one chunk available once release is called
            REQUIRE(checkAddress == pool.alloc());
        }
    }

    SECTION("Integrity of the freelist with multiple releases")
    {

        for (int i = 0; i < 3; ++i)
        {
            // Reallocate all
            for (size_t n = 0; n < elements; ++n)
                [[maybe_unused]] auto *p = pool.alloc();

            // Generate a random path of indexes in which we include the indexes from 0 to 7 without repeating ourselves
            std::array<size_t, elements> path {};
            for (size_t k = 0; k < elements; ++k)
                path[k] = k;
            std::shuffle(path.begin(), path.end(), mt);

            size_t at = 1;
            for (const auto &indexPath : path)
            {
                auto *freePtr = addresses[indexPath];
                pool.release(freePtr);
                freeList = pool.dump_free_list(addresses[0]);

                REQUIRE(freeList.size() == at);

                for (size_t k = 0; k < at; ++k)
                {
                    REQUIRE(freeList[k].first == addresses[path[at - 1 - k]]);
                    if (k == at - 1)
                        REQUIRE(freeList[k].second == nullptr);
                    else
                        REQUIRE(freeList[k].second == addresses[path[at - 2 - k]]);
                }
                ++at;
            }
        }
    }
}

TEST_CASE("Multiple pools")
{
    pool::fixed_memory_pool<size_t, 4096> pool(1024);

    size_t *_1_pool0 = pool.alloc<size_t>(4);
    size_t *_2_pool0 = pool.alloc<size_t>(44);
    size_t *_3_pool0 = pool.alloc<size_t>(434);
    size_t *_4_pool0 = pool.alloc<size_t>(453764);
    size_t *_1_pool1 = pool.alloc<size_t>(4537664);
    size_t *_2_pool1 = pool.alloc<size_t>(4537661224);
    size_t *_3_pool1 = pool.alloc<size_t>(453766124);
    size_t *_4_pool1 = pool.alloc<size_t>(45376614);
    size_t *_1_pool2 = pool.alloc<size_t>(453764);
    size_t *_2_pool2 = pool.alloc<size_t>(4534);
    size_t *_3_pool2 = pool.alloc<size_t>(454);
    size_t *_4_pool2 = pool.alloc<size_t>(4);


    REQUIRE( pool.block_count() == 3 );

    CHECK( pool.available_chunks_in_block(_1_pool0) == 0);
    CHECK_NOTHROW(pool.release(_2_pool0));
    CHECK(pool.available_chunks_in_block(_3_pool0) == 1);
    CHECK_NOTHROW(pool.release(_4_pool0));
    CHECK(pool.available_chunks_in_block(_3_pool0) == 2);

    CHECK( pool.available_chunks_in_block(_1_pool1) == 0);
    CHECK_NOTHROW(pool.release(_2_pool1));
    CHECK(pool.available_chunks_in_block(_3_pool1) == 1);
    CHECK_NOTHROW(pool.release(_4_pool1));
    CHECK(pool.available_chunks_in_block(_3_pool1) == 2);


    CHECK( pool.available_chunks_in_block(_1_pool2) == 0);
    CHECK_NOTHROW(pool.release(_2_pool2));
    CHECK(pool.available_chunks_in_block(_3_pool2) == 1);
    CHECK_NOTHROW(pool.release(_4_pool2));
    CHECK(pool.available_chunks_in_block(_3_pool2) == 2);

    CHECK_NOTHROW(pool.release(_1_pool2));
    CHECK_NOTHROW(pool.release(_3_pool2));
    REQUIRE( pool.block_count() == 2 );

    CHECK_NOTHROW(pool.release(_1_pool1));
    CHECK_NOTHROW(pool.release(_3_pool1));
    REQUIRE( pool.block_count() == 1 );

    CHECK_NOTHROW(pool.release(_1_pool0));
    CHECK_NOTHROW(pool.release(_3_pool0));
    REQUIRE( pool.block_count() == 1 );
    REQUIRE( pool.available_chunks_in_block(reinterpret_cast<size_t*>(pool.block_address(nullptr))) == 4 );
}

TEST_CASE("Benchmarking")
{
    constexpr size_t chunkSize = 8;
    // These parameters make 8 chunks
    pool::fixed_memory_pool<size_t, 4096 * 20> pool(chunkSize);

    size_t n = 0;

    BENCHMARK_ADVANCED("Object creation/destruction in pool")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<size_t *> poolObject;
        poolObject.reserve(300000);

        size_t *ptr;

        meter.measure([&] {
            for (int i = 0; i < 10'000; ++i)
            {
                ptr = pool.alloc(++n);
                poolObject.push_back(ptr);
            }
            for (auto &p : poolObject)
            {
                pool.release(p);
            }
        });
    };

    BENCHMARK_ADVANCED("Object creation/destruction using new/delete")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<size_t *> systemObject;
        systemObject.reserve(300000);
        size_t *ptr;

        meter.measure([&] {
            for (int i = 0; i < 10'000; ++i)
            {
                ptr = new size_t(++n);
                systemObject.push_back(ptr);
            }
            for (auto &p : systemObject)
            {
                delete p;
                p = nullptr;
            }

        });
    };
}

