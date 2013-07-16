#ifndef WHISPER__SLAB_HPP
#define WHISPER__SLAB_HPP

#include "common.hpp"
#include "debug.hpp"

#include <limits>

namespace Whisper {


//
// Slabs
// =====
//
// Slabs are used to allocate garbage-collected heap objects.
//
// A slab's layout is as follows:
//
//
//  /-> +-----------------------+   <--- Top - aligned to 1k
//  |   | Forward/Next          |   }
//  |   |                       |   }-- Header (multiple of 1k)
//  |   |                       |   }
//  |   +-----------------------+
//  \---|-- |     |   Traced    |   }
//      |---/     |   Objects   |   }
//      |         v             |   }
//      |~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~|   }
//      |                       |   }
//      |    Free Space         |   }-- Data space (multiple of 1k cards)
//      |                       |   }
//      |                       |   }
//      |~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~|   }
//      |         ^  NonTraced  |   }
//      |         |  Objects    |   }
//      |         |             |   }
//      +-----------------------+
//
// Slabs come in two basic forms: standard slab, which are of a fixed
// size and allocate multiple "small" objects, and singleton slabs,
// which are of variable sized, and hold a single "large" object.
//
// Singleton slabs are not necessarily larger than standard slabs.
// They are simply used for objects which are larger than a certain
// threshold size.  This reduces memory that would be wasted by
// allocating large objects within standard slabs.
//
// There's a maximum possible size for standard slabs implied by
// the size of the CardNo field in an object's header.  That field
// needs to be able to describe the card number it's allocated on.
// Since objects within standard chunks can exist on any card, the
// maximum card within such a chunk is limited to maximum CardNo
// describable by the object.
//
// Singleton chunks do not suffer this problem as only a single object
// is allocated in them, and thus the start of the object allocated on it
// will be in the first card.
//
// NOTE: The first 8 bytes of the allocation area are a pointer to the
// slab structure.
//

class Slab
{
  public:
    static constexpr uint32_t AllocAlign = sizeof(void *);
    static constexpr uint32_t CardSize = 1024;
    static constexpr uint32_t AlienRefSpaceSize = 512;

    static uint32_t PageSize();

    static uint32_t StandardSlabCards();
    static uint32_t StandardSlabHeaderCards();
    static uint32_t StandardSlabDataCards();
    static uint32_t StandardSlabMaxObjectSize();

    // Calculate the number of data cards required to store an object
    // of a particular size.
    static uint32_t NumDataCardsForObjectSize(uint32_t objectSize);

    // Calculate the number of header cards required in a chunk with the
    // given number of data cards.
    static uint32_t NumHeaderCardsForDataCards(uint32_t dataCards);

    // Allocate/destroy slabs.
    static Slab *AllocateStandard();
    static Slab *AllocateSingleton(uint32_t objectSize, bool needsGC);
    static void Destroy(Slab *slab);

  private:
    // Pointer to the actual system-allocated memory region containing
    // the slab.
    void *region_;
    uint32_t regionSize_;

    // Next/previous slab pointers.
    Slab *next_ = nullptr;
    Slab *previous_ = nullptr;

    // Pointer to top and bottom of allocation space
    uint8_t *allocTop_ = nullptr;
    uint8_t *allocBottom_ = nullptr;

    // Pointer to head and tail allocation pointers.
    uint8_t *headAlloc_ = nullptr;
    uint8_t *tailAlloc_ = nullptr;

    // Number of header cards.
    uint32_t headerCards_;

    // Number of data cards.
    uint32_t dataCards_;

    // Chunk flags.
    bool needsGC_ = false;

    Slab(void *region, uint32_t regionSize,
         uint32_t headerCards, uint32_t dataCards);

    ~Slab() {}

  public:
    inline Slab *next() const {
        return next_;
    }
    inline Slab *previous() const {
        return previous_;
    }

    inline uint32_t headerCards() const {
        return headerCards_;
    }

    inline uint32_t dataCards() const {
        return dataCards_;
    }

    // Allocate memory from Top
    uint8_t *allocateTop(uint32_t amount) {
        WH_ASSERT(IsIntAligned(amount, AllocAlign));

        uint8_t *oldTop = allocTop_;
        uint8_t *newTop = oldTop + amount;
        if (newTop > allocBottom_)
            return nullptr;

        allocTop_ = newTop;
        return oldTop;
    }

    // Allocate memory from Bottom
    uint8_t *allocateBottom(uint32_t amount) {
        WH_ASSERT(IsIntAligned(amount, AllocAlign));

        uint8_t *newBot = allocBottom_ - amount;
        if (newBot < allocTop_)
            return nullptr;

        allocBottom_ = newBot;
        return newBot;
    }
};


} // namespace Whisper

#endif // WHISPER__SLAB_HPP
