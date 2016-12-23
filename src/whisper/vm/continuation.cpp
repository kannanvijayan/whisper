
#include "runtime_inlines.hpp"
#include "vm/continuation.hpp"

namespace Whisper {
namespace VM {


/* static */ Result<Continuation*>
Continuation::Create(AllocationContext acx, Handle<Frame*> frame)
{
    return acx.create<Continuation>(frame);
}

StepResult
Continuation::continueWith(ThreadContext* cx, Handle<ValBox> value) const
{
    Local<Frame*> frame(cx, frame_);
    return Frame::Resolve(cx, frame, EvalResult::Value(value.get()));
}


} // namespace VM
} // namespace Whisper
