## A simple fixed-sized memory pool in C++

This is a simple C++20 header-only fixed-sized memory pool with a single linked list for the freelist.

It consists on two classes contained in the *__pool__* namespace.

```c++
namespace pool{
    template <typename T, size_t blockSize>
    struct fixed_memory_pool;
            
    template <typename T, size_t blockSize>
    class fixed_allocator;
}
```

The struct *fixed_memory_pool* contains all functionality of the memory pool while the class *fixed_allocator
wraps the functionality in two methods

### class fixed_allocator;

Allocation is done with

```c++
template <typename... Args>
auto allocate(Args &&...args) -> T *;
```

And to the release the memory.

```c++
void deallocate(T *p);
```

Special considerations about *deallocate*.
Don't deallocate twice a chunk since it will corrupt the freelist
If the pointer to release is not in a pool block it will throw an std::exception

### struct fixed_memory_pool

fixed_memory_pool, contains all basic functionality.

The constructor:

```c++
template <typename T, size_t blockSize>
struct fixed_memory_pool
{
    explicit fixed_memory_pool(size_t chunk)
}
```

By default, aligns the memory block to the system page size.

Will throw an exception if:

- chunk is less than sizeof(void*). That is because the freelist stores addresses.
- if *blockSize* **mod** *chunk* is not zero.
- if the block size is less than the system memory page size. On must systems, this size is 4096 KB

On must cases *class fixed_allocator;* is enough. However, fixed_memory_pool contains

```c++
[[nodiscard]] auto dump_free_list(T *p) -> std::vector<std::pair<T *, T *>>;
```

Which can be useful to see the freelist under the hood and several other methods to help in the debugging process.

## Usage

As simple as:

```c++
#include "fixpool.hpp"
#include <iostream>
int main()
{
    pool::fixed_allocator<int64_t, 4096> pool(8);
    
    int64_t *p = pool.allocate(3);
    
    std::cout << "Value:" << *p << "\n";
    
    pool.deallocate(p);
    
    return 0;
}

// output:
// Value: 3
```

For more complex data structures

```c++
#include "fixpool.hpp"
#include <iostream>
#include <string>

struct mystruct
{
    mystruct(int a, int b, int c, std::string d) :
        _a{a},
        _b{b},
        _c{c},
        _d{std::move(d)}
    { }
    int _a;
    int _b;
    int _c;
    std::string _d;
};

int main()
{
    pool::fixed_allocator<mystruct, 4096> pool(sizeof(mystruct));
    
    auto *p = pool.allocate(3 ,4, 5, "string");
    
    std::cout << "Value a: " << p->a
        << "Value b: " << p->b
        << "Value c: " << p->c
        << "Value d: " << p->d
        << "\n";

    pool.deallocate(p);
    
    return 0;
}

// Output:
// Value a: 3
// Value b: 4
// Value c: 5
// Value d: string
```

## Building the tests
The tests use **catch2** through **vcpkg** as the dependencies manager.

If you have vcpkg:
```shell
git clone https://github.com/richardrom/mempool.git
cd mempool/scripts
python build.py -v {VCPKG_INSTALL_PATH} -c {COMPILER_BINARY_PATH}
```
This will compile and run the tests for the Release and Debug building types.

- {VCPKG_INSTALL_PATH} is the path of vcpkg the script will add */vcpkg/scripts/buildsystems/vcpkg.cmake* to find the toolchain file.
- {COMPILER_BINARY_PATH} is used to find the desire compiler