
#include "runtime_inlines.hpp"
#include "vm/core.hpp"
#include "vm/predeclare.hpp"
#include "vm/frame.hpp"
#include "vm/runtime_state.hpp"
#include "vm/function.hpp"
#include "interp/heap_interpreter.hpp"

namespace Whisper {
namespace VM {


/* static */ OkResult
Frame::ResolveChild(ThreadContext* cx, Handle<Frame*> frame,
                    ControlFlow const& flow)
{
#define RESOLVE_CHILD_CASE_(name) \
    if (frame->is##name()) \
        return name::ResolveChildImpl(cx, frame.upConvertTo<name*>(), flow);

    WHISPER_DEFN_FRAME_KINDS(RESOLVE_CHILD_CASE_)

#undef RESOLVE_CHILD_CASE_

    WH_UNREACHABLE("Unrecognized frame type.");
    return ErrorVal();
}

/* static */ OkResult
Frame::Step(ThreadContext* cx, Handle<Frame*> frame)
{
#define RESOLVE_CHILD_CASE_(name) \
    if (frame->is##name()) \
        return name::StepImpl(cx, frame.upConvertTo<name*>());

    WHISPER_DEFN_FRAME_KINDS(RESOLVE_CHILD_CASE_)

#undef RESOLVE_CHILD_CASE_

    WH_UNREACHABLE("Unrecognized frame type.");
    return ErrorVal();
}

/* static */ Result<TerminalFrame*>
TerminalFrame::Create(AllocationContext acx)
{
    return acx.create<TerminalFrame>();
}

/* static */ OkResult
TerminalFrame::ResolveChildImpl(ThreadContext* cx,
                                Handle<TerminalFrame*> frame,
                                ControlFlow const& flow)
{
    // Any resolving of a child returns this frame as-is.
    return OkVal();
}

/* static */ OkResult
TerminalFrame::StepImpl(ThreadContext* cx,
                        Handle<TerminalFrame*> frame)
{
    // TerminalFrame should never be stepped!
    WH_UNREACHABLE("TerminalFrame should never be step-executed.");
    return cx->setInternalError("TerminalFrame should never be step-executed.");
}

/* static */ Result<EntryFrame*>
EntryFrame::Create(AllocationContext acx,
                   Handle<Frame*> parent,
                   Handle<SyntaxTreeFragment*> stFrag,
                   Handle<ScopeObject*> scope)
{
    return acx.create<EntryFrame>(parent, stFrag, scope);
}

/* static */ Result<EntryFrame*>
EntryFrame::Create(AllocationContext acx,
                   Handle<SyntaxTreeFragment*> stFrag,
                   Handle<ScopeObject*> scope)
{
    Local<Frame*> parent(acx, acx.threadContext()->topFrame());
    return Create(acx, parent, stFrag, scope);
}

/* static */ OkResult
EntryFrame::ResolveChildImpl(ThreadContext* cx, Handle<EntryFrame*> frame,
                             ControlFlow const& flow)
{
    // Resolve parent frame with the same controlflow result.
    Local<Frame*> rootedParent(cx, frame->parent());
    return Frame::ResolveChild(cx, rootedParent, flow);
}

/* static */ OkResult
EntryFrame::StepImpl(ThreadContext* cx, Handle<EntryFrame*> frame)
{
    // Call into the interpreter to initialize a SyntaxFrame
    // for the root node of this entry frame.
    Local<Frame*> newFrame(cx);
    if (!newFrame.setResult(Interp::CreateInitialSyntaxFrame(cx, frame)))
        return ErrorVal();

    // Update the top frame.
    cx->setTopFrame(newFrame);
    return OkVal();
}


/* static */ Result<SyntaxNameLookupFrame*>
SyntaxNameLookupFrame::Create(AllocationContext acx,
                              Handle<Frame*> parent,
                              Handle<EntryFrame*> entryFrame,
                              Handle<SyntaxTreeFragment*> stFrag)
{
    return acx.create<SyntaxNameLookupFrame>(parent, entryFrame, stFrag);
}

/* static */ Result<SyntaxNameLookupFrame*>
SyntaxNameLookupFrame::Create(AllocationContext acx,
                              Handle<EntryFrame*> entryFrame,
                              Handle<SyntaxTreeFragment*> stFrag)
{
    Local<Frame*> parent(acx, acx.threadContext()->topFrame());
    return Create(acx, parent, entryFrame, stFrag);
}

/* static */ OkResult
SyntaxNameLookupFrame::ResolveChildImpl(
        ThreadContext* cx,
        Handle<SyntaxNameLookupFrame*> frame,
        ControlFlow const& flow)
{
    WH_ASSERT(flow.isError() || flow.isException() || flow.isValue());

    if (flow.isError() || flow.isException())
        return ErrorVal();

    // Create invocation frame for the looked up value.
    Local<ValBox> syntaxHandler(cx, flow.value());
    Local<EntryFrame*> entryFrame(cx, frame->entryFrame());
    Local<Frame*> parentFrame(cx, frame->parent());

    Local<SyntaxTreeFragment*> arg(cx, frame->stFrag());
    Local<Frame*> invokeFrame(cx);
    if (!invokeFrame.setResult(Interp::CreateInvokeSyntaxFrame(cx,
            entryFrame, parentFrame, syntaxHandler, arg)))
    {
        return ErrorVal();
    }

    cx->setTopFrame(invokeFrame);
    return OkVal();
}

/* static */ OkResult
SyntaxNameLookupFrame::StepImpl(ThreadContext* cx,
                                Handle<SyntaxNameLookupFrame*> frame)
{
    // Get the name of the syntax handler method.
    Local<String*> name(cx,
        cx->runtimeState()->syntaxHandlerName(frame->stFrag()));
    if (name.get() == nullptr) {
        WH_UNREACHABLE("Handler name not found for SyntaxTreeFragment.");
        cx->setInternalError("Handler name not found for SyntaxTreeFragment.");
        return ErrorVal();
    }

    // Look up the property on the scope object.
    Local<ScopeObject*> scope(cx, frame->entryFrame()->scope());
    Interp::PropertyLookupResult lookupResult = Interp::GetObjectProperty(cx,
        scope.handle().convertTo<Wobject*>(), name);

    if (lookupResult.isError()) {
        return ResolveChildImpl(cx, frame, ControlFlow::Error());
    }

    if (lookupResult.isNotFound()) {
        cx->setExceptionRaised("Lookup name not found", name.get());
        return ResolveChildImpl(cx, frame, ControlFlow::Exception());
    }

    if (lookupResult.isFound()) {
        Local<PropertyDescriptor> descriptor(cx, lookupResult.descriptor());
        Local<LookupState*> lookupState(cx, lookupResult.lookupState());

        // Handle a value binding by returning the value.
        if (descriptor->isValue()) {
            return ResolveChildImpl(cx, frame,
                ControlFlow::Value(descriptor->valBox()));
        }

        // Handle a method binding by creating a bound FunctionObject
        // from the method.
        if (descriptor->isMethod()) {
            // Create a new function object bound to the scope.
            Local<ValBox> scopeVal(cx, ValBox::Object(scope.get()));
            Local<Function*> func(cx, descriptor->method());
            Local<FunctionObject*> funcObj(cx);
            if (!funcObj.setResult(FunctionObject::Create(
                    cx->inHatchery(), func, scopeVal, lookupState)))
            {
                return ErrorVal();
            }

            return ResolveChildImpl(cx, frame,
                    ControlFlow::Value(ValBox::Object(funcObj.get()))); 
        }

        WH_UNREACHABLE("PropertyDescriptor not one of Value, Method.");
        return ErrorVal();
    }

    WH_UNREACHABLE("Property lookup not one of Found, NotFound, Error.");
    return ErrorVal();
}


} // namespace VM
} // namespace Whisper
