#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>
#include <cstdint>
#include <utility>

template<typename T, size_t blockSize = 4096>
class memory_pool {
public:
  memory_pool() noexcept {}
  
  memory_pool(const memory_pool& memoryPool) noexcept = delete;
  
  memory_pool(memory_pool&& memoryPool) noexcept {
    currentBlock_ = memoryPool.currentBlock_;
    currentSlot_  = memoryPool.currentSlot_;
    lastSlot_     = memoryPool.lastSlot_;
    freeSlots_    = memoryPool.freeSlots;
    
    if (this == &memoryPool) return;
    
    memoryPool.currentBlock_ = nullptr;
    memoryPool.currentSlot_  = nullptr;
    memoryPool.lastSlot_     = nullptr;
    memoryPool.freeSlots_    = nullptr;
  }

  ~memory_pool() noexcept {
    clear();
  }

  memory_pool& operator=(const memory_pool& memoryPool) = delete;

  memory_pool& operator=(memory_pool&& memoryPool) noexcept {
    if (this != &memoryPool) {
      std::swap(currentBlock_, memoryPool.currentBlock_);
      currentSlot_ = memoryPool.currentSlot_;
      lastSlot_ = memoryPool.lastSlot_;
      freeSlots_ = memoryPool.freeSlots;
    }

    return *this;
  }

  size_t max_size() const noexcept {
    size_t maxBlocks = -1 / blockSize;
    return (blockSize - sizeof(char*)) / sizeof(Slot_) * maxBlocks;
  }

  template <class... Args> T* create(Args&&... args)  {
    T* result = nullptr;
    
    if (freeSlots_ != nullptr) {
      T* result = reinterpret_cast<T*>(freeSlots_);
      freeSlots_ = freeSlots_->next;
    } else {
      if (currentSlot_ >= lastSlot_) allocateBlock();

      result = reinterpret_cast<T*>(currentSlot_++);
    }
    
    new (result) U (std::forward<Args>(args)...);
    return result;
  }

  void destroy(T* p) {
    if (p == nullptr) return;
    
    p->~T();
    reinterpret_cast<Slot_*>(p)->next = freeSlots_;
    freeSlots_ = reinterpret_cast<Slot_*>(p);
  }

  void clear() {
    Slot_* curr = currentBlock_;
    while (curr != nullptr) {
      Slot_* prev = curr->next;
      delete [] reinterpret_cast<char*>(curr);
      curr = prev;
    }
  }
private:
  union Slot_ {
    T element;
    Slot_* next;
  };

  Slot_* currentBlock_ = nullptr;
  Slot_* currentSlot_  = nullptr;
  Slot_* lastSlot_     = nullptr;
  Slot_* freeSlots_    = nullptr;

  size_t padPointer(char* p, const size_t &align) const noexcept {
    uintptr_t result = reinterpret_cast<uintptr_t>(p);
    return ((align - result) % align);
  }

  void allocateBlock() {
    // allocate new memory block with size blockSize+sizeof(pointer)
    size_t size = blockSize + sizeof(Slot_*);
    char* newBlock = new char[size];
    reinterpret_cast<Slot_*>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<Slot_*>(newBlock);

    // block align
    char* body = newBlock + sizeof(Slot_*); // first sizeof(Slot_*) bytes pointer to previous block
    size_t bodyPadding = padPointer(body, alignof(Slot_));
    currentSlot_ = reinterpret_cast<Slot_*>(body + bodyPadding);
    lastSlot_ = reinterpret_cast<Slot_*>(newBlock + size - sizeof(Slot_) + 1);
  }
};

#endif
