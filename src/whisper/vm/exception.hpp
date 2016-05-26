#ifndef WHISPER__VM__EXCEPTION_HPP
#define WHISPER__VM__EXCEPTION_HPP


#include "vm/core.hpp"
#include "vm/predeclare.hpp"
#include "vm/box.hpp"

namespace Whisper {
namespace VM {


//
// Base class for exceptions.
//
class Exception
{
  protected:
    Exception() {}

  public:
    bool isInternalException() const {
        return HeapThing::From(this)->isInternalException();
    }

    InternalException* toInternalException() {
        WH_ASSERT(isInternalException());
        return reinterpret_cast<InternalException*>(this);
    }

    size_t snprint(char* buf, size_t n);
};

//
// An internal has a string message, and zero or more Box arguments
// indicating the exception data.
//
class InternalException : public Exception
{
  friend class TraceTraits<InternalException>;
  private:
    char const* message_;
    uint32_t numArguments_;
    HeapField<Box> arguments_[0];

  public:
    InternalException(char const* message,
                      uint32_t numArguments,
                      Box const* arguments)
      : message_(message),
        numArguments_(numArguments)
    {
        for (uint32_t i = 0; i < numArguments; i++) {
            arguments_[i].init(arguments[i], this);
        }
    }

    static uint32_t CalculateSize(uint32_t numArguments) {
        return sizeof(InternalException) +
                (numArguments * sizeof(HeapField<Box>));
    }

    static Result<InternalException*> Create(AllocationContext acx,
                                             char const* message,
                                             ArrayHandle<Box> const& args);

    static Result<InternalException*> Create(AllocationContext acx,
                                             char const* message)
    {
        return Create(acx, message, ArrayHandle<Box>::Empty());
    }

    template <typename T>
    static Result<InternalException*> Create(AllocationContext acx,
                                             char const* message,
                                             Handle<T*> arg)
    {
        Local<Box> args(acx, Box::Pointer(arg.get()));
        return Create(acx, message, ArrayHandle<Box>(args));
    }

    static Result<InternalException*> Create(AllocationContext acx,
                                             char const* message,
                                             Handle<Box> arg)
    {
        return Create(acx, message, ArrayHandle<Box>(arg));
    }

    static Result<InternalException*> Create(AllocationContext acx,
                                             char const* message,
                                             Handle<ValBox> arg)
    {
        Local<Box> args(acx, arg.get());
        return Create(acx, message, ArrayHandle<Box>(args));
    }

    char const* message() const {
        return message_;
    }

    uint32_t numArguments() const {
        return numArguments_;
    }

    Box const& argument(uint32_t argNo) const {
        WH_ASSERT(argNo < numArguments_);
        return arguments_[argNo];
    }

    size_t snprint(char* buf, size_t n);
};


} // namespace VM


//
// GC Specializations
//

template <>
struct TraceTraits<VM::Exception>
  : public UntracedTraceTraits<VM::Exception>
{};

template <>
struct TraceTraits<VM::InternalException>
{
    TraceTraits() = delete;

    static constexpr bool Specialized = true;
    static constexpr bool IsLeaf = false;

    template <typename Scanner>
    static void Scan(Scanner& scanner, VM::InternalException const& obj,
                     void const* start, void const* end)
    {
        TraceTraits<VM::Exception>::Scan<Scanner>(scanner, obj, start, end);
        for (uint32_t i = 0; i < obj.numArguments_; i++) {
            obj.arguments_[i].scan(scanner, start, end);
        }
    }

    template <typename Updater>
    static void Update(Updater& updater, VM::InternalException& obj,
                       void const* start, void const* end)
    {
        TraceTraits<VM::Exception>::Update<Updater>(updater, obj, start, end);
        for (uint32_t i = 0; i < obj.numArguments_; i++) {
            obj.arguments_[i].update(updater, start, end);
        }
    }
};


} // namespace Whisper


#endif // WHISPER__VM__EXCEPTION_HPP