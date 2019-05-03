/*
 * auto release version of default memory pool
 * destructor is highly unoptimal
 */

#ifndef MEMORY_POOL_AUTORELEASE_H
#define MEMORY_POOL_AUTORELEASE_H

#include <cstddef>
#include <cstdint>
#include <utility>

#include <unordered_set>

template<typename T, size_t blockSize = 4096>
class memory_pool_autorelease {
public:
  memory_pool_autorelease() noexcept {}
  
  memory_pool_autorelease(const memory_pool_autorelease& memoryPool) noexcept = delete;
  
  memory_pool_autorelease(memory_pool_autorelease&& memoryPool) noexcept {
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

  ~memory_pool_autorelease() noexcept {
    clear();
  }

  memory_pool_autorelease& operator=(const memory_pool_autorelease& memoryPool) = delete;

  memory_pool_autorelease& operator=(memory_pool_autorelease&& memoryPool) noexcept {
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
    std::unordered_set<void*> set;
    Slot_* currFree = freeSlots_;
    while (currFree != nullptr) {
      Slot_* prev = currFree->next;
      set.insert(currFree);
      currFree = prev;
    }
    
    const size_t countInBlock = blockSize / sizeof(T);
    
    Slot_* currDel = currentBlock_;
    while (currDel != nullptr) {
      Slot_* prev = currDel->next;
      
      Slot_* arr = reinterpret_cast<Slot_*>(reinterpret_cast<char*>(currDel) + sizeof(char*));
      arr = reinterpret_cast<Slot_*>(reinterpret_cast<char*>(arr) + padPointer(reinterpret_cast<char*>(arr), alignof(Slot_)));
      for (size_t i = 0; i < countInBlock; ++i) {
        auto itr = set.find(&arr[i]);
        if (itr != set.end()) continue;
        
        arr[i].element.~T();
      }
      
      delete [] reinterpret_cast<char*>(currDel);
      
      currDel = prev;
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

