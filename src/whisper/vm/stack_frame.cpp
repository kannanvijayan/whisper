#include "value_inlines.hpp"
#include "vm/stack_frame.hpp"
#include "vm/heap_thing_inlines.hpp"

#include <algorithm>

namespace Whisper {
namespace VM {

//
// StackFrame
//

/*static*/ uint32_t
StackFrame::CalculateSize(const Config &config)
{
    return FixedSlots + config.maxStackDepth + config.numActualArgs;
}

void
StackFrame::initialize(const Config &config)
{
    WH_ASSERT(config.maxStackDepth <= MaxStackDepthMaskLow);
    uint64_t val = UInt64(config.maxStackDepth) << MaxStackDepthShift;
    info_ = MagicValue(val);
}

void
StackFrame::incrCurStackDepth()
{
    WH_ASSERT(curStackDepth() < CurStackDepthMaskLow);
    WH_ASSERT(CurStackDepthShift == 0);

    // Adding 1 to curStackDepth, which lives in the low bits.
    // The add cannot overflow.
    info_ = MagicValue(info_.getMagicInt() + 1);
}

void
StackFrame::decrCurStackDepth(uint32_t count)
{
    WH_ASSERT(curStackDepth() >= count);
    WH_ASSERT(CurStackDepthShift == 0);

    // Subtracting 1 to curStackDepth, which lives in the low bits.
    // The sub cannot overflow.
    info_ = MagicValue(info_.getMagicInt() - count);
}

StackFrame::StackFrame(Script *script, const Config &config)
  : callee_(script)
{
    initialize(config);
    WH_ASSERT(config.numActualArgs == numActualArgs());
}

bool
StackFrame::hasCallerFrame() const
{
    return callerFrame_.hasHeapThing();
}

StackFrame *
StackFrame::callerFrame() const
{
    WH_ASSERT(hasCallerFrame());
    return callerFrame_;
}

bool
StackFrame::isScriptFrame() const
{
    return callee_->isScript();
}

Script *
StackFrame::script() const
{
    WH_ASSERT(isScriptFrame());
    return callee_->toScript();
}

bool
StackFrame::isTopLevelFrame() const
{
    return isScriptFrame() && script()->isTopLevel();
}

uint32_t
StackFrame::maxStackDepth() const
{
    return (info_.getMagicInt() >> MaxStackDepthShift) & MaxStackDepthMaskLow;
}

uint32_t
StackFrame::curStackDepth() const
{
    return (info_.getMagicInt() >> CurStackDepthShift) & CurStackDepthMaskLow;
}

uint32_t
StackFrame::numActualArgs() const
{
    WH_ASSERT(objectValueCount() >= (FixedSlots + maxStackDepth()));
    return objectValueCount() - (FixedSlots + maxStackDepth());
}

const Value &
StackFrame::actualArg(uint32_t idx) const
{
    WH_ASSERT(idx < numActualArgs());
    return valueRef(FixedSlots + maxStackDepth() + idx);
}

void
StackFrame::pushValue(const Value &val)
{
    WH_ASSERT(curStackDepth() < maxStackDepth());
    uint32_t idx = FixedSlots + curStackDepth();
    noteWrite(&valueRef(idx));
    incrCurStackDepth();
    valueRef(idx) = val;
}

const Value &
StackFrame::peekValue(uint32_t offset) const
{
    WH_ASSERT(offset < curStackDepth());
    uint32_t idx = (FixedSlots + curStackDepth()) - offset;
    return valueRef(idx);
}

void
StackFrame::popValue(uint32_t count)
{
    // Do not need to noteWrite here because we are writing non-heapthing
    // references for sure.

    WH_ASSERT(curStackDepth() > 0);
    WH_ASSERT(count <= curStackDepth());
    uint32_t idxEnd = FixedSlots + curStackDepth();
    uint32_t idxStart = idxEnd - count;
    std::fill(&valueRef(idxStart), &valueRef(idxEnd), UndefinedValue());
    decrCurStackDepth(count);
}


} // namespace VM
} // namespace Whisper
