#ifndef WHISPER__GC__FORMATS_HPP
#define WHISPER__GC__FORMATS_HPP


#define WHISPER_DEFN_GC_STACK_FORMATS(_) \
    _(NativeCallInfo)           \
    _(SyntaxNodeRef)            \
    _(PropertyName)             \
    _(PropertyDescriptor)       \
    _(Box)                      \
    _(ValBox)                   \
    _(PackedSyntaxElement)      \
    _(PackedWriter)             \
    _(PackedReader)             \
    _(HeapPointer)              \
    _(ControlFlow)


#define WHISPER_DEFN_GC_HEAP_FORMATS(_) \
    _(UInt8Array)               \
    _(UInt16Array)              \
    _(UInt32Array)              \
    _(UInt64Array)              \
    _(Int8Array)                \
    _(Int16Array)               \
    _(Int32Array)               \
    _(Int64Array)               \
    _(FloatArray)               \
    _(DoubleArray)              \
    _(String)                   \
    _(BoxArray)                 \
    _(ValBoxArray)              \
    _(RuntimeState)             \
    _(PackedSyntaxTree)         \
    _(SyntaxTreeFragment)       \
    _(HeapPointerArray)         \
    _(SourceFile)               \
    _(PropertyDict)             \
    _(PlainObject)              \
    _(CallScope)                \
    _(ModuleScope)              \
    _(GlobalScope)              \
    _(LookupSeenObjects)        \
    _(LookupNode)               \
    _(LookupState)              \
    _(NativeFunction)           \
    _(ScriptedFunction)         \
    _(FunctionObject)           \
    _(Frame)


#endif // WHISPER__GC__FORMATS_HPP
