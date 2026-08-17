#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "sanitizer.h"

namespace blogin {

// Bump allocator for parse trees.
//
// Nodes are carved out of large blocks and released together when the arena
// dies. Nothing allocated here is destroyed individually, so every type placed
// in an arena must be trivially destructible. Anything owning heap memory of
// its own belongs outside.
//
// An arena is not thread safe. Each thread owns one.
//
// Under AddressSanitizer each allocation is unpoisoned to its exact size with a
// gap between neighbours, so an overrun into the next node reports. Without it
// the layout is a plain bump.
class Arena {
public:
  static constexpr std::size_t default_block_size = 64UZ * 1024;

  explicit Arena(std::size_t block_size = default_block_size)
    : block_size_(block_size) {}

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&&) noexcept = default;

  // Written out, not defaulted, because the blocks being replaced carry
  // poison this arena applied, and it has to come off before they are freed.
  Arena& operator=(Arena&& other) noexcept {
    if (this != &other) {
      unpoison_blocks();

      blocks_ = std::move(other.blocks_);
      used_ = other.used_;
      allocated_ = other.allocated_;
      block_size_ = other.block_size_;
    }

    return *this;
  }

  ~Arena() { unpoison_blocks(); }

  template <typename Node, typename... Args>
  Node* create(Args&&... args) {
    static_assert(std::is_trivially_destructible_v<Node>,
                  "arena types are never destroyed individually");

    void* storage = allocate(sizeof(Node), alignof(Node));

    return std::construct_at(static_cast<Node*>(storage), std::forward<Args>(args)...);
  }

  void* allocate(std::size_t size, std::size_t alignment) {
    if (blocks_.empty() || !fits(size, alignment)) {
      grow(size, alignment);
    }

    const Block& block = blocks_.back();
    const std::size_t padding = padding_for(alignment);

    std::byte* result = block.data.get() + used_ + padding;
    used_ += padding + size;

    unpoison_memory(result, size);

    // The next allocation starts past a poisoned gap, so a write running off
    // the end of this one lands in the gap, not in the next node. The
    // gap is zero bytes wide without the sanitizer.
    used_ = std::min(used_ + redzone, block.size);

    return result;
  }

  // Copies bytes into the arena and hands back a view of the copy.
  //
  // Parsing mostly borrows from the source buffer, but not always: an entity
  // reference or a backslash escape produces bytes the source does not contain.
  // Those live here, with the same lifetime as the tree that refers to them.
  std::string_view intern(std::string_view text) {
    if (text.empty()) {
      return {};
    }

    char* copy = static_cast<char*>(allocate(text.size(), alignof(char)));
    std::copy(text.begin(), text.end(), copy);

    return std::string_view(copy, text.size());
  }

  // Drops everything allocated so far and keeps the blocks for reuse. Parsing a
  // thousand posts on one thread then costs one set of block allocations rather
  // than a thousand.
  //
  // Every pointer and every string_view handed out before a reset dangles
  // afterward. Reset only when the whole tree is finished with.
  void reset() {
    if (blocks_.size() > 1) {
      Block largest = std::move(*std::max_element(
        blocks_.begin(), blocks_.end(),
        [](const Block& left, const Block& right) { return left.size < right.size; }));

      unpoison_blocks();

      blocks_.clear();
      blocks_.push_back(std::move(largest));
    }

    used_ = 0;

    // The retained block goes back to poisoned, so a pointer from before the
    // reset reports, instead of reading storage the next parse has taken over.
    if (!blocks_.empty()) {
      poison_memory(blocks_.back().data.get(), blocks_.back().size);
    }
  }

  std::size_t bytes_allocated() const { return allocated_; }

  std::size_t block_count() const { return blocks_.size(); }

private:
  struct Block {
    std::unique_ptr<std::byte[]> data;
    std::size_t size;
  };

  // Bytes left poisoned between neighbouring allocations. Wide enough to catch
  // the overrun of a pointer or a couple of fields, the size an off-by-one
  // in a parser produces.
  static constexpr std::size_t redzone = sanitizer_enabled ? 16UZ : 0UZ;

  std::size_t padding_for(std::size_t alignment) const {
    const auto address = reinterpret_cast<std::uintptr_t>(blocks_.back().data.get() + used_);
    const auto misaligned = static_cast<std::size_t>(address % alignment);

    return misaligned == 0 ? 0 : alignment - misaligned;
  }

  bool fits(std::size_t size, std::size_t alignment) const {
    return used_ + padding_for(alignment) + size <= blocks_.back().size;
  }

  void grow(std::size_t size, std::size_t alignment) {
    // A fresh block only carries the alignment operator new[] guarantees, which
    // is less than an over-aligned type asks for. Room for the padding is part
    // of what the block has to hold.
    const std::size_t needed = size + alignment;
    const std::size_t size_to_allocate = needed > block_size_ ? needed : block_size_;

    blocks_.push_back(Block{
      std::make_unique_for_overwrite<std::byte[]>(size_to_allocate),
      size_to_allocate,
    });

    used_ = 0;
    allocated_ += size_to_allocate;

    poison_memory(blocks_.back().data.get(), size_to_allocate);
  }

  // Poison belongs to this arena, so it comes off before the allocator gets the
  // memory back. A moved-from block has no storage and is skipped.
  void unpoison_blocks() {
    for (const Block& block : blocks_) {
      if (block.data) {
        unpoison_memory(block.data.get(), block.size);
      }
    }
  }

  std::vector<Block> blocks_;
  std::size_t used_ = 0;
  std::size_t allocated_ = 0;
  std::size_t block_size_;
};

}  // namespace blogin
