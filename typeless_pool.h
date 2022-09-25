/*
 * this pool might be usefull when you need a vector of pools
 * this pool does not pay attention to different size of T
 * use it only for one type
 */

#ifndef MEMORY_POOLZ_TYPELESS_POOL_H
#define MEMORY_POOLZ_TYPELESS_POOL_H

#include <cstddef>
#include <cstdint>
#include <utility>

namespace poolz {
  class typeless_pool {
  public:
    typeless_pool(const size_t size, const size_t piece_size, const size_t piece_aligment) noexcept;
    ~typeless_pool() noexcept;

    typeless_pool(const typeless_pool &copy) noexcept = delete;
    typeless_pool & operator=(const typeless_pool &copy) noexcept = delete;
    typeless_pool(typeless_pool &&move) noexcept;
    typeless_pool & operator=(typeless_pool &&move) noexcept;

    void* allocate();
    void free(void* block) noexcept;
    void clear() noexcept;

    template <typename T, typename... Args>
    T* create(Args&&... args) {
      assert(sizeof(T) <= m_piece_size && "Type size is more than this pool piece size");
      assert(alignof(T) <= m_piece_aligment && "Type aligment is more than this pool piece aligment");
      auto ptr = allocate();
      return new (ptr) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy(T* obj) noexcept {
      assert(sizeof(T) <= m_piece_size && "Type size is more than this pool piece size");
      assert(alignof(T) <= m_piece_aligment && "Type aligment is more than this pool piece aligment");
      if (obj == nullptr) return;
      obj->~T();
      free(obj);
    }

    size_t size() const noexcept;
    size_t block_size() const noexcept;
    size_t piece_size() const noexcept;
    size_t piece_aligment() const noexcept;
  private:
    void allocate_block();

    size_t m_piece_aligment;
    size_t m_piece_size;
    size_t m_size;
    size_t m_current;
    char* m_memory;
    void* m_free_mem;
  };
}

#endif
