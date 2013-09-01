#ifndef WHISPER__VM__STACK_FRAME_HPP
#define WHISPER__VM__STACK_FRAME_HPP

#include "common.hpp"
#include "debug.hpp"
#include "value.hpp"
#include "vm/heap_thing.hpp"
#include "vm/script.hpp"

namespace Whisper {
namespace VM {


//
// A StackFrame represents a traced, heap-allocated stack frame object
// used by the C++ interpreter.
//
//      +-----------------------+
//      | Header                |
//      +-----------------------+
//      | CallerFrame           |
//      +-----------------------+
//      | Script                |
//      +-----------------------+
//      | Info                  |
//      +-----------------------+
//      | StackVal              |
//      +-----------------------+
//      | ...                   |
//      +-----------------------+
//      | StackVal              |
//      +-----------------------+
//      | ArgVal                |
//      +-----------------------+
//      | ...                   |
//      +-----------------------+
//      | ArgVal                |
//      +-----------------------+
//
// CallerFrame - points to the caller StackFrame.  For the initial stack
//  frame, this is null.
//
// Script - the Script object that's executing in this frame.
//
// Info - a Magic bitfield storing information about the frame.
//      * The maximum stack depth (20 bits).
//      * The current stack depth (20 bits).
//
//      The number of actual arguments can be computed from the object
//      size and the maximum stack depth.
//
struct StackFrame : public HeapThing,
                    public TypedHeapThing<HeapType::StackFrame>
{
  public:
    static constexpr unsigned CurStackDepthBits = 20;
    static constexpr unsigned CurStackDepthShift = 0;
    static constexpr uint64_t CurStackDepthMaskLow =
        (UInt64(1) << CurStackDepthBits) - 1;

    static constexpr unsigned MaxStackDepthBits = 20;
    static constexpr unsigned MaxStackDepthShift = 20;
    static constexpr uint64_t MaxStackDepthMaskLow =
        (UInt64(1) << MaxStackDepthBits) - 1;

    // Three fixed slots: callerFrame, callee, info
    static constexpr uint32_t FixedSlots = 3;

    struct Config
    {
        uint32_t maxStackDepth;
    };

  private:
    // Pointer to bytecode for the script.
    NullableHeapThingValue<StackFrame> callerFrame_;
    HeapThingValue<Script> script_;
    Value info_;

    void initialize(const Config &config);

    void incrCurStackDepth();
    void decrCurStackDepth(uint32_t count);

  public:
    StackFrame(Script *script, const Config &config);
    StackFrame(StackFrame *callerFrame, Script *script, const Config &config);

    bool hasCallerFrame() const;
    StackFrame *callerFrame() const;

    Script *script() const;

    uint32_t maxStackDepth() const;
    uint32_t curStackDepth() const;

    uint32_t numActualArgs() const;
    const Value &actualArg(uint32_t idx) const;

    void pushValue(const Value &val);
    const Value &peekValue(uint32_t offset = 0) const;
    void popValue(uint32_t count = 1);
};


} // namespace VM
} // namespace Whisper

#endif // WHISPER__VM__STACK_FRAME_HPP