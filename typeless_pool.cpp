#include "typeless_pool.h"

namespace poolz {
  constexpr size_t align_to(const size_t size, const size_t aligment) {
    return (size + aligment - 1) / aligment * aligment;
  }

  typeless_pool::typeless_pool(const size_t size, const size_t piece_size, const size_t piece_aligment) noexcept :
    m_piece_aligment(std::max(piece_aligment, alignof(char*))), 
    m_piece_size(align_to(std::max(piece_size, sizeof(char*)), m_piece_aligment)), 
    m_size(align_to(size, m_piece_aligment)), 
    m_current(0), 
    m_memory(nullptr), 
    m_free_mem(nullptr)
  {
    assert(m_size >= sizeof(void*)*2);
    assert(m_piece_size >= sizeof(void*));
  }

  typeless_pool::~typeless_pool() noexcept { clear(); }

  typeless_pool::typeless_pool(typeless_pool &&move) noexcept :
    m_size(move.m_size), m_piece_size(move.m_piece_size), m_piece_aligment(move.m_piece_aligment), m_current(move.m_current), m_memory(move.m_memory), m_free_mem(move.m_free_mem)
  {
    move.m_memory = nullptr;
    move.m_free_mem = nullptr;
  }

  typeless_pool & typeless_pool::operator=(typeless_pool &&move) noexcept {
    m_size = move.m_size;
    m_piece_size = move.m_piece_size;
    m_piece_aligment = move.m_piece_aligment;
    m_current = move.m_current;
    m_memory = move.m_memory;
    m_free_mem = move.m_free_mem;
    move.m_memory = nullptr;
    move.m_free_mem = nullptr;
    return *this;
  }

  void* typeless_pool::allocate() {
    if (m_free_mem != nullptr) {
      auto ptr = m_free_mem;
      m_free_mem = reinterpret_cast<char**>(m_free_mem)[0];
      return ptr;
    }

    if (m_memory == nullptr || (m_current + m_piece_size > m_size)) allocate_block();
    auto ptr = &m_memory[m_current];
    m_current += m_piece_size;
    return ptr;
  }

  void typeless_pool::free(void* block) noexcept {
    reinterpret_cast<void**>(block)[0] = m_free_mem;
    m_free_mem = block;
  }

  size_t typeless_pool::size() const noexcept {
    size_t summ = 0;
    auto mem = m_memory;
    while (mem != nullptr) {
      summ += m_size;
      mem = reinterpret_cast<char**>(mem)[0];
    }

    return summ;
  }

  size_t typeless_pool::block_size() const noexcept { return m_size; }
  size_t typeless_pool::piece_size() const noexcept { return m_piece_size; }
  size_t typeless_pool::piece_aligment() const noexcept { return m_piece_aligment; }

  void typeless_pool::allocate_block() {
    const size_t offset = m_piece_aligment;
    const size_t final_size = offset + m_size;
    auto new_mem = new (std::align_val_t{m_piece_aligment}) char[final_size];
    assert(new_mem != nullptr);
    reinterpret_cast<char**>(new_mem)[0] = m_memory;
    m_memory = new_mem;
    m_current = offset;
  }

  void typeless_pool::clear() noexcept {
    auto mem = m_memory;
    while (mem != nullptr) {
      auto next = reinterpret_cast<char**>(mem)[0];
      ::operator delete[] (mem, std::align_val_t{m_piece_aligment});
      mem = next;
    }
  }
}