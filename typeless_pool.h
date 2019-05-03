/*
 * this pool might be usefull when you need a vector of pools
 * this pool does not pay respect to different size of T
 * use it only for one type
 */

#ifndef TYPELESS_POOL_H
#define TYPELESS_POOL_H

#include <cstddef>
#include <cstdint>
#include <utility>

class typeless_pool {
public:
  typeless_pool(const size_t &blockSize) : blockSize(blockSize), currentSize(0), memory(nullptr), freeSlots(nullptr) {
    allocateBlock();
  }
  
  typeless_pool(TypelessPool &&another) : blockSize(another.blockSize), currentSize(another.currentSize), memory(another.memory), freeSlots(another.freeSlots) {
    another.currentSize = 0;
    another.memory = 0;
    another.freeSlots = nullptr;
  }
  
  ~typeless_pool() {
    char* tmp = memory;
    while (tmp != nullptr) {
      char** ptr = reinterpret_cast<char**>(tmp);
      char* nextBuffer = ptr[0];
      
      delete [] tmp;
      tmp = nextBuffer;
    }
  }
  
  template<typename T, typename... Args>
  T* create(Args&&... args) {
    T* ptr;
    
    if (freeSlots != nullptr) {
      ptr = reinterpret_cast<T*>(freeSlots);
      freeSlots = freeSlots->next;
    } else {
      if (currentSize + std::max(sizeof(T), sizeof(_Slot)) > blockSize) allocateBlock();
    
      ptr = reinterpret_cast<T*>(memory+sizeof(char*)+currentSize);
      currentSize += std::max(sizeof(T), sizeof(_Slot));
    }
    
    new (ptr) T(std::forward<Args>(args)...);
    
    return ptr;
  }
  
  template<typename T>
  void destroy(T* ptr) {
    if (ptr == nullptr) return;
    
    ptr->~T();
    
    reinterpret_cast<_Slot*>(ptr)->next = freeSlots;
    freeSlots = reinterpret_cast<_Slot*>(ptr);
  }
  
  TypelessPool & operator=(const TypelessPool &pool) = delete;
  TypelessPool & operator=(TypelessPool &&pool) = delete;
private:
  union _Slot {
    //char obj[typeSize];
    //char* obj;
    _Slot* next;
  };
  
  const size_t blockSize;
  size_t currentSize;
  char* memory;
  _Slot* freeSlots;
  
  void allocateBlock() {
    const size_t newBufferSize = blockSize + sizeof(char*);
    char* newBuffer = new char[newBufferSize];
    
    currentSize = 0;
    
    char** tmp = reinterpret_cast<char**>(newBuffer);
    tmp[0] = memory;
    memory = newBuffer;
  }
};

#endif
