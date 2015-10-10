
#include "runtime_inlines.hpp"
#include "vm/core.hpp"
#include "vm/predeclare.hpp"
#include "vm/frame.hpp"
#include "interp/heap_interpreter.hpp"

namespace Whisper {
namespace VM {


Result<Frame*>
Frame::resolveChild(ThreadContext* cx,
                    Handle<Frame*> child,
                    ControlFlow const& flow)
{
#define RESOLVE_CHILD_CASE_(name) \
    if (this->is##name()) \
        return this->to##name()->resolve##name##Child(cx, child, flow);

    WHISPER_DEFN_FRAME_KINDS(RESOLVE_CHILD_CASE_)

#undef RESOLVE_CHILD_CASE_

    WH_UNREACHABLE("Unrecognized frame type.");
    return ErrorVal();
}

Result<Frame*>
Frame::step(ThreadContext* cx)
{
#define RESOLVE_CHILD_CASE_(name) \
    if (this->is##name()) \
        return this->to##name()->step##name(cx);

    WHISPER_DEFN_FRAME_KINDS(RESOLVE_CHILD_CASE_)

#undef RESOLVE_CHILD_CASE_

    WH_UNREACHABLE("Unrecognized frame type.");
    return ErrorVal();
}

/* static */ Result<EntryFrame*>
EntryFrame::Create(AllocationContext acx,
                   Handle<Frame*> caller,
                   Handle<PackedSyntaxTree*> syntaxTree,
                   uint32_t syntaxOffset,
                   Handle<ScopeObject*> scope)
{
    return acx.create<EntryFrame>(caller, syntaxTree, syntaxOffset, scope);
}

/* static */ Result<EntryFrame*>
EntryFrame::Create(AllocationContext acx,
                   Handle<PackedSyntaxTree*> syntaxTree,
                   uint32_t syntaxOffset,
                   Handle<ScopeObject*> scope)
{
    Local<Frame*> caller(acx, acx.threadContext()->maybeLastFrame());
    return Create(acx, caller, syntaxTree, syntaxOffset, scope);
}

Result<Frame*>
EntryFrame::resolveEntryFrameChild(ThreadContext* cx,
                                      Handle<Frame*> child,
                                      ControlFlow const& flow)
{
    // Resolve caller frame with the same controlflow result.
    Local<Frame*> rootedThis(cx, this);
    return caller_->resolveChild(cx, rootedThis, flow);
}

Result<Frame*>
EntryFrame::stepEntryFrame(ThreadContext* cx)
{
    return ErrorVal();
}


/* static */ Result<EvalFrame*>
EvalFrame::Create(AllocationContext acx,
                  Handle<Frame*> caller,
                  Handle<SyntaxTreeFragment*> syntax)
{
    return acx.create<EvalFrame>(caller, syntax);
}

/* static */ Result<EvalFrame*>
EvalFrame::Create(AllocationContext acx, Handle<SyntaxTreeFragment*> syntax)
{
    Local<Frame*> caller(acx, acx.threadContext()->maybeLastFrame());
    return Create(acx, caller, syntax);
}

Result<Frame*>
EvalFrame::resolveEvalFrameChild(ThreadContext* cx,
                                 Handle<Frame*> child,
                                 ControlFlow const& flow)
{
    // Resolve caller frame with the same controlflow result.
    Local<Frame*> rootedThis(cx, this);
    return caller_->resolveChild(cx, rootedThis, flow);
}

Result<Frame*>
EvalFrame::stepEvalFrame(ThreadContext* cx)
{
    return OkVal<Frame*>(nullptr);
}


/* static */ Result<SyntaxFrame*>
SyntaxFrame::Create(AllocationContext acx,
                    Handle<Frame*> caller,
                    Handle<SyntaxNode*> node,
                    uint32_t position,
                    ResolveChildFunc resolveChildFunc,
                    StepFunc stepFunc)
{
    return acx.create<SyntaxFrame>(caller, node, position,
                                   resolveChildFunc, stepFunc);
}

/* static */ Result<SyntaxFrame*>
SyntaxFrame::Create(AllocationContext acx,
                    Handle<SyntaxNode*> node,
                    uint32_t position,
                    ResolveChildFunc resolveChildFunc,
                    StepFunc stepFunc)
{
    Local<Frame*> caller(acx, acx.threadContext()->maybeLastFrame());
    return Create(acx, caller, node, position,
                  resolveChildFunc, stepFunc);
}

Result<Frame*>
SyntaxFrame::resolveSyntaxFrameChild(ThreadContext* cx,
                                     Handle<Frame*> child,
                                     ControlFlow const& flow)
{
    Local<SyntaxFrame*> rootedThis(cx, this);
    return resolveChildFunc_(cx, rootedThis, child, flow);
}

Result<Frame*>
SyntaxFrame::stepSyntaxFrame(ThreadContext* cx)
{
    Local<SyntaxFrame*> rootedThis(cx, this);
    return stepFunc_(cx, rootedThis);
}


/* static */ Result<FunctionFrame*>
FunctionFrame::Create(AllocationContext acx,
                      Handle<Frame*> caller,
                      Handle<Function*> function)
{
    return acx.create<FunctionFrame>(caller, function);
}

/* static */ Result<FunctionFrame*>
FunctionFrame::Create(AllocationContext acx, Handle<Function*> function)
{
    Local<Frame*> caller(acx, acx.threadContext()->maybeLastFrame());
    return Create(acx, caller, function);
}

Result<Frame*>
FunctionFrame::resolveFunctionFrameChild(ThreadContext* cx,
                                         Handle<Frame*> child,
                                         ControlFlow const& flow)
{
    return OkVal<Frame*>(nullptr);
}

Result<Frame*>
FunctionFrame::stepFunctionFrame(ThreadContext* cx)
{
    return OkVal<Frame*>(nullptr);
}


} // namespace VM
} // namespace Whisper
