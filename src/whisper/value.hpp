#ifndef WHISPER__VALUE_HPP
#define WHISPER__VALUE_HPP

#include <limits>
#include <type_traits>
#include "common.hpp"
#include "debug.hpp"

namespace Whisper {


//
// Value
//
// A value is a 64-bit integer value, which can represent pointers to
// heap things, as well as immediate values of various types.
//
// The high 4 bits of a value are used for a type tag.
//
// The notable aspect of the value boxing format is its treatment of
// doubles.  A value cannot immediately represent all doubles, but it
// can represent a range of common double values as immediates.  Other
// double values must be heap allocated.
//
// Tag types:
//  Object          - pointer to object.
//  Null            - null value (low 60 bits are ignored).
//  Undef           - undefined value (low 60 bits are ignored).
//  Boolean         - boolean value (low bit holds value).
//  HeapString      - pointer to string on heap.
//  ImmString8      - immediate 8-bit character string.
//  ImmString16     - immediate 16-bit character string.
//  ImmDoubleLow    - immediate doubles (1.0 > value && -1.0 < value)
//  ImmDoubleHigh   - immediate doubles (1.0 <= value || -1.0 >= value)
//  ImmDoubleX      - immediate NaN, Inf, -Inf, and -0 (low 2 bits hold value).
//  HeapDouble      - heap double.
//  Int32           - 32-bit integer.
//  Magic           - magic value.
//
// Object:
//  0000-W000 PPPP-PPPP PPPP-PPPP ... PPPP-PPPP     - Object pointer
//  0000-W001 PPPP-PPPP PPPP-PPPP ... PPPP-PPPP     - Foreign pointer
//
//      W is the weak bit.  If W=1, the reference is weak.
//
// Null & Undefined:
//  0001-0000 0000-0000 0000-0000 ... 0000-0000     - Null value
//  0010-0000 0000-0000 0000-0000 ... 0000-0000     - Undefined value
//
// Boolean:
//  0011-0000 0000-0000 0000-0000 ... 0000-000V     - Boolean value
//
// HeapString & ImmString8 & ImmString16:
//  0100-W000 PPPP-PPPP PPPP-PPPP ... PPPP-PPPP     - HeapString pointer
//  0101-0LLL AAAA-AAAA BBBB-BBBB ... GGGG-GGGG     - Immediate 8-bit string
//  0110-0000 0000-0000 AAAA-AAAA ... CCCC-CCCC     - Immediate 16-bit string
//
//      In a heap string reference, W is the weak bit.  If W=1, the
//      reference is weak.
//
//      Immediate strings come in two variants.  The first variant can
//      represent all strings of length up to 7 containing 8-bit chars
//      only.  The second variant can represent all strings of length up
//      to 3 containing 16-bit chars.
//
//      Characters are stored from high to low.
//
//      This representation allows a lexical comparison of 8-bit immediate
//      strings with other 8-bit immediate strings, and of 16-bit immediate
//      strings with other 16-bit immediate strings, by simply rotating
//      the value left by 8 bits and doing an integer compare.
//
// ImmDoubleLow, ImmDoubleHigh, ImmDoubleX, and HeapDouble:
//  0111-EEEE EEEM-MMMM MMMM-MMMM ... MMMM-MMMS     - ImmDoubleLow
//  1000-EEEE EEEM-MMMM MMMM-MMMM ... MMMM-MMMS     - ImmDoubleHigh
//  1001-0000 0000-0000 0000-0000 ... 0000-00XX     - ImmDoubleX
//  1010-W000 PPPP-PPPP PPPP-PPPP ... PPPP-PPPP     - HeapDouble pointer
//
//      ImmDoubleLo and ImmDoubleHi are "regular" double values which
//      are immediately representable.  The only requirement is that
//       they have an exponent value in the range [128, -127].
//
//      The boxed representation is achieved by rotating the native double
//      representation left by 1 bit.  Given the exponent requirement,
//      this naturally ensures that the top 4 bits of the boxed representation
//      are either 0x7 or 0x8.
//
//      ImmDoubleX can represent -0.0, NaN, Inf, and -Inf.  Each of these
//      options is enumerated in the low 2 bits (XX):
//          00 => -0.0, 
//          01 => NaN
//          10 => Inf
//          11 => -Inf
//
//      In a heap double reference, W is the weak bit.  If W=1, the
//      reference is weak.
//
// Int32:
//  1100-0000 0000-0000 0000-0000 ... IIII-IIII     - Int32 value.
//
//      Only the low 32 bits are used for integer values.
//
// Magic:
//  1101-MMMM MMMM-MMMM MMMM-MMMM ... MMMM-MMMM     - Magic value.
//
//      The low 60 bits can be used to store any required magic value
//      info.
//
// UNUSED:
//  1110-**** ****-**** ****-**** ... ****-****
//  1111-**** ****-**** ****-**** ... ****-****
//
//

namespace VM {
    class UntypedHeapThing;
    class Object;
    class HeapString;
    class HeapDouble;
}

// Type tag enumeration for values.
enum class ValueTag : uint8_t
{
    Object          = 0x0,

    Null            = 0x1,
    Undefined       = 0x2,
    Boolean         = 0x3,

    HeapString      = 0x4,
    ImmString8      = 0x5,
    ImmString16     = 0x6,

    ImmDoubleLow    = 0x7,
    ImmDoubleHigh   = 0x8,
    ImmDoubleX      = 0x9,
    HeapDouble      = 0xA,

    UNUSED_B        = 0xB,

    Int32           = 0xC,
    Magic           = 0xD,

    UNUSED_E        = 0xE,
    UNUSED_F        = 0xF
};

inline uint8_t constexpr ValueTagNumber(ValueTag type) {
    return static_cast<uint8_t>(type);
}

inline bool IsValidValueTag(ValueTag tag) {
    return tag == ValueTag::Object        || tag == ValueTag::Null         ||
           tag == ValueTag::Undefined     || tag == ValueTag::Boolean      ||
           tag == ValueTag::HeapString    || tag == ValueTag::ImmString8   ||
           tag == ValueTag::ImmString16   || tag == ValueTag::ImmDoubleLow ||
           tag == ValueTag::ImmDoubleHigh || tag == ValueTag::ImmDoubleX   ||
           tag == ValueTag::HeapDouble    || tag == ValueTag::Int32        ||
           tag == ValueTag::Magic;
}


enum class Magic : uint32_t
{
    INVALID         = 0,
    LIMIT
};

class Value
{
  public:
    // Constants relating to tag bits.
    static constexpr unsigned TagBits = 4;
    static constexpr unsigned TagShift = 60;
    static constexpr uint64_t TagMaskLow = 0xFu;
    static constexpr uint64_t TagMaskHigh = TagMaskLow << TagShift;

    // High 16 bits of pointer values do not contain address bits.
    static constexpr unsigned PtrHighBits = 8;
    static constexpr unsigned PtrTypeShift = 56;
    static constexpr unsigned PtrTypeMask = 0xFu;
    static constexpr unsigned PtrType_Native = 0x0u;
    static constexpr unsigned PtrType_Foreign = 0x1u;

    // Constants relating to string bits.
    static constexpr unsigned ImmString8MaxLength = 7;
    static constexpr unsigned ImmString8LengthShift = 56;
    static constexpr uint64_t ImmString8LengthMaskLow = 0x7;
    static constexpr uint64_t ImmString8LengthMaskHigh =
        ImmString8LengthMaskLow << ImmString8LengthShift;

    static constexpr unsigned ImmString16MaxLength = 3;
    static constexpr unsigned ImmString16LengthShift = 56;
    static constexpr uint64_t ImmString16LengthMaskLow = 0x3;
    static constexpr uint64_t ImmString16LengthMaskHigh =
        ImmString16LengthMaskLow << ImmString16LengthShift;

    // The weak bit is the same bit across all pointer-type values.
    static constexpr unsigned WeakBit = 59;
    static constexpr uint64_t WeakMask = UInt64(1) << WeakBit;

    // Bounds for representable simple ImmDouble:
    // In order of: [PosMax, PosMin, NegMax, NegMin]
    //           SEEE-EEEE EEEE-MMMM MMMM-MMMM .... MMMM-MMMM
    // PosMax: 0100-0111 1111-1111 1111-1111 .... 1111-1111
    // PosMin: 0011-1000 0000-0000 0000-0000 .... 0000-0000
    // NegMax: 1011-1000 0000-0000 0000-0000 .... 0000-0000
    // MegMin: 1100-0111 1111-1111 1111-1111 .... 1111-1111
    template <bool NEG, unsigned EXP, bool MANT>
    struct GenerateDouble_ {
        static constexpr uint64_t Value =
            (UInt64(NEG) << 63) |
            (UInt64(EXP) << 52) |
            (MANT ? ((UInt64(MANT) << 52) - 1u) : 0);
    };
    static constexpr uint64_t ImmDoublePosMax =
        GenerateDouble_<false, 0x47f, true>::Value;
    static constexpr uint64_t ImmDoublePosMin =
        GenerateDouble_<false, 0x380, false>::Value;
    static constexpr uint64_t ImmDoubleNegMax =
        GenerateDouble_<true, 0x380, false>::Value;
    static constexpr uint64_t ImmDoubleNegMin =
        GenerateDouble_<true, 0x47f, true>::Value;

    // Immediate values for special doubles.
    static constexpr unsigned NegZeroVal = 0x0;
    static constexpr unsigned NaNVal     = 0x1;
    static constexpr unsigned PosInfVal  = 0x2;
    static constexpr unsigned NegInfVal  = 0x3;

    // Invalid value is a null-pointer
    static constexpr uint64_t Invalid = 0u;

  protected:
    uint64_t tagged_;

  public:
    Value() : tagged_(Invalid) {}

  protected:
    // Raw uint64_t constructor is private.
    explicit Value(uint64_t tagged) : tagged_(tagged) {}

    bool checkTag(ValueTag tag) const {
        return (tagged_ >> TagShift) == ValueTagNumber(tag);
    }

    template <typename T=void>
    T *getPtr() const {
        WH_ASSERT(isObject() || isHeapString() || isHeapDouble());
        return reinterpret_cast<T *>(
                (static_cast<int64_t>(tagged_ << PtrHighBits)) >> PtrHighBits);
    }

    uint64_t removeTag() const {
        return (tagged_ << TagBits) >> TagBits;
    }

    template <typename T>
    static Value MakePtr(ValueTag tag, T *ptr) {
        WH_ASSERT(tag == ValueTag::Object || tag == ValueTag::HeapString ||
                  tag == ValueTag::HeapDouble);
        uint64_t ptrval = reinterpret_cast<uint64_t>(ptr);
        ptrval &= ~TagMaskHigh;
        ptrval |= UInt64(tag) << TagShift;
        return Value(ptrval);
    }

    static Value MakeTag(ValueTag tag) {
        WH_ASSERT(IsValidValueTag(tag));
        return Value(UInt64(tag) << TagShift);
    }

    static Value MakeTagValue(ValueTag tag, uint64_t ival) {
        WH_ASSERT(IsValidValueTag(tag));
        WH_ASSERT(ival <= (~UInt64(0) >> TagBits));
        return Value(UInt64(tag) << TagShift);
    }

  public:

#if defined(ENABLE_DEBUG)
    bool isValid() const {
        // If heap thing, pointer can't be zero.
        if (((tagged_ & 0x3) == 0) && ((tagged_ >> 4) == 0))
            return false;

        // Otherwise, check for unused tag range.
        unsigned tag = tagged_ & 0xFu;
        if (tag >= 0x8u && tag <= 0xBu)
            return false;

        return true;
    }
#endif // defined(ENABLE_DEBUG)

    //
    // Checker methods
    //

    bool isObject() const {
        return checkTag(ValueTag::Object);
    }

    bool isNativeObject() const {
        return isObject() &&
               ((tagged_ >> PtrTypeShift) & PtrTypeMask) == PtrType_Native;
    }

    template <typename T>
    bool isNativeObjectOf() const {
        static_assert(std::is_base_of<VM::UntypedHeapThing, T>::value,
                      "Type is not a heap thing.");
        if (!isNativeObject())
            return false;
        return getPtr<T>()->type() == T::Type;
    }

    bool isForeignObject() const {
        return isObject() &&
               ((tagged_ >> PtrTypeShift) & PtrTypeMask) == PtrType_Foreign;
    }

    bool isNull() const {
        return checkTag(ValueTag::Null);
    }

    bool isUndefined() const {
        return checkTag(ValueTag::Undefined);
    }

    bool isBoolean() const {
        return checkTag(ValueTag::Boolean);
    }

    bool isHeapString() const {
        return checkTag(ValueTag::HeapString);
    }

    bool isImmString8() const {
        return checkTag(ValueTag::ImmString8);
    }

    bool isImmString16() const {
        return checkTag(ValueTag::ImmString16);
    }

    bool isImmDoubleLow() const {
        return checkTag(ValueTag::ImmDoubleLow);
    }

    bool isImmDoubleHigh() const {
        return checkTag(ValueTag::ImmDoubleHigh);
    }

    bool isImmDoubleX() const {
        return checkTag(ValueTag::ImmDoubleX);
    }

    bool isNegZero() const {
        return isImmDoubleX() && (tagged_ & 0xFu) == NegZeroVal;
    }

    bool isNaN() const {
        return isImmDoubleX() && (tagged_ & 0xFu) == NaNVal;
    }

    bool isPosInf() const {
        return isImmDoubleX() && (tagged_ & 0xFu) == PosInfVal;
    }

    bool isNegInf() const {
        return isImmDoubleX() && (tagged_ & 0xFu) == NegInfVal;
    }

    bool isHeapDouble() const {
        return checkTag(ValueTag::HeapDouble);
    }

    bool isInt32() const {
        return checkTag(ValueTag::Int32);
    }

    bool isMagic() const {
        return checkTag(ValueTag::Magic);
    }

    // Helper functions to check combined types.

    bool isString() const {
        return isImmString8() || isImmString16() || isHeapString();
    }

    bool isImmString() const {
        return isImmString8() || isImmString16();
    }

    bool isNumber() const {
        return isImmDoubleLow() || isImmDoubleHigh() || isImmDoubleX() ||
               isHeapDouble()   || isInt32();
    }

    bool isDouble() const {
        return isImmDoubleLow() || isImmDoubleHigh() || isImmDoubleX() ||
               isHeapDouble();
    }

    bool isSpecialImmDouble() const {
       return isImmDoubleLow() || isImmDoubleHigh() || isImmDoubleX();
    }

    bool isRegularImmDouble() const {
       return isImmDoubleLow() || isImmDoubleHigh();
    }

    bool isWeakPointer() const {
        WH_ASSERT(isHeapDouble() || isHeapString() || isObject());
        return tagged_ & WeakMask;
    }

    //
    // Getter methods
    //
    template <typename T=VM::Object>
    T *getNativeObject() const {
        WH_ASSERT(isNativeObject());
        WH_ASSERT(getPtr<T>()->type() == T::Type);
        return getPtr<T>();
    }

    VM::UntypedHeapThing *getAnyNativeObject() const {
        WH_ASSERT(isNativeObject());
        return getPtr<VM::UntypedHeapThing>();
    }

    template <typename T>
    T *getForeignObject() const {
        WH_ASSERT(isForeignObject());
        return getPtr<T>();
    }

    bool getBoolean() const {
        WH_ASSERT(isBoolean());
        return tagged_ & 0x1;
    }

    VM::HeapString *getHeapString() const {
        WH_ASSERT(isHeapString());
        return getPtr<VM::HeapString>();
    }

    unsigned immString8Length() const {
        WH_ASSERT(isImmString8());
        return (tagged_ >> ImmString8LengthShift) & ImmString8LengthMaskLow;
    }

    uint8_t getImmString8Char(unsigned idx) const {
        WH_ASSERT(isImmString8());
        WH_ASSERT(idx < immString8Length());
        return (tagged_ >> (48 - (idx*8))) & 0xFFu;
    }

    template <typename CharT>
    uint32_t readImmString8(CharT *buf) const {
        WH_ASSERT(isImmString8());
        uint32_t length = immString8Length();
        for (uint32_t i = 0; i < length; i++)
            buf[i] = (tagged_ >> (48 - (8 * i))) & 0xFFu;
        return length;
    }

    unsigned immString16Length() const {
        WH_ASSERT(isImmString16());
        return (tagged_ >> ImmString16LengthShift) & ImmString16LengthMaskLow;
    }

    uint16_t getImmString16Char(unsigned idx) const {
        WH_ASSERT(isImmString16());
        WH_ASSERT(idx < immString16Length());
        return (tagged_ >> (32 - (idx*16))) & 0xFFFFu;
    }

    template <typename CharT>
    uint32_t readImmString16(CharT *buf) const {
        WH_ASSERT(isImmString16());
        uint32_t length = immString16Length();
        for (uint32_t i = 0; i < length; i++)
            buf[i] = (tagged_ >> (32 - (16 * i))) & 0xFFu;
        return length;
    }

    unsigned immStringLength() const {
        WH_ASSERT(isImmString());
        return isImmString8() ? immString8Length() : immString16Length();
    }

    uint16_t getImmStringChar(unsigned idx) const {
        WH_ASSERT(isImmString());
        return isImmString8() ? getImmString8Char(idx)
                              : getImmString16Char(idx);
    }

    template <typename CharT>
    unsigned readImmString(CharT *buf) const {
        WH_ASSERT(isImmString());
        return isImmString8() ? readImmString8<CharT>(buf)
                              : readImmString16<CharT>(buf);
    }

    double getImmDoubleHiLoValue() const {
        WH_ASSERT(isImmDoubleHigh() || isImmDoubleLow());
        return IntToDouble(RotateRight(tagged_, 1));
    }

    double getImmDoubleXValue() const {
        WH_ASSERT(isImmDoubleX());
        switch (tagged_) {
          case NaNVal:
            return std::numeric_limits<double>::quiet_NaN();
          case NegZeroVal:
            return -0.0;
          case PosInfVal:
            return std::numeric_limits<double>::infinity();
          case NegInfVal:
            return -std::numeric_limits<double>::infinity();
        }
        WH_UNREACHABLE("Bad special immedate double.");
        return 0.0;
    }

    double getImmDoubleValue() const {
        WH_ASSERT(isSpecialImmDouble());
        return isImmDoubleX() ? getImmDoubleXValue() : getImmDoubleHiLoValue();
    }

    VM::HeapDouble *getHeapDouble() const {
        WH_ASSERT(isHeapDouble());
        return getPtr<VM::HeapDouble>();
    }

    uint64_t getMagicInt() const {
        WH_ASSERT(isMagic());
        return removeTag();
    }

    Magic getMagic() const {
        WH_ASSERT(isMagic());
        return static_cast<Magic>(removeTag());
    }

    int32_t getInt32() const {
        WH_ASSERT(isInt32());
        return static_cast<int32_t>(tagged_);
    }

    //
    // Friend functions
    //
    template <typename T>
    friend Value NativeObjectValue(T *obj);
    template <typename T>
    friend Value WeakNativeObjectValue(T *obj);
    friend Value ForeignObjectValue(void *obj);
    friend Value WeakForeignObjectValue(void *obj);
    friend Value NullValue();
    friend Value UndefinedValue();
    friend Value BooleanValue(bool b);
    friend Value StringValue(VM::HeapString *str);
    template <typename CharT>
    friend Value String8Value(unsigned length, const CharT *data);
    template <typename CharT>
    friend Value String16Value(unsigned length, const CharT *data);
    template <typename CharT>
    friend Value StringValue(unsigned length, const CharT *data);
    friend Value MagicValue(uint64_t val);
    friend Value MagicValue(Magic m);
    friend Value IntegerValue(int32_t i);
    friend Value DoubleValue(VM::HeapDouble *d);
    friend Value NaNValue();
    friend Value NegZeroValue();
    friend Value PosInfValue();
    friend Value NegInfValue();
    friend Value DoubleValue(double d);
};


template <typename T>
inline Value
NativeObjectValue(T *obj) {
    WH_ASSERT(T::Type == obj->type());
    Value val = Value::MakePtr(ValueTag::Object, obj);
    return val;
}

template <typename T>
inline Value
WeakNativeObjectValue(T *obj) {
    Value val = NativeObjectValue<T>(obj);
    val.tagged_ |= Value::WeakMask;
    return val;
}

inline Value
ForeignObjectValue(void *obj) {
    Value val = Value::MakePtr(ValueTag::Object, obj);
    val.tagged_ |= (UInt64(Value::PtrType_Foreign) << Value::PtrTypeShift);
    return val;
}

inline Value
WeakForeignObjectValue(void *obj) {
    Value val = ForeignObjectValue(obj);
    val.tagged_ |= Value::WeakMask;
    return val;
}

inline Value
NullValue() {
    return Value::MakeTag(ValueTag::Null);
}

inline Value
UndefinedValue() {
    return Value::MakeTag(ValueTag::Undefined);
}

inline Value
BooleanValue(bool b) {
    return Value::MakeTagValue(ValueTag::Boolean, b);
}

inline Value
StringValue(VM::HeapString *str) {
    return Value::MakePtr(ValueTag::HeapString, str);
}

template <typename CharT>
inline Value
String8Value(unsigned length, const CharT *data) {
    WH_ASSERT(length < Value::ImmString8MaxLength);
    uint64_t val = length;
    for (unsigned i = 0; i < Value::ImmString8MaxLength; i++) {
        val <<= 8;
        if (i < length) {
            WH_ASSERT(data[i] == data[i] & 0xFFu);
            val |= data[i];
        }
    }
    return Value::MakeTagValue(ValueTag::ImmString8, val);
}

template <typename CharT>
inline Value
String16Value(unsigned length, const CharT *data) {
    WH_ASSERT(length < Value::ImmString16MaxLength);
    uint64_t val = length;
    for (unsigned i = 0; i < Value::ImmString16MaxLength; i++) {
        val <<= 16;
        if (i < length) {
            WH_ASSERT(data[i] == data[i] & 0xFFFFu);
            val |= data[i];
        }
    }
    return Value::MakeTagValue(ValueTag::ImmString16, val);
}

template <typename CharT>
inline Value
StringValue(unsigned length, const CharT *data)
{
    bool make8 = true;
    for (unsigned i = 0; i < length; i++) {
        WH_ASSERT(data[i] == data[i] & 0xFFFFu);
        if (data[i] != data[i] & 0xFFu) {
            make8 = false;
            break;
        }
    }

    return make8 ? String8Value(length, data) : String16Value(length, data);
}

inline Value
DoubleValue(VM::HeapDouble *d) {
    return Value::MakePtr(ValueTag::HeapDouble, d);
}

inline Value
NegZeroValue() {
    return Value::MakeTagValue(ValueTag::ImmDoubleX, UInt64(Value::NegZeroVal));
}

inline Value
NaNValue() {
    return Value::MakeTagValue(ValueTag::ImmDoubleX, UInt64(Value::NaNVal));
}

inline Value
PosInfValue() {
    return Value::MakeTagValue(ValueTag::ImmDoubleX, UInt64(Value::PosInfVal));
}

inline Value
NegInfValue() {
    return Value::MakeTagValue(ValueTag::ImmDoubleX, UInt64(Value::NegInfVal));
}

inline Value
DoubleValue(double dval) {
    uint64_t ival = DoubleToInt(dval);
    WH_ASSERT(((ival <= Value::ImmDoublePosMax) &&
               (ival >= Value::ImmDoublePosMin)) ||
              ((ival <= Value::ImmDoubleNegMax) &&
               (ival >= Value::ImmDoubleNegMin)));
    return Value(RotateLeft(ival, 1));
}

inline Value
MagicValue(uint64_t val) {
    WH_ASSERT((val & Value::TagMaskHigh) == 0);
    return Value::MakeTagValue(ValueTag::Magic, val);
}

inline Value
MagicValue(Magic magic) {
    return Value::MakeTagValue(ValueTag::Magic, UInt64(magic));
}

inline Value
IntegerValue(int32_t ival) {
    // Cast to uint32_t so value doesn't get sign extended when casting
    // to uint64_t
    return Value::MakeTagValue(ValueTag::Int32, UInt32(ival));
}


} // namespace Whisper

#endif // WHISPER__VALUE_HPP
