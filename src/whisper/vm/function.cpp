
#include "runtime_inlines.hpp"
#include "vm/core.hpp"
#include "vm/function.hpp"

namespace Whisper {
namespace VM {


bool
Function::isApplicative() const
{
    if (isNative())
        return asNative()->isApplicative();

    if (isScripted())
        return asScripted()->isApplicative();

    WH_UNREACHABLE("Unknown function type.");
    return false;
}


/* static */ Result<NativeFunction *>
NativeFunction::Create(AllocationContext acx, NativeApplicativeFuncPtr app)
{
    return acx.create<NativeFunction>(app);
}

/* static */ Result<NativeFunction *>
NativeFunction::Create(AllocationContext acx, NativeOperativeFuncPtr oper)
{
    return acx.create<NativeFunction>(oper);
}


/* static */ Result<ScriptedFunction *>
ScriptedFunction::Create(AllocationContext acx,
                         Handle<SyntaxTreeFragment *> definition,
                         Handle<ScopeObject *> scopeChain,
                         bool isOperative)
{
    return acx.create<ScriptedFunction>(definition, scopeChain, isOperative);
}


/* static */ Result<FunctionObject *>
FunctionObject::Create(AllocationContext acx, Handle<Function *> func)
{
    // Allocate empty array of delegates.
    Local<Array<Wobject *> *> delegates(acx);
    if (!delegates.setResult(Array<Wobject *>::CreateEmpty(acx)))
        return ErrorVal();

    // Allocate a dictionary.
    Local<PropertyDict *> props(acx);
    if (!props.setResult(PropertyDict::Create(acx, InitialPropertyCapacity)))
        return ErrorVal();

    return acx.create<FunctionObject>(delegates.handle(), props.handle(),
                                      func);
}

/* static */ void
FunctionObject::GetDelegates(ThreadContext *cx,
                             Handle<FunctionObject *> obj,
                             MutHandle<Array<Wobject *> *> delegatesOut)
{
    HashObject::GetDelegates(cx,
        Handle<HashObject *>::Convert(obj),
        delegatesOut);
}

/* static */ bool
FunctionObject::GetProperty(ThreadContext *cx,
                            Handle<FunctionObject *> obj,
                            Handle<String *> name,
                            MutHandle<PropertyDescriptor> result)
{
    return HashObject::GetProperty(cx,
        Handle<HashObject *>::Convert(obj),
        name, result);
}

/* static */ OkResult
FunctionObject::DefineProperty(ThreadContext *cx,
                               Handle<FunctionObject *> obj,
                               Handle<String *> name,
                               Handle<PropertyDescriptor> defn)
{
    return HashObject::DefineProperty(cx,
        Handle<HashObject *>::Convert(obj),
        name, defn);
}


} // namespace VM
} // namespace Whisper
