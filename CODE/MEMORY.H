#ifndef MEMORY_H
#define MEMORY_H

#include <cstdlib>

class MemoryClass
{
public:
    MemoryClass() = default;

    explicit operator bool() const { return true; }

    void* Alloc(std::size_t size) const
    {
        return std::malloc(size);
    }

    void Free(void const* ptr) const
    {
        std::free(const_cast<void*>(ptr));
    }
};

extern MemoryClass Mem;

#endif
