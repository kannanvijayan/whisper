
#include <iostream>

#include "parser/syntax_defn.hpp"
#include "parser/packed_syntax.hpp"
#include "runtime_inlines.hpp"
#include "vm/global_scope.hpp"
#include "vm/runtime_state.hpp"
#include "vm/function.hpp"
#include "interp/interpreter.hpp"
#include "interp/syntax_behaviour.hpp"

namespace Whisper {
namespace Interp {


// Declare a lift function for each syntax node type.
#define DECLARE_LIFT_FN_(name) \
    static VM::ControlFlow Lift_##name( \
        ThreadContext* cx, \
        Handle<VM::NativeCallInfo> callInfo, \
        ArrayHandle<VM::SyntaxTreeRef> args);

    DECLARE_LIFT_FN_(File)
    DECLARE_LIFT_FN_(EmptyStmt)
    DECLARE_LIFT_FN_(ExprStmt)
    DECLARE_LIFT_FN_(ReturnStmt)
    DECLARE_LIFT_FN_(DefStmt)
    DECLARE_LIFT_FN_(VarStmt)
    DECLARE_LIFT_FN_(ParenExpr)
    DECLARE_LIFT_FN_(NameExpr)
    DECLARE_LIFT_FN_(IntegerExpr)
    //WHISPER_DEFN_SYNTAX_NODES(DECLARE_LIFT_FN_)

#undef DECLARE_LIFT_FN_

static OkResult
BindGlobalMethod(AllocationContext acx,
                 Handle<VM::GlobalScope*> obj,
                 VM::String* name,
                 VM::NativeOperativeFuncPtr opFunc)
{
    Local<VM::String*> rootedName(acx, name);

    // Allocate NativeFunction object.
    Local<VM::NativeFunction*> natF(acx);
    if (!natF.setResult(VM::NativeFunction::Create(acx, opFunc)))
        return ErrorVal();
    Local<VM::PropertyDescriptor> desc(acx, VM::PropertyDescriptor(natF.get()));

    // Bind method on global.
    if (!VM::GlobalScope::DefineProperty(acx, obj, rootedName, desc))
        return ErrorVal();

    return OkVal();
}

OkResult
BindSyntaxHandlers(AllocationContext acx, VM::GlobalScope* scope)
{
    Local<VM::GlobalScope*> rootedScope(acx, scope);

    ThreadContext* cx = acx.threadContext();
    Local<VM::RuntimeState*> rtState(acx, cx->runtimeState());

#define BIND_GLOBAL_METHOD_(name) \
    do { \
        if (!BindGlobalMethod(acx, rootedScope, rtState->nm_At##name(), \
                              &Lift_##name)) \
        { \
            return ErrorVal(); \
        } \
    } while(false)

    BIND_GLOBAL_METHOD_(File);
    BIND_GLOBAL_METHOD_(EmptyStmt);
    BIND_GLOBAL_METHOD_(ExprStmt);
    BIND_GLOBAL_METHOD_(ReturnStmt);
    BIND_GLOBAL_METHOD_(DefStmt);
    BIND_GLOBAL_METHOD_(VarStmt);
    BIND_GLOBAL_METHOD_(ParenExpr);
    BIND_GLOBAL_METHOD_(NameExpr);
    BIND_GLOBAL_METHOD_(IntegerExpr);

#undef BIND_GLOBAL_METHOD_

    return OkVal();
}

#define IMPL_LIFT_FN_(name) \
    static VM::ControlFlow Lift_##name( \
        ThreadContext* cx, \
        Handle<VM::NativeCallInfo> callInfo, \
        ArrayHandle<VM::SyntaxTreeRef> args)

IMPL_LIFT_FN_(File)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@File called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::File);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedFileNode> fileNode(cx,
        AST::PackedFileNode(pst->data(), stRef->offset()));

    SpewInterpNote("Lift_File: Interpreting %u statements",
                   unsigned(fileNode->numStatements()));
    for (uint32_t i = 0; i < fileNode->numStatements(); i++) {
        Local<AST::PackedBaseNode> stmtNode(cx, fileNode->statement(i));
        SpewInterpNote("Lift_File: statement %u is %s",
                       unsigned(i), AST::NodeTypeString(stmtNode->type()));

        VM::ControlFlow stmtFlow = InterpretSyntax(cx, callInfo->callerScope(),
                                                   pst, stmtNode->offset());
        // Statements can yield void or value control flows and still
        // continue.
        if (stmtFlow.isVoid() || stmtFlow.isValue())
            continue;

        return stmtFlow;
    }

    return VM::ControlFlow::Void();
}

IMPL_LIFT_FN_(EmptyStmt)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@EmptyStmt called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::EmptyStmt);
    return VM::ControlFlow::Void();
}

IMPL_LIFT_FN_(ExprStmt)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@ExprStmt called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::ExprStmt);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedExprStmtNode> exprStmtNode(cx,
        AST::PackedExprStmtNode(pst->data(), stRef->offset()));

    Local<AST::PackedBaseNode> exprNode(cx, exprStmtNode->expression());

    VM::ControlFlow exprFlow = InterpretSyntax(cx, callInfo->callerScope(), pst,
                                               exprNode->offset());
    // An expression should only ever resolve to a value, error, or exception.
    WH_ASSERT(exprFlow.isExpressionResult());
    return exprFlow;
}

IMPL_LIFT_FN_(ReturnStmt)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@ReturnStmt called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::ReturnStmt);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedReturnStmtNode> returnStmtNode(cx,
        AST::PackedReturnStmtNode(pst->data(), stRef->offset()));

    // If it's a bare return, resolve to a return control flow with
    // an undefined value.
    if (!returnStmtNode->hasExpression()) {
        SpewInterpNote("Lift_ReturnStmt: Empty return.");
        return VM::ControlFlow::Return(VM::ValBox::Undefined());
    }

    SpewInterpNote("Lift_ReturnStmt: Evaluating expression.");
    Local<AST::PackedBaseNode> exprNode(cx, returnStmtNode->expression());
    VM::ControlFlow exprFlow = InterpretSyntax(cx, callInfo->callerScope(), pst,
                                               exprNode->offset());
    // An expression should only ever resolve to a value, error, or exception.
    WH_ASSERT(exprFlow.isExpressionResult());
    // If a value, wrap the value in a return, otherwise return straight.
    if (exprFlow.isValue())
        return VM::ControlFlow::Return(exprFlow.value());

    return exprFlow;
}

IMPL_LIFT_FN_(DefStmt)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@DefStmt called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::DefStmt);

    Local<VM::ValBox> receiverBox(cx, callInfo->receiver());
    if (receiverBox->isPrimitive())
        return cx->setExceptionRaised("Cannot define method on primitive.");
    Local<VM::Wobject*> receiver(cx, receiverBox->objectPointer());

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedDefStmtNode> defStmtNode(cx,
        AST::PackedDefStmtNode(pst->data(), stRef->offset()));

    AllocationContext acx = cx->inHatchery();

    // Create the scripted function.
    Local<VM::ScriptedFunction*> func(cx);
    if (!func.setResult(VM::ScriptedFunction::Create(
            acx, pst, stRef->offset(), callInfo->callerScope(), false)))
    {
        return ErrorVal();
    }

    // Bind the name to the function.
    Local<VM::Box> funcnameBox(cx, pst->getConstant(defStmtNode->nameCid()));
    WH_ASSERT(funcnameBox->isPointer());
    WH_ASSERT(funcnameBox->pointer<HeapThing>()->header().isFormat_String());
    Local<VM::String*> funcname(cx, funcnameBox->pointer<VM::String>());
    Local<VM::PropertyDescriptor> descr(cx, VM::PropertyDescriptor(func.get()));
    if (!VM::Wobject::DefineProperty(acx, receiver, funcname, descr))
        return ErrorVal();
    
    return VM::ControlFlow::Void();
}

IMPL_LIFT_FN_(VarStmt)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@VarStmt called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::VarStmt);

    Local<VM::ValBox> receiverBox(cx, callInfo->receiver());
    if (receiverBox->isPrimitive())
        return cx->setExceptionRaised("Cannot define var on primitive.");
    Local<VM::Wobject*> receiver(cx, receiverBox->objectPointer());

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedVarStmtNode> varStmtNode(cx,
        AST::PackedVarStmtNode(pst->data(), stRef->offset()));
    Local<VM::Box> varnameBox(cx);
    Local<VM::String*> varname(cx);
    Local<VM::ValBox> varvalBox(cx);

    AllocationContext acx = cx->inHatchery();

    // Iterate through all bindings.
    SpewInterpNote("Lift_VarStmt: Defining %u vars!",
                   unsigned(varStmtNode->numBindings()));
    for (uint32_t i = 0; i < varStmtNode->numBindings(); i++) {
        varnameBox = pst->getConstant(varStmtNode->varnameCid(i));
        WH_ASSERT(varnameBox->isPointer());
        WH_ASSERT(varnameBox->pointer<HeapThing>()->header().isFormat_String());
        varname = varnameBox->pointer<VM::String>();
        
        if (varStmtNode->hasVarexpr(i)) {
            SpewInterpNote("Lift_VarStmt var %d evaluating initial value!",
                           unsigned(i));
            Local<AST::PackedBaseNode> exprNode(cx, varStmtNode->varexpr(i));
            VM::ControlFlow varExprFlow =
                InterpretSyntax(cx, callInfo->callerScope(), pst,
                                exprNode->offset());

            // The underlying expression can return a value, error out,
            // or throw an exception.  It should never conclude with
            // a void control flow, or a return control flow.
            WH_ASSERT(varExprFlow.isExpressionResult());
            if (!varExprFlow.isValue())
                return varExprFlow;
            varvalBox = varExprFlow.value();

        } else {
            varvalBox = VM::ValBox::Undefined();
        }

        WH_ASSERT_IF(varvalBox->isPointer(),
            VM::Wobject::IsWobject(varvalBox->pointer<HeapThing>()));

        // Bind the name and value onto the receiver.
        Local<VM::PropertyDescriptor> descr(cx,
            VM::PropertyDescriptor(varvalBox));
        if (!VM::Wobject::DefineProperty(acx, receiver, varname, descr))
            return ErrorVal();
    }

    return VM::ControlFlow::Void();
}

IMPL_LIFT_FN_(ParenExpr)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@ParenExpr called with wrong number of arguments.");
    }

    WH_ASSERT(args.get(0).nodeType() == AST::ParenExpr);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedParenExprNode> parenExpr(cx,
        AST::PackedParenExprNode(pst->data(), stRef->offset()));

    Local<AST::PackedBaseNode> subexprNode(cx, parenExpr->subexpr());
    VM::ControlFlow exprFlow =
        InterpretSyntax(cx, callInfo->callerScope(), pst,
                        subexprNode->offset());
    WH_ASSERT(exprFlow.isExpressionResult());
    return exprFlow;
}

IMPL_LIFT_FN_(NameExpr)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@NameExpr called with wrong number of arguments.");
    }
    SpewInterpNote("Lift_NameExpr: Looking up name!");

    WH_ASSERT(args.get(0).nodeType() == AST::NameExpr);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedNameExprNode> nameExpr(cx,
        AST::PackedNameExprNode(pst->data(), stRef->offset()));

    // Get the scope to look up on.
    Local<VM::Wobject*> scopeObj(cx,
        callInfo->callerScope().convertTo<VM::Wobject*>());

    // Get the constant name to look up.
    Local<VM::Box> nameBox(cx, pst->getConstant(nameExpr->nameCid()));
    WH_ASSERT(nameBox->isPointer());
    WH_ASSERT(nameBox->pointer<HeapThing>()->header().isFormat_String());
    Local<VM::String*> name(cx, nameBox->pointer<VM::String>());

    // Do the lookup.
    Local<VM::LookupState*> lookupState(cx);
    Local<VM::PropertyDescriptor> propDesc(cx);

    VM::ControlFlow propFlow = GetObjectProperty(cx, scopeObj, name);
    WH_ASSERT(propFlow.isExpressionResult() || propFlow.isVoid());
    if (propFlow.isValue())
        return VM::ControlFlow::Value(propFlow.value());

    // Void control flow means that property was not found.
    // Throw an error.
    if (propFlow.isVoid())
        return cx->setExceptionRaised("Name not found", name.get());

    return propFlow;
}

IMPL_LIFT_FN_(IntegerExpr)
{
    if (args.length() != 1) {
        return cx->setExceptionRaised(
            "@IntegerExpr called with wrong number of arguments.");
    }
    SpewInterpNote("Lift_IntegerExpr: Returning integer!");

    WH_ASSERT(args.get(0).nodeType() == AST::IntegerExpr);

    Local<VM::SyntaxTreeRef> stRef(cx, args.get(0));
    Local<VM::PackedSyntaxTree*> pst(cx, stRef->pst());
    Local<AST::PackedIntegerExprNode> intExpr(cx,
        AST::PackedIntegerExprNode(pst->data(), stRef->offset()));

    // Make an integer box and return it.
    return VM::ControlFlow::Value(VM::ValBox::Integer(intExpr->value()));
}


} // namespace Interp
} // namespace Whisper