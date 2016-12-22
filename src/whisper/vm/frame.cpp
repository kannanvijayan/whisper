
#include <limits>

#include "runtime_inlines.hpp"
#include "vm/core.hpp"
#include "vm/predeclare.hpp"
#include "vm/frame.hpp"
#include "vm/exception.hpp"
#include "vm/runtime_state.hpp"
#include "vm/function.hpp"
#include "interp/heap_interpreter.hpp"

namespace Whisper {
namespace VM {


/* static */ StepResult
Frame::ResolveChild(ThreadContext* cx,
                    Handle<Frame*> frame,
                    Handle<Frame*> childFrame,
                    Handle<EvalResult> result)
{
    WH_ASSERT(childFrame->parent() == frame);

#define RESOLVE_CHILD_CASE_(name) \
    if (frame->is##name()) \
        return name::ResolveChildImpl(cx, frame.upConvertTo<name*>(), \
                                      childFrame, result);

    WHISPER_DEFN_FRAME_KINDS(RESOLVE_CHILD_CASE_)

#undef RESOLVE_CHILD_CASE_

    WH_UNREACHABLE("Unrecognized frame type.");
    return ErrorVal();
}

/* static */ StepResult
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

EntryFrame*
Frame::maybeAncestorEntryFrame()
{
    Frame* cur = this;
    while (cur && !cur->isEntryFrame())
        cur = cur->parent();
    WH_ASSERT(!cur || cur->isEntryFrame());
    return reinterpret_cast<EntryFrame*>(cur);
}

/* static */ Result<TerminalFrame*>
TerminalFrame::Create(AllocationContext acx)
{
    return acx.create<TerminalFrame>();
}

/* static */ StepResult
TerminalFrame::ResolveChildImpl(ThreadContext* cx,
                                Handle<TerminalFrame*> frame,
                                Handle<Frame*> childFrame,
                                Handle<EvalResult> result)
{
    // Any resolving of a child of this frame just continues with
    // the terminal frame.
    frame->result_.set(result, frame.get());
    return StepResult::Continue(frame);
}

/* static */ StepResult
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

/* static */ StepResult
EntryFrame::ResolveChildImpl(ThreadContext* cx,
                             Handle<EntryFrame*> frame,
                             Handle<Frame*> childFrame,
                             Handle<EvalResult> result)
{
    // Resolve parent frame with the same result.
    Local<Frame*> rootedParent(cx, frame->parent());
    return Frame::ResolveChild(cx, rootedParent, frame, result);
}

/* static */ StepResult
EntryFrame::StepImpl(ThreadContext* cx, Handle<EntryFrame*> frame)
{
    // Call into the interpreter to initialize a SyntaxFrame
    // for the root node of this entry frame.
    Local<Frame*> newFrame(cx);
    if (!newFrame.setResult(Interp::CreateInitialSyntaxFrame(cx, frame, frame)))
        return ErrorVal();

    // Update the top frame.
    return StepResult::Continue(newFrame);
}

/* static */ Result<InvokeSyntaxNodeFrame*>
InvokeSyntaxNodeFrame::Create(AllocationContext acx,
                              Handle<Frame*> parent,
                              Handle<EntryFrame*> entryFrame,
                              Handle<SyntaxTreeFragment*> stFrag)
{
    return acx.create<InvokeSyntaxNodeFrame>(parent, entryFrame, stFrag);
}

/* static */ StepResult
InvokeSyntaxNodeFrame::ResolveChildImpl(
        ThreadContext* cx,
        Handle<InvokeSyntaxNodeFrame*> frame,
        Handle<Frame*> childFrame,
        Handle<EvalResult> result)
{
    // Resolve parent frame with the same result.
    Local<Frame*> rootedParent(cx, frame->parent());
    return Frame::ResolveChild(cx, rootedParent, frame, result);
}

/* static */ StepResult
InvokeSyntaxNodeFrame::StepImpl(ThreadContext* cx,
                                Handle<InvokeSyntaxNodeFrame*> frame)
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
    Local<Interp::PropertyLookupResult> lookupResult(cx,
        Interp::GetObjectProperty(cx, scope.handle().convertTo<Wobject*>(),
                                      name));

    Local<Frame*> parent(cx, frame->parent());
    Local<EvalResult> lookupEvalResult(cx,
        lookupResult->toEvalResult(cx, frame));

    WH_ASSERT(lookupEvalResult->isError() || lookupEvalResult->isExc() ||
              lookupEvalResult->isValue());

    if (!lookupEvalResult->isValue()) {
        return Frame::ResolveChild(cx, parent, frame, lookupEvalResult.get());
    }

    // Invoke the syntax handler.
    Local<ValBox> syntaxHandler(cx, lookupEvalResult->value());
    Local<ScopeObject*> callerScope(cx, frame->entryFrame()->scope());
    Local<SyntaxTreeFragment*> stFrag(cx, frame->stFrag());
    Local<CallResult> result(cx, Interp::InvokeOperativeValue(
            cx, frame, callerScope, syntaxHandler, stFrag));

    // Forward result from syntax handler.
    if (result->isError())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());

    if (result->isExc()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->excAsEvalResult());
    }

    if (result->isValue()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->valueAsEvalResult());
    }

    if (result->isVoid())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Void());

    if (result->isContinue())
        return StepResult::Continue(result->continueFrame());

    WH_UNREACHABLE("Unknown CallResult.");
    return ErrorVal();
}


/* static */ Result<FileSyntaxFrame*>
FileSyntaxFrame::Create(AllocationContext acx,
                        Handle<Frame*> parent,
                        Handle<EntryFrame*> entryFrame,
                        Handle<SyntaxTreeFragment*> stFrag,
                        uint32_t statementNo)
{
    return acx.create<FileSyntaxFrame>(parent, entryFrame, stFrag,
                                       statementNo);
}

/* static */ Result<FileSyntaxFrame*>
FileSyntaxFrame::CreateNext(AllocationContext acx,
                        Handle<FileSyntaxFrame*> curFrame)
{
    WH_ASSERT(curFrame->stFrag()->isNode());
    Local<SyntaxNodeRef> fileNode(acx,
        SyntaxNodeRef(curFrame->stFrag()->toNode()));
    WH_ASSERT(fileNode->nodeType() == AST::File);
    WH_ASSERT(curFrame->statementNo() < fileNode->astFile().numStatements());

    Local<Frame*> parent(acx, curFrame->parent());
    Local<EntryFrame*> entryFrame(acx, curFrame->entryFrame());
    Local<SyntaxTreeFragment*> stFrag(acx, curFrame->stFrag());
    uint32_t nextStatementNo = curFrame->statementNo() + 1;

    return Create(acx, parent, entryFrame, stFrag, nextStatementNo);
}

/* static */ StepResult
FileSyntaxFrame::ResolveChildImpl(
        ThreadContext* cx,
        Handle<FileSyntaxFrame*> frame,
        Handle<Frame*> childFrame,
        Handle<EvalResult> result)
{
    WH_ASSERT(frame->stFrag()->isNode());
    Local<SyntaxNodeRef> fileNode(cx, SyntaxNodeRef(frame->stFrag()->toNode()));
    WH_ASSERT(fileNode->nodeType() == AST::File);
    WH_ASSERT(frame->statementNo() < fileNode->astFile().numStatements());

    Local<Frame*> rootedParent(cx, frame->parent());

    // If result is an error, resolve to parent.
    if (result->isError() || result->isExc())
        return Frame::ResolveChild(cx, rootedParent, frame, result);

    // Otherwise, create new file syntax frame for executing next
    // statement.
    Local<FileSyntaxFrame*> nextFileFrame(cx);
    if (!nextFileFrame.setResult(FileSyntaxFrame::CreateNext(
            cx->inHatchery(), frame)))
    {
        return ErrorVal();
    }
    return StepResult::Continue(nextFileFrame);
}

/* static */ StepResult
FileSyntaxFrame::StepImpl(ThreadContext* cx,
                            Handle<FileSyntaxFrame*> frame)
{
    WH_ASSERT(frame->stFrag()->isNode());
    Local<SyntaxNodeRef> fileNode(cx, SyntaxNodeRef(frame->stFrag()->toNode()));
    WH_ASSERT(fileNode->nodeType() == AST::File);
    WH_ASSERT(frame->statementNo() <= fileNode->astFile().numStatements());

    Local<Frame*> rootedParent(cx, frame->parent());

    if (frame->statementNo() == fileNode->astFile().numStatements())
        return Frame::ResolveChild(cx, rootedParent, frame, EvalResult::Void());

    // Get SyntaxTreeFragment for next statement node.
    Local<SyntaxTreeFragment*> stmtNode(cx);
    if (!stmtNode.setResult(SyntaxNode::Create(
            cx->inHatchery(), fileNode->pst(),
            fileNode->astFile().statement(frame->statementNo()).offset())))
    {
        return ErrorVal();
    }

    // Create a new InvokeSyntaxNode frame for interpreting each statement.
    Local<ScopeObject*> scope(cx, frame->entryFrame()->scope());
    Local<VM::EntryFrame*> entryFrame(cx, frame->entryFrame());
    Local<VM::InvokeSyntaxNodeFrame*> syntaxFrame(cx);
    if (!syntaxFrame.setResult(VM::InvokeSyntaxNodeFrame::Create(
            cx->inHatchery(), frame, entryFrame, stmtNode)))
    {
        return ErrorVal();
    }

    return StepResult::Continue(syntaxFrame);
}


/* static */ Result<CallExprSyntaxFrame*>
CallExprSyntaxFrame::CreateCallee(AllocationContext acx,
                                  Handle<Frame*> parent,
                                  Handle<EntryFrame*> entryFrame,
                                  Handle<SyntaxTreeFragment*> stFrag)
{
    return acx.create<CallExprSyntaxFrame>(parent, entryFrame, stFrag,
                                           State::Callee, 0,
                                           ValBox(), nullptr, nullptr);
}

/* static */ Result<CallExprSyntaxFrame*>
CallExprSyntaxFrame::CreateFirstArg(AllocationContext acx,
                                    Handle<CallExprSyntaxFrame*> calleeFrame,
                                    Handle<ValBox> callee,
                                    Handle<FunctionObject*> calleeFunc)
{
    Local<Frame*> parent(acx, calleeFrame->parent());
    Local<EntryFrame*> entryFrame(acx, calleeFrame->entryFrame());
    Local<SyntaxTreeFragment*> stFrag(acx, calleeFrame->stFrag());
    return acx.create<CallExprSyntaxFrame>(parent.handle(),
                                           entryFrame.handle(),
                                           stFrag.handle(),
                                           State::Arg, 0,
                                           callee, calleeFunc, nullptr);
}

/* static */ Result<CallExprSyntaxFrame*>
CallExprSyntaxFrame::CreateNextArg(AllocationContext acx,
                                   Handle<CallExprSyntaxFrame*> calleeFrame,
                                   Handle<Slist<ValBox>*> operands)
{
    Local<Frame*> parent(acx, calleeFrame->parent());
    Local<EntryFrame*> entryFrame(acx, calleeFrame->entryFrame());
    Local<SyntaxTreeFragment*> stFrag(acx, calleeFrame->stFrag());
    Local<ValBox> callee(acx, calleeFrame->callee());
    Local<FunctionObject*> calleeFunc(acx, calleeFrame->calleeFunc());
    uint16_t argNo = calleeFrame->argNo() + 1;
    return acx.create<CallExprSyntaxFrame>(parent.handle(),
                                           entryFrame.handle(),
                                           stFrag.handle(),
                                           State::Arg, argNo,
                                           callee.handle(),
                                           calleeFunc.handle(),
                                           operands);
}

/* static */ Result<CallExprSyntaxFrame*>
CallExprSyntaxFrame::CreateInvoke(AllocationContext acx,
                                  Handle<CallExprSyntaxFrame*> frame,
                                  Handle<Slist<ValBox>*> operands)
{
    Local<Frame*> parent(acx, frame->parent());
    Local<EntryFrame*> entryFrame(acx, frame->entryFrame());
    Local<SyntaxTreeFragment*> stFrag(acx, frame->stFrag());
    Local<ValBox> callee(acx, frame->callee());
    Local<FunctionObject*> calleeFunc(acx, frame->calleeFunc());
    return acx.create<CallExprSyntaxFrame>(parent.handle(),
                                           entryFrame.handle(),
                                           stFrag.handle(),
                                           State::Invoke, 0,
                                           callee.handle(),
                                           calleeFunc.handle(),
                                           operands);
}

/* static */ Result<CallExprSyntaxFrame*>
CallExprSyntaxFrame::CreateInvoke(AllocationContext acx,
                                  Handle<CallExprSyntaxFrame*> frame,
                                  Handle<ValBox> callee,
                                  Handle<FunctionObject*> calleeFunc,
                                  Handle<Slist<ValBox>*> operands)
{
    Local<Frame*> parent(acx, frame->parent());
    Local<EntryFrame*> entryFrame(acx, frame->entryFrame());
    Local<SyntaxTreeFragment*> stFrag(acx, frame->stFrag());
    return acx.create<CallExprSyntaxFrame>(parent.handle(),
                                           entryFrame.handle(),
                                           stFrag.handle(),
                                           State::Invoke, 0,
                                           callee, calleeFunc, operands);
}

/* static */ StepResult
CallExprSyntaxFrame::ResolveChildImpl(
        ThreadContext* cx,
        Handle<CallExprSyntaxFrame*> frame,
        Handle<Frame*> childFrame,
        Handle<EvalResult> result)
{
    Local<SyntaxNodeRef> callNodeRef(cx, frame->stFrag()->toNode());
    WH_ASSERT(callNodeRef->nodeType() == AST::CallExpr);

    Local<PackedSyntaxTree*> pst(cx, frame->stFrag()->pst());
    Local<AST::PackedCallExprNode> callExprNode(cx, callNodeRef->astCallExpr());

    Local<VM::Frame*> parent(cx, frame->parent());

    // Always forward errors or exceptions.
    if (result->isError() || result->isExc())
        return Frame::ResolveChild(cx, parent, frame, result);

    // Switch on state to handle rest of behaviour.
    switch (frame->state_) {
      case State::Callee:
        return ResolveCalleeChild(cx, frame, pst, callExprNode, result);
      case State::Arg:
        return ResolveArgChild(cx, frame, pst, callExprNode, result);
      case State::Invoke:
        return ResolveInvokeChild(cx, frame, pst, callExprNode, result);
      default:
        WH_UNREACHABLE("Invalid State.");
        return cx->setError(RuntimeError::InternalError,
                            "Invalid CallExpr frame state.");
    }
}

/* static */ StepResult
CallExprSyntaxFrame::ResolveCalleeChild(
        ThreadContext* cx,
        Handle<CallExprSyntaxFrame*> frame,
        Handle<PackedSyntaxTree*> pst,
        Handle<AST::PackedCallExprNode> callExprNode,
        Handle<EvalResult> result)
{
    WH_ASSERT(frame->state_ == State::Callee);
    WH_ASSERT(result->isVoid() || result->isValue());

    Local<VM::Frame*> parent(cx, frame->parent());

    uint32_t offset = callExprNode->callee().offset();

    // A void result is forwarded as an exception.
    // Involving the syntax tree in question.
    if (result->isVoid()) {
        Local<SyntaxNodeRef> subNodeRef(cx,
            SyntaxNodeRef(pst, offset));
        Local<SyntaxNode*> subNode(cx);
        if (!subNode.setResult(subNodeRef->createSyntaxNode(
                                    cx->inHatchery())))
        {
            return ErrorVal();
        }

        Local<Exception*> exc(cx);
        if (!exc.setResult(InternalException::Create(cx->inHatchery(),
                           "Callee expression yielded void",
                           subNode.handle())))
        {
            return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());
        }

        return Frame::ResolveChild(cx, parent, frame,
                                   EvalResult::Exc(frame, exc));
    }

    WH_ASSERT(result->isValue());
    Local<ValBox> calleeBox(cx, result->value());
    Local<FunctionObject*> calleeObj(cx);
    if (!calleeObj.setMaybe(Interp::FunctionObjectForValue(cx, calleeBox))) {
        Local<Exception*> exc(cx);
        if (!exc.setResult(InternalException::Create(cx->inHatchery(),
                           "Callee expression is not callable",
                           calleeBox.handle())))
        {
            return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());
        }
        return Frame::ResolveChild(cx, parent, frame,
                                   EvalResult::Exc(frame, exc));
    }

    Local<CallExprSyntaxFrame*> nextFrame(cx);

    // If the function is an operative, the next frame to get created
    // will be an Invoke frame, since the args do not need to be evaluated.
    if (calleeObj->isOperative()) {
        // Zero-argument applicative can be invoked immediately.
        Local<Slist<ValBox>*> operands(cx, nullptr);
        if (!nextFrame.setResult(CallExprSyntaxFrame::CreateInvoke(
                cx->inHatchery(), frame, calleeBox, calleeObj, operands)))
        {
            return ErrorVal();
        }

        return StepResult::Continue(nextFrame.get());
    }

    // If the function is an applicative, check the arity of the call.
    WH_ASSERT(calleeObj->isApplicative());
    if (callExprNode->numArgs() == 0) {
        // Zero-argument applicative can be invoked immediately.
        Local<Slist<ValBox>*> operands(cx, nullptr);
        if (!nextFrame.setResult(CallExprSyntaxFrame::CreateInvoke(
                cx->inHatchery(), frame, calleeBox, calleeObj, operands)))
        {
            return ErrorVal();
        }

        return StepResult::Continue(nextFrame.get());
    }

    if (!nextFrame.setResult(CallExprSyntaxFrame::CreateFirstArg(
            cx->inHatchery(), frame, calleeBox, calleeObj)))
    {
        return ErrorVal();
    }

    return StepResult::Continue(nextFrame.get());
}

/* static */ StepResult
CallExprSyntaxFrame::ResolveArgChild(
        ThreadContext* cx,
        Handle<CallExprSyntaxFrame*> frame,
        Handle<PackedSyntaxTree*> pst,
        Handle<AST::PackedCallExprNode> callExprNode,
        Handle<EvalResult> result)
{
    WH_ASSERT(frame->state_ == State::Arg);
    WH_ASSERT(frame->argNo() < callExprNode->numArgs());
    WH_ASSERT(result->isVoid() || result->isValue());

    Local<VM::Frame*> parent(cx, frame->parent());

    uint32_t offset = callExprNode->arg(frame->argNo()).offset();

    // A void result is forwarded as an exception.
    // Involving the syntax tree in question.
    if (result->isVoid()) {
        Local<SyntaxNodeRef> subNodeRef(cx,
            SyntaxNodeRef(pst, offset));
        Local<SyntaxNode*> subNode(cx);
        if (!subNode.setResult(subNodeRef->createSyntaxNode(
                                    cx->inHatchery())))
        {
            return ErrorVal();
        }

        Local<Exception*> exc(cx);
        if (!exc.setResult(InternalException::Create(cx->inHatchery(),
                           "Callee arg expression yielded void",
                           subNode.handle())))
        {
            return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());
        }
        return Frame::ResolveChild(cx, parent, frame,
                                   EvalResult::Exc(frame, exc));
    }

    // Prepend the value to the operands list.
    Local<Slist<ValBox>*> oldOperands(cx, frame->operands());
    Local<Slist<ValBox>*> operands(cx);
    if (!operands.setResult(Slist<ValBox>::Create(
                cx->inHatchery(), result->value(), oldOperands)))
    {
        return ErrorVal();
    }

    uint32_t nextArgNo = frame->argNo() + 1;
    WH_ASSERT(nextArgNo <= callExprNode->numArgs());
    Local<CallExprSyntaxFrame*> invokeFrame(cx);

    if (nextArgNo == callExprNode->numArgs()) {
        if (!invokeFrame.setResult(CallExprSyntaxFrame::CreateInvoke(
                cx->inHatchery(), frame, operands)))
        {
            return ErrorVal();
        }
    } else {
        if (!invokeFrame.setResult(CallExprSyntaxFrame::CreateNextArg(
                cx->inHatchery(), frame, operands)))
        {
            return ErrorVal();
        }
    }

    return StepResult::Continue(invokeFrame.get());
}

/* static */ StepResult
CallExprSyntaxFrame::ResolveInvokeChild(
        ThreadContext* cx,
        Handle<CallExprSyntaxFrame*> frame,
        Handle<PackedSyntaxTree*> pst,
        Handle<AST::PackedCallExprNode> callExprNode,
        Handle<EvalResult> result)
{
    WH_ASSERT(frame->inInvokeState());
    WH_ASSERT(result->isVoid() || result->isValue());

    Local<Frame*> parent(cx, frame->parent());
    return Frame::ResolveChild(cx, parent, frame, result);
}

/* static */ StepResult
CallExprSyntaxFrame::StepImpl(ThreadContext* cx,
                              Handle<CallExprSyntaxFrame*> frame)
{
    WH_ASSERT(frame->stFrag()->isNode());

    // On initial step, just set up the entry frame for evaluating the
    // underlying callee or arg expression.

    switch (frame->state()) {
      case State::Callee:
        return StepCallee(cx, frame);
      case State::Arg:
        return StepArg(cx, frame);
      case State::Invoke:
        return StepInvoke(cx, frame);
      default:
        WH_UNREACHABLE("Invalid CallExprSyntaxFrame::State.");
        return cx->setError(RuntimeError::InternalError,
                            "Invalid CallExprSyntaxFrame::State.");
    }
}

/* static */ StepResult
CallExprSyntaxFrame::StepCallee(ThreadContext* cx,
                                Handle<CallExprSyntaxFrame*> frame)
{
    WH_ASSERT(frame->inCalleeState());

    Local<SyntaxNodeRef> callNodeRef(cx, frame->stFrag()->toNode());
    WH_ASSERT(callNodeRef->nodeType() == AST::CallExpr);

    Local<PackedSyntaxTree*> pst(cx, frame->stFrag()->pst());
    Local<AST::PackedCallExprNode> callExprNode(cx, callNodeRef->astCallExpr());

    return StepSubexpr(cx, frame, pst, callExprNode->callee().offset());
}

/* static */ StepResult
CallExprSyntaxFrame::StepArg(ThreadContext* cx,
                             Handle<CallExprSyntaxFrame*> frame)
{
    WH_ASSERT(frame->inArgState());

    // Only applicatives need evaluation of arguments.
    WH_ASSERT(frame->calleeFunc()->isApplicative());

    Local<SyntaxNodeRef> callNodeRef(cx, frame->stFrag()->toNode());
    WH_ASSERT(callNodeRef->nodeType() == AST::CallExpr);

    Local<PackedSyntaxTree*> pst(cx, frame->stFrag()->pst());
    Local<AST::PackedCallExprNode> callExprNode(cx, callNodeRef->astCallExpr());

    uint16_t argNo = frame->argNo();
    WH_ASSERT(argNo < callExprNode->numArgs());

    return StepSubexpr(cx, frame, pst, callExprNode->arg(argNo).offset());
}

/* static */ StepResult
CallExprSyntaxFrame::StepInvoke(ThreadContext* cx,
                                Handle<CallExprSyntaxFrame*> frame)
{
    WH_ASSERT(frame->inInvokeState());
    Local<ValBox> callee(cx, frame->callee());
    Local<FunctionObject*> calleeFunc(cx, frame->calleeFunc());
    Local<Slist<ValBox>*> operands(cx, frame->operands());

    if (calleeFunc->isApplicative()) {
        Local<InvokeApplicativeFrame*> invokeFrame(cx);
        if (!invokeFrame.setResult(InvokeApplicativeFrame::Create(
                cx->inHatchery(), frame, callee, calleeFunc, operands)))
        {
            return ErrorVal();
        }
        return StepResult::Continue(invokeFrame.get());
    }

    WH_ASSERT(calleeFunc->isOperative());
    WH_ASSERT(operands.get() == nullptr);

    Local<SyntaxTreeFragment*> stFrag(cx, frame->stFrag());

    Local<InvokeOperativeFrame*> invokeFrame(cx);
    if (!invokeFrame.setResult(InvokeOperativeFrame::Create(
            cx->inHatchery(), frame, callee, calleeFunc, stFrag)))
    {
        return ErrorVal();
    }
    return StepResult::Continue(invokeFrame.get());
}

/* static */ StepResult
CallExprSyntaxFrame::StepSubexpr(ThreadContext* cx,
                                 Handle<CallExprSyntaxFrame*> frame,
                                 Handle<PackedSyntaxTree*> pst,
                                 uint32_t offset)
{
    // Create a new SyntaxNode for the subexpression (callee or argN).
    Local<SyntaxNodeRef> nodeRef(cx, SyntaxNodeRef(pst, offset));
    Local<SyntaxNode*> node(cx);
    if (!node.setResult(nodeRef->createSyntaxNode(cx->inHatchery())))
        return ErrorVal();

    Local<ScopeObject*> scope(cx, frame->entryFrame()->scope());

    // Create and return entry frame.
    Local<EntryFrame*> entryFrame(cx, frame->entryFrame());
    Local<VM::InvokeSyntaxNodeFrame*> syntaxFrame(cx);
    if (!syntaxFrame.setResult(VM::InvokeSyntaxNodeFrame::Create(
            cx->inHatchery(), frame, entryFrame, node.handle())))
    {
        return ErrorVal();
    }

    return StepResult::Continue(syntaxFrame);
}

/* static */ Result<InvokeApplicativeFrame*>
InvokeApplicativeFrame::Create(
        AllocationContext acx,
        Handle<Frame*> parent,
        Handle<ValBox> callee,
        Handle<FunctionObject*> calleeFunc,
        Handle<Slist<ValBox>*> operands)
{
    return acx.create<InvokeApplicativeFrame>(parent, callee, calleeFunc,
                                              operands);
}

/* static */ StepResult
InvokeApplicativeFrame::ResolveChildImpl(ThreadContext* cx,
                                         Handle<InvokeApplicativeFrame*> frame,
                                         Handle<Frame*> childFrame,
                                         Handle<EvalResult> result)
{
    Local<Frame*> parent(cx, frame->parent());
    return Frame::ResolveChild(cx, parent, frame, result);
}

/* static */ StepResult
InvokeApplicativeFrame::StepImpl(ThreadContext* cx,
                                 Handle<InvokeApplicativeFrame*> frame)
{
    Local<ValBox> callee(cx, frame->callee());
    Local<FunctionObject*> calleeFunc(cx, frame->calleeFunc());
    Local<Slist<ValBox>*> operands(cx, frame->operands());
    Local<ScopeObject*> callerScope(cx, frame->ancestorEntryFrame()->scope());

    uint32_t length = operands->length();
    LocalArray<ValBox> args(cx, length);

    // Fill in args (reverse order).
    Slist<ValBox>* curArg = operands;
    for (uint32_t i = 0; i < length; i++) {
        WH_ASSERT(curArg);

        uint32_t idx = (length - 1) - i;
        args[idx] = curArg->value();

        curArg = curArg->rest();
    }

    // Invoke the applicative function.
    Local<CallResult> result(cx, Interp::InvokeApplicativeFunction(
            cx, frame, callerScope, callee, calleeFunc, args));

    Local<Frame*> parent(cx, frame->parent());

    if (result->isError())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());

    if (result->isExc()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->excAsEvalResult());
    }

    if (result->isValue()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->valueAsEvalResult());
    }

    if (result->isVoid())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Void());

    if (result->isContinue())
        return StepResult::Continue(result->continueFrame());

    WH_UNREACHABLE("Unknown CallResult outcome.");
    return cx->setError(RuntimeError::InternalError,
                        "Unknown CallResult outcome.");
}

/* static */ Result<InvokeOperativeFrame*>
InvokeOperativeFrame::Create(AllocationContext acx,
                             Handle<Frame*> parent,
                             Handle<ValBox> callee,
                             Handle<FunctionObject*> calleeFunc,
                             Handle<SyntaxTreeFragment*> stFrag)
{
    return acx.create<InvokeOperativeFrame>(parent, callee, calleeFunc,
                                            stFrag);
}

/* static */ StepResult
InvokeOperativeFrame::ResolveChildImpl(ThreadContext* cx,
                                   Handle<InvokeOperativeFrame*> frame,
                                   Handle<Frame*> childFrame,
                                   Handle<EvalResult> result)
{
    Local<Frame*> parent(cx, frame->parent());
    return Frame::ResolveChild(cx, parent, frame, result);
}

/* static */ StepResult
InvokeOperativeFrame::StepImpl(ThreadContext* cx,
                               Handle<InvokeOperativeFrame*> frame)
{
    Local<ValBox> callee(cx, frame->callee());
    Local<FunctionObject*> calleeFunc(cx, frame->calleeFunc());
    Local<SyntaxTreeFragment*> stFrag(cx, frame->stFrag());
    Local<ScopeObject*> callerScope(cx, frame->ancestorEntryFrame()->scope());

    Local<SyntaxNodeRef> callNodeRef(cx, frame->stFrag()->toNode());
    WH_ASSERT(callNodeRef->nodeType() == AST::CallExpr);

    Local<PackedSyntaxTree*> pst(cx, frame->stFrag()->pst());
    Local<AST::PackedCallExprNode> callExprNode(cx, callNodeRef->astCallExpr());

    // Assemble an array of SyntaxTreeFragment pointers.
    LocalArray<SyntaxTreeFragment*> operandExprs(cx, callExprNode->numArgs());
    for (uint32_t i = 0; i < callExprNode->numArgs(); i++) {
        uint32_t offset = callExprNode->arg(i).offset();
        if (!operandExprs.setResult(i,
                SyntaxNode::Create(cx->inHatchery(), pst, offset)))
        {
            return ErrorVal();
        }
    }

    // Invoke the applicative function.
    Local<CallResult> result(cx, Interp::InvokeOperativeFunction(
            cx, frame, callerScope, callee, calleeFunc, operandExprs));
    Local<Frame*> parent(cx, frame->parent());

    if (result->isError())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());

    if (result->isExc()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->excAsEvalResult());
    }

    if (result->isValue()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   result->valueAsEvalResult());
    }

    if (result->isVoid())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Void());

    if (result->isContinue())
        return StepResult::Continue(result->continueFrame());

    WH_UNREACHABLE("Unknown CallResult outcome.");
    return cx->setError(RuntimeError::InternalError,
                        "Unknown CallResult outcome.");
}

/* static */ Result<NativeCallResumeFrame*>
NativeCallResumeFrame::Create(AllocationContext acx,
                              Handle<Frame*> parent,
                              Handle<NativeCallInfo> callInfo,
                              Handle<ScopeObject*> evalScope,
                              Handle<SyntaxTreeFragment*> syntaxFragment,
                              NativeCallResumeFuncPtr resumeFunc,
                              Handle<HeapThing*> resumeState)
{
    return acx.create<NativeCallResumeFrame>(parent, callInfo, evalScope,
                                             syntaxFragment, resumeFunc,
                                             resumeState);
}

/* static */ StepResult
NativeCallResumeFrame::ResolveChildImpl(ThreadContext* cx,
                                        Handle<NativeCallResumeFrame*> frame,
                                        Handle<Frame*> childFrame,
                                        Handle<EvalResult> result)
{
    Local<Frame*> parent(cx, frame->parent());

    // When the child completes, call into the native resume func.
    NativeCallResumeFuncPtr resumeFunc = frame->resumeFunc();

    Local<NativeCallInfo> callInfo(cx,
        NativeCallInfo(parent,
                       frame->lookupState(),
                       frame->callerScope(),
                       frame->calleeFunc(),
                       frame->receiver()));
    Local<HeapThing*> resumeState(cx, frame->resumeState());

    Local<CallResult> resumeResult(cx,
        resumeFunc(cx, callInfo, resumeState, result));

    if (resumeResult->isError())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Error());

    if (resumeResult->isExc()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   resumeResult->excAsEvalResult());
    }

    if (resumeResult->isValue()) {
        return Frame::ResolveChild(cx, parent, frame,
                                   resumeResult->valueAsEvalResult());
    }

    if (resumeResult->isVoid())
        return Frame::ResolveChild(cx, parent, frame, EvalResult::Void());

    if (resumeResult->isContinue())
        return StepResult::Continue(resumeResult->continueFrame());

    WH_UNREACHABLE("Unknown CallResult.");
    return ErrorVal();
}

/* static */ StepResult
NativeCallResumeFrame::StepImpl(ThreadContext* cx,
                                Handle<NativeCallResumeFrame*> frame)
{
    Local<VM::SyntaxTreeFragment*> stFrag(cx, frame->syntaxFragment());
    Local<VM::ScopeObject*> evalScope(cx, frame->evalScope());

    // Create an EntryFrame for the evaluation of the syntax tree fragment.
    Local<EntryFrame*> entryFrame(cx);
    if (!entryFrame.setResult(EntryFrame::Create(cx->inHatchery(),
                                                 frame, stFrag, evalScope)))
    {
        return ErrorVal();
    }

    // Continue to it.
    return StepResult::Continue(entryFrame);
}


} // namespace VM
} // namespace Whisper
