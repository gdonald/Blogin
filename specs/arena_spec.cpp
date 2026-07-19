#include <cstddef>
#include <cstdint>
#include <string_view>

#include "arena.h"
#include "sanitizer.h"
#include "support/spec.h"

using blogin::Arena;
using spec::expect;

namespace {

struct Small {
  int value;
};

struct Aligned {
  alignas(64) double value;
};

}  // namespace

SPEC {
  spec::describe("Arena", [] {
    spec::it("hands back distinct storage for each allocation", [] {
      Arena arena;

      const Small* first = arena.create<Small>(1);
      const Small* second = arena.create<Small>(2);

      spec::aggregate_failures([&] {
        expect(first != second).to_be_true();
        expect(first->value).to_eq(1);
        expect(second->value).to_eq(2);
      });
    });

    spec::context("when an allocation does not fit the current block", [] {
      spec::it("keeps earlier allocations valid", [] {
        Arena arena(128);

        const Small* first = arena.create<Small>(7);

        for (int index = 0; index < 500; ++index) {
          arena.create<Small>(index);
        }

        expect(first->value).to_eq(7);
      });

      spec::it("grows into further blocks", [] {
        Arena arena(128);

        for (int index = 0; index < 500; ++index) {
          arena.create<Small>(index);
        }

        expect(arena.block_count()).to_be_greater_than(std::size_t{1});
      });

      spec::it("serves a single allocation larger than a whole block", [] {
        Arena arena(8);

        auto* aligned = arena.create<Aligned>();
        aligned->value = 3.5;

        expect(aligned->value).to_eq(3.5);
      });

      spec::it("aligns the first allocation in the new block", [] {
        Arena arena(8);

        const Aligned* aligned = arena.create<Aligned>();

        const auto address = reinterpret_cast<std::uintptr_t>(aligned);

        expect(address % alignof(Aligned)).to_eq(std::uintptr_t{0});
      });
    });

    spec::it("respects over-aligned types", [] {
      Arena arena(128);

      arena.create<Small>(1);
      const Aligned* aligned = arena.create<Aligned>();

      const auto address = reinterpret_cast<std::uintptr_t>(aligned);

      expect(address % alignof(Aligned)).to_eq(std::uintptr_t{0});
    });

    spec::context("reset", [] {
      spec::it("hands out storage again afterward", [] {
        Arena arena(128);

        arena.create<Small>(1);
        arena.reset();

        auto* after = arena.create<Small>(2);

        expect(after->value).to_eq(2);
      });

      spec::it("keeps a block so the next parse does not allocate one", [] {
        Arena arena(128);

        arena.create<Small>(1);
        arena.reset();

        expect(arena.block_count()).to_eq(std::size_t{1});
      });

      spec::it("releases the blocks a large parse needed", [] {
        Arena arena(128);

        for (int index = 0; index < 500; ++index) {
          arena.create<Small>(index);
        }

        const std::size_t grown = arena.block_count();
        arena.reset();

        spec::aggregate_failures([&] {
          expect(grown).to_be_greater_than(std::size_t{1});
          expect(arena.block_count()).to_eq(std::size_t{1});
        });
      });

      spec::it("reuses its storage rather than growing without bound", [] {
        Arena arena(4096);

        for (int round = 0; round < 50; ++round) {
          for (int index = 0; index < 100; ++index) {
            arena.create<Small>(index);
          }

          arena.reset();
        }

        expect(arena.block_count()).to_eq(std::size_t{1});
      });
    });

    spec::it("allocates no blocks before first use", [] {
      const Arena arena;

      expect(arena.block_count()).to_eq(std::size_t{0});
    });

    // The sanitizer sees one large heap block and nothing about the nodes
    // inside it, so the arena marks the unused bytes itself. These examples ask
    // whether a byte is poisoned rather than touching it, since touching one
    // would abort the run instead of failing the example.
    spec::context("under AddressSanitizer", [] {
      spec::before_each([] {
        if (!blogin::sanitizer_enabled) {
          spec::pending("built without AddressSanitizer");
        }
      });

      spec::it("leaves the bytes of an allocation readable", [] {
        Arena arena(128);

        const Small* first = arena.create<Small>(1);

        expect(blogin::memory_is_poisoned(first)).to_be_false();
      });

      spec::it("poisons the gap that follows an allocation", [] {
        Arena arena(128);

        const auto* first = reinterpret_cast<const std::byte*>(arena.create<Small>(1));

        expect(blogin::memory_is_poisoned(first + sizeof(Small))).to_be_true();
      });

      spec::it("keeps a later allocation out of the gap", [] {
        Arena arena(128);

        const auto* first = reinterpret_cast<const std::byte*>(arena.create<Small>(1));
        const auto* second = reinterpret_cast<const std::byte*>(arena.create<Small>(2));

        expect(second - first).to_be_greater_than(static_cast<std::ptrdiff_t>(sizeof(Small)));
      });

      spec::it("poisons the storage a block has not handed out yet", [] {
        Arena arena(128);

        const auto* first = reinterpret_cast<const std::byte*>(arena.create<Small>(1));

        expect(blogin::memory_is_poisoned(first + 64)).to_be_true();
      });

      spec::it("poisons the padding an over-aligned allocation skips over", [] {
        Arena arena(256);

        const auto* first = reinterpret_cast<const std::byte*>(arena.create<Small>(1));
        arena.create<Aligned>();

        expect(blogin::memory_is_poisoned(first + sizeof(Small))).to_be_true();
      });

      spec::it("poisons storage that a reset has taken back", [] {
        Arena arena(128);

        const Small* stale = arena.create<Small>(1);
        arena.reset();

        expect(blogin::memory_is_poisoned(stale)).to_be_true();
      });

      spec::it("hands the same storage back out readable after a reset", [] {
        Arena arena(128);

        arena.create<Small>(1);
        arena.reset();

        const Small* after = arena.create<Small>(2);

        expect(blogin::memory_is_poisoned(after)).to_be_false();
      });

      spec::it("leaves interned bytes readable to their exact length", [] {
        Arena arena(128);

        const std::string_view interned = arena.intern("hello");

        expect(blogin::memory_is_poisoned(interned.data() + interned.size())).to_be_true();
      });
    });
  });
}
