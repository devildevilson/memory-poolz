#ifndef MEMORY_POOLZ_MEMORY_POOL_MT_H
#define MEMORY_POOLZ_MEMORY_POOL_MT_H

#include <cstddef>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cassert>

namespace poolz {
  template <typename T, size_t N>
  class memory_pool_mt {
  public:
    using value_ptr = T*;
    using memory_type = char;
    using memory_ptr = memory_type*;
    using size_type = size_t;
    using atomic_value_ptr = std::atomic<value_ptr>;
    using atomic_memory_ptr = std::atomic<memory_ptr>;
    
    static_assert(sizeof(T) >= sizeof(void*));
    
    memory_pool_mt() noexcept : allocated(0), free_slot(nullptr), memory(nullptr) { memory = allocate(nullptr); }
    memory_pool_mt(const memory_pool_mt<T, N> &pool) noexcept = delete;
    // перемещать в мультитрединге НЕЖЕЛАТЕЛЬНО
    memory_pool_mt(memory_pool_mt<T, N> &&pool) noexcept {
      clear();

      memory = pool.memory.exchange(nullptr);
      free_slot = pool.free_slot.exchange(nullptr);
      allocated = pool.allocated;
      pool.allocated = 0;

      memory_ptr mem = nullptr;
      do {
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // с этой строкой работает стабильнее
        mem = pool.memory.exchange(nullptr);
      } while (mem == nullptr);
      mem = allocate(mem);
      pool.memory = mem;
    }
    
    ~memory_pool_mt() noexcept { clear(); }
    
    memory_pool_mt<T, N> & operator=(const memory_pool_mt<T, N> &pool) noexcept = delete;
    void operator=(memory_pool_mt<T, N> &&pool) noexcept {
      clear();

      memory = pool.memory.exchange(nullptr);
      free_slot = pool.free_slot.exchange(nullptr);
      allocated = pool.allocated;
      pool.allocated = 0;

      memory_ptr mem = nullptr;
      do {
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // с этой строкой работает стабильнее
        mem = pool.memory.exchange(nullptr);
      } while (mem == nullptr);
      mem = allocate(mem);
      pool.memory = mem;
    }
    
    template <typename... Args>
    value_ptr create(Args&& ...args) {
      auto ptr = find_memory();
      auto val = new (ptr) T(std::forward<Args>(args)...);
      return val;
    }
    
    void destroy(value_ptr ptr) noexcept {
      if (ptr == nullptr) return;
      
      ptr->~T();
      auto place = reinterpret_cast<char**>(ptr);
      auto ptr_fin = reinterpret_cast<char*>(ptr);
      place[0] = free_slot.exchange(ptr_fin);
    }
    
    size_type size() const noexcept {
      const size_t final_size = final_block_size();
      size_t counter = 0;
      auto tmp = memory.load();
      while (tmp != nullptr) {
        auto nextBuffer = reinterpret_cast<memory_ptr*>(tmp)[0];
        counter += final_size;
        tmp = nextBuffer;
      }
      
      return counter;
    }
    
    void clear() noexcept {
      auto tmp = memory.load();
      while (tmp != nullptr) {
        auto nextBuffer = reinterpret_cast<memory_ptr*>(tmp)[0];
        ::operator delete[] (tmp, std::align_val_t{element_alignment()});
        tmp = nextBuffer;
      }
      
      memory = nullptr;
      free_slot = nullptr;
      allocated = 0;
    }
  private:
    using atomic_free_slot_ptr = std::atomic<memory_ptr>;
    
    size_t allocated;
    atomic_free_slot_ptr free_slot;
    atomic_memory_ptr memory;
    
    constexpr static size_t align_to(const size_t size, const size_t aligment) noexcept {
      return (size + aligment - 1) / aligment * aligment;
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
    
    memory_ptr allocate(memory_ptr old_mem) noexcept {
      assert(memory == nullptr);
      
      const size_t block_size = final_block_size();
      auto mem = new (std::align_val_t{element_alignment()}) memory_type[block_size];
      assert(mem != nullptr);
      allocated = element_alignment(); // нужно ли это сделать тут?
      auto tmp = reinterpret_cast<memory_ptr*>(mem);
      tmp[0] = old_mem;
      
      return mem;
    }
    
    memory_ptr find_memory() noexcept {
      memory_ptr tmp = nullptr;
      if (!free_slot.compare_exchange_strong(tmp, nullptr)) {
        // в этом месте может быть удаление объекта
        // по идее все что мы потеряем - это создание одного лишнего блока
        if (free_slot.compare_exchange_strong(tmp, reinterpret_cast<char**>(tmp)[0])) {
          return tmp;
        }
      }

      // способ 1: кажется он самый адекватный, в этом случае мы имитируем мьютекс с помощью указателя памяти
      // слишком длинная блокировка? думаю что вряд ли это возможно
      // (другой способ утерян в пучине)
      memory_ptr mem = nullptr;
      do {
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // с этой строкой работает стабильнее
        mem = memory.exchange(nullptr);
      } while (mem == nullptr);

      if (allocated+element_size() > N) {
        mem = allocate(mem);
      }

      const size_t place = allocated;
      allocated += element_size();
      auto ptr = mem + place;

      memory = mem; // возвращаем указатель на память, чтобы ей мог возпользоваться другой поток
      
      return ptr;
    }
  };
}

#endif