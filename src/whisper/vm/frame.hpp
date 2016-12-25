#ifndef WHISPER__VM__FRAME_HPP
#define WHISPER__VM__FRAME_HPP

#include "vm/core.hpp"
#include "vm/predeclare.hpp"
#include "vm/box.hpp"
#include "vm/exception.hpp"
#include "vm/control_flow.hpp"
#include "vm/slist.hpp"
#include "vm/function.hpp"
#include "parser/packed_syntax.hpp"

namespace Whisper {
namespace VM {

#define WHISPER_DEFN_FRAME_KINDS(_) \
    _(TerminalFrame)                \
    _(EntryFrame)                   \
    _(InvokeSyntaxNodeFrame)        \
    _(FileSyntaxFrame)              \
    _(BlockSyntaxFrame)             \
    _(ReturnStmtSyntaxFrame)        \
    _(VarSyntaxFrame)               \
    _(CallExprSyntaxFrame)          \
    _(InvokeApplicativeFrame)       \
    _(InvokeOperativeFrame)         \
    _(DotExprSyntaxFrame)           \
    _(NativeCallResumeFrame)


#define PREDECLARE_FRAME_CLASSES_(name) class name;
    WHISPER_DEFN_FRAME_KINDS(PREDECLARE_FRAME_CLASSES_)
#undef PREDECLARE_FRAME_CLASSES_

//
// Base class for interpreter frames.
//
class Frame
{
    friend class TraceTraits<Frame>;
  protected:
    // The parent frame.
    HeapField<Frame*> parent_;

    Frame(Frame* parent)
      : parent_(parent)
    {}

  public:
    Frame* parent() const {
        return parent_;
    }

    static StepResult Resolve(ThreadContext* cx,
                              Handle<Frame*> frame,
                              Handle<EvalResult> result);

    static StepResult Resolve(ThreadContext* cx,
                              Handle<Frame*> frame,
                              EvalResult const& result)
    {
        Local<EvalResult> rootedResult(cx, result);
        return Resolve(cx, frame, rootedResult.handle());
    }

    static StepResult Step(ThreadContext* cx, Handle<Frame*> frame);

    EntryFrame* maybeAncestorEntryFrame();
    EntryFrame* ancestorEntryFrame() {
        EntryFrame* result = maybeAncestorEntryFrame();
        WH_ASSERT(result);
        return result;
    }

#define FRAME_KIND_METHODS_(name) \
    bool is##name() const { \
        return HeapThing::From(this)->is##name(); \
    } \
    name const* to##name() const { \
        WH_ASSERT(is##name()); \
        return reinterpret_cast<name const*>(this); \
    } \
    name* to##name() { \
        WH_ASSERT(is##name()); \
        return reinterpret_cast<name*>(this); \
    }
    WHISPER_DEFN_FRAME_KINDS(FRAME_KIND_METHODS_)
#undef FRAME_KIND_METHODS_
};

//
// An TerminalFrame is signifies the end of computation when its
// child is resolved.
//
// It is always the bottom-most frame in the frame stack, and
// thus has a null parent frame.
//
class TerminalFrame : public Frame
{
    friend class TraceTraits<TerminalFrame>;

  private:
    HeapField<EvalResult> result_;

  public:
    TerminalFrame()
      : Frame(nullptr),
        result_(EvalResult::UndefinedValue())
    {}

    static Result<TerminalFrame*> Create(AllocationContext acx);

    EvalResult const& result() const {
        return result_;
    }

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<TerminalFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<TerminalFrame*> frame);
};

//
// An EntryFrame establishes an object in the frame chain which
// represents the entry into a new evaluation scope.  It establishes
// the PackedSyntaxTree in effect, the offset of the logical AST
// node the evaluation relates to (e.g. the File or DefStmt node),
// and the scope object in effect.
//
// All syntactic child frames within the lexical scope of this
// entry frame refer to it.
//
class EntryFrame : public Frame
{
    friend class TraceTraits<EntryFrame>;

  private:
    // The syntax tree in effect.
    HeapField<SyntaxNode*> syntaxNode_;

    // The scope in effect.
    HeapField<ScopeObject*> scope_;

  public:
    EntryFrame(Frame* parent,
               SyntaxNode* syntaxNode,
               ScopeObject* scope)
      : Frame(parent),
        syntaxNode_(syntaxNode),
        scope_(scope)
    {
        WH_ASSERT(parent != nullptr);
    }

    static Result<EntryFrame*> Create(AllocationContext acx,
                                      Handle<Frame*> parent,
                                      Handle<SyntaxNode*> syntaxNode,
                                      Handle<ScopeObject*> scope);

    SyntaxNode* syntaxNode() const {
        return syntaxNode_;
    }
    ScopeObject* scope() const {
        return scope_;
    }

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<EntryFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx, Handle<EntryFrame*> frame);
};

class SyntaxFrame : public Frame
{
    friend class TraceTraits<SyntaxFrame>;

  protected:
    // The entry frame corresponding to the syntax frame.
    HeapField<EntryFrame*> entryFrame_;

    // The syntax tree fargment corresponding to the frame being
    // evaluated.
    HeapField<SyntaxNode*> syntaxNode_;

    SyntaxFrame(Frame* parent,
                EntryFrame* entryFrame,
                SyntaxNode* syntaxNode)
      : Frame(parent),
        entryFrame_(entryFrame),
        syntaxNode_(syntaxNode)
    {
        WH_ASSERT(parent != nullptr);
        WH_ASSERT(entryFrame != nullptr);
        WH_ASSERT(syntaxNode != nullptr);
    }

  public:
    EntryFrame* entryFrame() const {
        return entryFrame_;
    }
    SyntaxNode* syntaxNode() const {
        return syntaxNode_;
    }
};

class InvokeSyntaxNodeFrame : public SyntaxFrame
{
    friend class TraceTraits<InvokeSyntaxNodeFrame>;
  public:
    InvokeSyntaxNodeFrame(Frame* parent,
                          EntryFrame* entryFrame,
                          SyntaxNode* syntaxNode)
      : SyntaxFrame(parent, entryFrame, syntaxNode)
    {}

    static Result<InvokeSyntaxNodeFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<InvokeSyntaxNodeFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<InvokeSyntaxNodeFrame*> frame);
};


class FileSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<FileSyntaxFrame>;
  private:
    uint32_t statementNo_;

  public:
    FileSyntaxFrame(Frame* parent,
                    EntryFrame* entryFrame,
                    SyntaxNode* syntaxNode,
                    uint32_t statementNo)
      : SyntaxFrame(parent, entryFrame, syntaxNode),
        statementNo_(statementNo)
    {}

    uint32_t statementNo() const {
        return statementNo_;
    }

    static Result<FileSyntaxFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode,
            uint32_t statementNo);

    static Result<FileSyntaxFrame*> CreateNext(
            AllocationContext acx,
            Handle<FileSyntaxFrame*> curFrame);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<FileSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<FileSyntaxFrame*> frame);
};

class BlockSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<BlockSyntaxFrame>;
  private:
    uint32_t statementNo_;

  public:
    BlockSyntaxFrame(Frame* parent,
                     EntryFrame* entryFrame,
                     SyntaxNode* syntaxNode,
                     uint32_t statementNo)
      : SyntaxFrame(parent, entryFrame, syntaxNode),
        statementNo_(statementNo)
    {}

    uint32_t statementNo() const {
        return statementNo_;
    }

    static Result<BlockSyntaxFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode,
            uint32_t statementNo);

    static Result<BlockSyntaxFrame*> CreateNext(
            AllocationContext acx,
            Handle<BlockSyntaxFrame*> curFrame);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<BlockSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<BlockSyntaxFrame*> frame);
};

class ReturnStmtSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<ReturnStmtSyntaxFrame>;
  private:
    uint32_t bindingNo_;

  public:
    ReturnStmtSyntaxFrame(Frame* parent,
                          EntryFrame* entryFrame,
                          SyntaxNode* syntaxNode)
      : SyntaxFrame(parent, entryFrame, syntaxNode)
    {}

    static Result<ReturnStmtSyntaxFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<ReturnStmtSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<ReturnStmtSyntaxFrame*> frame);
};

class VarSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<VarSyntaxFrame>;
  private:
    uint32_t bindingNo_;

  public:
    VarSyntaxFrame(Frame* parent,
                   EntryFrame* entryFrame,
                   SyntaxNode* syntaxNode,
                   uint32_t bindingNo)
      : SyntaxFrame(parent, entryFrame, syntaxNode),
        bindingNo_(bindingNo)
    {}

    bool isConst() const {
        return syntaxNode()->isConstStmt();
    }
    bool isVar() const {
        return syntaxNode()->isVarStmt();
    }

    uint32_t bindingNo() const {
        return bindingNo_;
    }

    static Result<VarSyntaxFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode,
            uint32_t bindingNo);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<VarSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<VarSyntaxFrame*> frame);
};

class CallExprSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<CallExprSyntaxFrame>;
  public:
    enum class State : uint8_t { Callee, Arg, Invoke };

  private:
    State state_;
    uint32_t argNo_;
    HeapField<ValBox> callee_;
    HeapField<FunctionObject*> calleeFunc_;
    HeapField<Slist<ValBox>*> operands_;

  public:
    CallExprSyntaxFrame(Frame* parent,
                        EntryFrame* entryFrame,
                        SyntaxNode* syntaxNode,
                        State state,
                        uint32_t argNo,
                        ValBox const& callee,
                        FunctionObject* calleeFunc,
                        Slist<ValBox>* operands)
      : SyntaxFrame(parent, entryFrame, syntaxNode),
        state_(state),
        argNo_(argNo),
        callee_(callee),
        calleeFunc_(calleeFunc),
        operands_(operands)
    {}

    State state() const {
        return state_;
    }
    bool inCalleeState() const {
        return state() == State::Callee;
    }
    bool inArgState() const {
        return state() == State::Arg;
    }
    bool inInvokeState() const {
        return state() == State::Invoke;
    }

    uint32_t argNo() const {
        WH_ASSERT(inArgState());
        return argNo_;
    }
    ValBox const& callee() const {
        WH_ASSERT(inArgState() || inInvokeState());
        return callee_;
    }
    FunctionObject* calleeFunc() const {
        WH_ASSERT(inArgState() || inInvokeState());
        return calleeFunc_;
    }
    Slist<ValBox>* operands() const {
        WH_ASSERT(inArgState() || inInvokeState());
        return operands_;
    }

    static Result<CallExprSyntaxFrame*> CreateCallee(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode);

    static Result<CallExprSyntaxFrame*> CreateFirstArg(
            AllocationContext acx,
            Handle<CallExprSyntaxFrame*> calleeFrame,
            Handle<ValBox> callee,
            Handle<FunctionObject*> calleeFunc);

    static Result<CallExprSyntaxFrame*> CreateNextArg(
            AllocationContext acx,
            Handle<CallExprSyntaxFrame*> calleeFrame,
            Handle<Slist<ValBox>*> operands);

    static Result<CallExprSyntaxFrame*> CreateInvoke(
            AllocationContext acx,
            Handle<CallExprSyntaxFrame*> frame,
            Handle<Slist<ValBox>*> operands);

    static Result<CallExprSyntaxFrame*> CreateInvoke(
            AllocationContext acx,
            Handle<CallExprSyntaxFrame*> frame,
            Handle<ValBox> callee,
            Handle<FunctionObject*> calleeFunc,
            Handle<Slist<ValBox>*> operands);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<CallExprSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<CallExprSyntaxFrame*> frame);

  private:
    static StepResult ResolveCallee(
            ThreadContext* cx,
            Handle<CallExprSyntaxFrame*> frame,
            Handle<PackedSyntaxTree*> pst,
            Handle<AST::PackedCallExprNode> callExprNode,
            Handle<EvalResult> result);

    static StepResult ResolveArg(
            ThreadContext* cx,
            Handle<CallExprSyntaxFrame*> frame,
            Handle<PackedSyntaxTree*> pst,
            Handle<AST::PackedCallExprNode> callExprNode,
            Handle<EvalResult> result);

    static StepResult ResolveInvoke(
            ThreadContext* cx,
            Handle<CallExprSyntaxFrame*> frame,
            Handle<PackedSyntaxTree*> pst,
            Handle<AST::PackedCallExprNode> callExprNode,
            Handle<EvalResult> result);

    static StepResult StepCallee(ThreadContext* cx,
                                 Handle<CallExprSyntaxFrame*> frame);
    static StepResult StepArg(ThreadContext* cx,
                              Handle<CallExprSyntaxFrame*> frame);
    static StepResult StepInvoke(ThreadContext* cx,
                                 Handle<CallExprSyntaxFrame*> frame);

    static StepResult StepSubexpr(ThreadContext* cx,
                                  Handle<CallExprSyntaxFrame*> frame,
                                  Handle<PackedSyntaxTree*> pst,
                                  uint32_t offset);
};

class InvokeApplicativeFrame : public Frame
{
    friend class TraceTraits<InvokeApplicativeFrame>;
  private:
    HeapField<ValBox> callee_;
    HeapField<FunctionObject*> calleeFunc_;
    HeapField<Slist<ValBox>*> operands_;

  public:
    InvokeApplicativeFrame(Frame* parent,
                           ValBox const& callee,
                           FunctionObject* calleeFunc,
                           Slist<ValBox>* operands)
      : Frame(parent),
        callee_(callee),
        calleeFunc_(calleeFunc),
        operands_(operands)
    {}

    ValBox const& callee() const {
        return callee_;
    }
    FunctionObject* calleeFunc() const {
        return calleeFunc_;
    }
    Slist<ValBox>* operands() const {
        return operands_;
    }

    static Result<InvokeApplicativeFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<ValBox> callee,
            Handle<FunctionObject*> calleeFunc,
            Handle<Slist<ValBox>*> operands);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<InvokeApplicativeFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<InvokeApplicativeFrame*> frame);
};

class InvokeOperativeFrame : public Frame
{
    friend class TraceTraits<InvokeOperativeFrame>;
  private:
    HeapField<ValBox> callee_;
    HeapField<FunctionObject*> calleeFunc_;
    HeapField<SyntaxNode*> syntaxNode_;

  public:
    InvokeOperativeFrame(Frame* parent,
                         ValBox const& callee,
                         FunctionObject* calleeFunc,
                         SyntaxNode* syntaxNode)
      : Frame(parent),
        callee_(callee),
        calleeFunc_(calleeFunc),
        syntaxNode_(syntaxNode)
    {}

    ValBox const& callee() const {
        return callee_;
    }
    FunctionObject* calleeFunc() const {
        return calleeFunc_;
    }
    SyntaxNode* syntaxNode() const {
        return syntaxNode_;
    }

    static Result<InvokeOperativeFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<ValBox> callee,
            Handle<FunctionObject*> calleeFunc,
            Handle<SyntaxNode*> syntaxNode);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<InvokeOperativeFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<InvokeOperativeFrame*> frame);
};

class DotExprSyntaxFrame : public SyntaxFrame
{
    friend class TraceTraits<DotExprSyntaxFrame>;
  public:
    DotExprSyntaxFrame(Frame* parent,
                       EntryFrame* entryFrame,
                       SyntaxNode* syntaxNode)
      : SyntaxFrame(parent, entryFrame, syntaxNode)
    {}

    static Result<DotExprSyntaxFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<EntryFrame*> entryFrame,
            Handle<SyntaxNode*> syntaxNode);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<DotExprSyntaxFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<DotExprSyntaxFrame*> frame);
};

typedef VM::CallResult (*NativeCallResumeFuncPtr)(
        ThreadContext* cx,
        Handle<NativeCallInfo> callInfo,
        Handle<HeapThing*> state,
        Handle<VM::EvalResult> evalResult);

class NativeCallResumeFrame : public Frame
{
    friend class TraceTraits<NativeCallResumeFrame>;
  private:
    HeapField<LookupState*> lookupState_;
    HeapField<ScopeObject*> callerScope_;
    HeapField<FunctionObject*> calleeFunc_;
    HeapField<ValBox> receiver_;
    HeapField<ScopeObject*> evalScope_;
    HeapField<SyntaxNode*> syntaxNode_;
    NativeCallResumeFuncPtr resumeFunc_;
    HeapField<HeapThing*> resumeState_;

  public:
    NativeCallResumeFrame(Frame* parent,
                          NativeCallInfo const& callInfo,
                          ScopeObject* evalScope,
                          SyntaxNode* syntaxNode,
                          NativeCallResumeFuncPtr resumeFunc,
                          HeapThing* resumeState)
      : Frame(parent),
        lookupState_(callInfo.lookupState()),
        callerScope_(callInfo.callerScope()),
        calleeFunc_(callInfo.calleeFunc()),
        receiver_(callInfo.receiver()),
        evalScope_(evalScope),
        syntaxNode_(syntaxNode),
        resumeFunc_(resumeFunc),
        resumeState_(resumeState)
    {}

    LookupState* lookupState() const {
        return lookupState_;
    }

    ScopeObject* callerScope() const {
        return callerScope_;
    }

    FunctionObject* calleeFunc() const {
        return calleeFunc_;
    }

    ValBox const& receiver() const {
        return receiver_;
    }

    ScopeObject* evalScope() const {
        return evalScope_;
    }

    SyntaxNode* syntaxNode() const {
        return syntaxNode_;
    }

    NativeCallResumeFuncPtr resumeFunc() const {
        return resumeFunc_;
    }

    HeapThing* resumeState() const {
        return resumeState_;
    }

    static Result<NativeCallResumeFrame*> Create(
            AllocationContext acx,
            Handle<Frame*> parent,
            Handle<NativeCallInfo> callInfo,
            Handle<ScopeObject*> evalScope,
            Handle<SyntaxNode*> syntaxNode,
            NativeCallResumeFuncPtr resumeFunc,
            Handle<HeapThing*> resumeState);

    static StepResult ResolveImpl(ThreadContext* cx,
                                  Handle<NativeCallResumeFrame*> frame,
                                  Handle<EvalResult> result);
    static StepResult StepImpl(ThreadContext* cx,
                               Handle<NativeCallResumeFrame*> frame);
};


} // namespace VM


//
// GC Specializations
//

template <>
struct TraceTraits<VM::Frame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::Frame const& obj,
                     void const* start, void const* end)
    {
        obj.parent_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::Frame& obj,
                       void const* start, void const* end)
    {
        obj.parent_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::TerminalFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::TerminalFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.result_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::TerminalFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.result_.update(updater, start, end);
    }
};


template <>
struct TraceTraits<VM::EntryFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::EntryFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.syntaxNode_.scan(scanner, start, end);
        obj.scope_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::EntryFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.syntaxNode_.update(updater, start, end);
        obj.scope_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::SyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::SyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.entryFrame_.scan(scanner, start, end);
        obj.syntaxNode_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::SyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.entryFrame_.update(updater, start, end);
        obj.syntaxNode_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::InvokeSyntaxNodeFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::InvokeSyntaxNodeFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::InvokeSyntaxNodeFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::FileSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::FileSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::FileSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::BlockSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::BlockSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::BlockSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::ReturnStmtSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::ReturnStmtSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::ReturnStmtSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::VarSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::VarSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::VarSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::CallExprSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::CallExprSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
        obj.callee_.scan(scanner, start, end);
        obj.operands_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::CallExprSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
        obj.callee_.update(updater, start, end);
        obj.operands_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::DotExprSyntaxFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::DotExprSyntaxFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Scan<Scanner>(scanner, obj, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::DotExprSyntaxFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::SyntaxFrame>::Update<Updater>(updater, obj, start, end);
    }
};

template <>
struct TraceTraits<VM::InvokeApplicativeFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::InvokeApplicativeFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.callee_.scan(scanner, start, end);
        obj.calleeFunc_.scan(scanner, start, end);
        obj.operands_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::InvokeApplicativeFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.callee_.update(updater, start, end);
        obj.calleeFunc_.update(updater, start, end);
        obj.operands_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::InvokeOperativeFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::InvokeOperativeFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.callee_.scan(scanner, start, end);
        obj.calleeFunc_.scan(scanner, start, end);
        obj.syntaxNode_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::InvokeOperativeFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.callee_.update(updater, start, end);
        obj.calleeFunc_.update(updater, start, end);
        obj.syntaxNode_.update(updater, start, end);
    }
};

template <>
struct TraceTraits<VM::NativeCallResumeFrame>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::NativeCallResumeFrame const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Scan<Scanner>(scanner, obj, start, end);
        obj.lookupState_.scan(scanner, start, end);
        obj.callerScope_.scan(scanner, start, end);
        obj.calleeFunc_.scan(scanner, start, end);
        obj.receiver_.scan(scanner, start, end);
        obj.evalScope_.scan(scanner, start, end);
        obj.syntaxNode_.scan(scanner, start, end);
        obj.resumeState_.scan(scanner, start, end);
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::NativeCallResumeFrame& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Frame>::Update<Updater>(updater, obj, start, end);
        obj.lookupState_.update(updater, start, end);
        obj.callerScope_.update(updater, start, end);
        obj.calleeFunc_.update(updater, start, end);
        obj.receiver_.update(updater, start, end);
        obj.evalScope_.update(updater, start, end);
        obj.syntaxNode_.update(updater, start, end);
        obj.resumeState_.update(updater, start, end);
    }
};


} // namespace Whisper


#endif // WHISPER__VM__FRAME_HPP
