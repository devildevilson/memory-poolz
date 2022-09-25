/*
 * auto release version of default memory pool
 * destructor is highly unoptimal
 */

#ifndef MEMORY_POOLZ_MEMORY_POOL_AUTORELEASE_H
#define MEMORY_POOLZ_MEMORY_POOL_AUTORELEASE_H

#include <cstddef>
#include <cstdint>
#include <utility>

#include <unordered_set>

namespace poolz {
  template <typename T, size_t N = 4096>
  class memory_pool_autorelease {
  public:
    using elem_ptr = T*;

    memory_pool_autorelease() noexcept : memory(nullptr), current(0), free_memory(nullptr) {}

    memory_pool_autorelease(const memory_pool_autorelease &copy) noexcept = delete;
    memory_pool_autorelease & operator=(const memory_pool_autorelease &copy) noexcept = delete;
    memory_pool_autorelease(memory_pool_autorelease &&move) noexcept : memory(move.memory), current(move.current), free_memory(move.free_memory) {
      move.memory = nullptr;
      move.current = 0;
      move.free_memory = nullptr;
    }

    memory_pool_autorelease & operator=(memory_pool_autorelease &&move) noexcept {
      clear();

      memory = move.memory;
      current = move.current;
      free_memory = move.free_memory;
      move.memory = nullptr;
      move.current = 0;
      move.free_memory = nullptr;
      return *this;
    }

    ~memory_pool_autorelease() noexcept { clear(); }

    template <typename ...Args>
    elem_ptr create(Args&& ...args) {
      auto ptr = allocate();
      elem_ptr p = new (ptr) T(std::forward<Args>(args)...);
      return p;
    }

    void destroy(elem_ptr ptr) noexcept {
      if (ptr == nullptr) return;
      ptr->~T();
      reinterpret_cast<char**>(ptr)[0] = free_memory;
      free_memory = reinterpret_cast<char*>(ptr);
    }

    char* allocate() noexcept {
      if (free_memory != nullptr) {
        auto ptr = free_memory;
        free_memory = reinterpret_cast<char**>(free_memory)[0];
        return ptr;
      }

      constexpr size_t block_size = final_block_size();
      if (memory == nullptr || current + element_size() > block_size) allocate_memory();
      auto ptr = &memory[current];
      current += element_size();
      return ptr;
    }

    void clear() noexcept {
      std::unordered_set<char*> set;
      for (char* mem = free_memory; mem != nullptr; mem = reinterpret_cast<char**>(mem)[0]) { set.insert(mem); }

      char* old_mem = memory;
      while (old_mem != nullptr) {
        char* tmp = reinterpret_cast<char**>(old_mem)[0];
        auto start = old_mem + element_alignment();
        for (size_t i = 0; i < block_elem_count(); ++i) {
          auto offset = start + i * element_size();
          if (set.find(offset) == set.end()) continue;
          auto ptr = reinterpret_cast<T*>(offset);
          ptr->~T();
        }

        ::operator delete[] (old_mem, std::align_val_t{element_alignment()});
        old_mem = tmp;
      }

      memory = nullptr;
      free_memory = nullptr;
      current = 0;
    }

    size_t blocks_allocated() const noexcept {
      size_t counter = 0;
      for (char* old_mem = memory; old_mem != nullptr; old_mem = reinterpret_cast<char**>(old_mem)[0]) { counter += 1; }
      return counter;
    }

    size_t free_elements_count() const noexcept {
      size_t counter = 0;
      for (char* old_mem = free_memory; old_mem != nullptr; old_mem = reinterpret_cast<char**>(old_mem)[0]) { counter += 1; }
      return counter;
    }

    constexpr static size_t block_elem_count() noexcept {
      return N / element_size();
    }

    constexpr static size_t block_size() noexcept {
      return final_block_size();
    }
  private:
    char* memory;
    size_t current;
    char* free_memory;

    constexpr static size_t align_to(const size_t &mem, const size_t &align) noexcept {
      return (mem + align - 1) / align * align;
    }

    constexpr static size_t element_size() noexcept {
      return align_to(std::max(sizeof(T), sizeof(char*)), element_alignment());
    }

    constexpr static size_t element_alignment() noexcept {
      return std::max(alignof(T), alignof(char*));
    }

    constexpr static size_t final_block_size() noexcept {
      const size_t elem_size = element_size();
      const size_t elem_align = element_alignment();
      const size_t count = block_elem_count();
      const size_t mem = align_to(count * elem_size, elem_align);
      return mem + elem_align;
    }

    void allocate_memory() noexcept {
      const size_t block_size = final_block_size();
      char* new_memory = new (std::align_val_t{element_alignment()}) char[block_size];
      assert(new_memory != nullptr);
      reinterpret_cast<char**>(new_memory)[0] = memory;
      memory = new_memory;
      current = element_alignment();
    }
  };
}

#endif

