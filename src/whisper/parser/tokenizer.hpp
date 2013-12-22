#ifndef WHISPER__PARSER__TOKENIZER_HPP
#define WHISPER__PARSER__TOKENIZER_HPP

#include <list>
#include "common.hpp"
#include "debug.hpp"
#include "parser/code_source.hpp"
#include "parser/token_defn.hpp"
#include "allocators.hpp"

//
// The tokenizer parses a code source into a series of tokens.
//

namespace Whisper {


//
// KeywordTable
//
// Keeps an ordered table of keywords which can be used to do quick
// lookups of identifiers which may be keywords.
//
void InitializeKeywordTable();


//
// QuickTokenTable
//
// Maps ascii characters to immediately-returnable tokens.
//
void InitializeQuickTokenTable();


//
// Token
//
// Represents a token.
//
class Token
{
  public:
    enum Type : uint8_t
    {
        INVALID = 0,
#define DEF_ENUM_(tok)   tok,
        WHISPER_DEFN_TOKENS(DEF_ENUM_)
#undef DEF_ENUM_
        LIMIT
    };

    inline static bool IsValidType(Type type) {
        return (type > INVALID) && (type < LIMIT);
    }

    inline static bool IsKeywordType(Type type) {
        return (type >= WHISPER_FIRST_KEYWORD_TOKEN) &&
               (type <= WHISPER_LAST_KEYWORD_TOKEN);
    }

    static const char *TypeString(Type type);

    // The flags enum allows annotating a token with
    // particular flags.  Different flags may use the same
    // bits, but any two flags which may be used together
    // must use different bits.
    enum Flags : uint16_t
    {
        NoFlags                 = 0x0000u,

        // Integer flags.
        Int_BinPrefix           = 0x0001u,
        Int_OctPrefix           = 0x0002u,
        Int_DecPrefix           = 0x0004u,
        Int_HexPrefix           = 0x0008u
    };

  protected:
    Type type_ = INVALID;
    Flags flags_ = NoFlags;
    uint32_t offset_ = 0;
    uint32_t length_ = 0;
    uint32_t startLine_ = 0;
    uint32_t startLineOffset_ = 0;
    uint32_t endLine_ = 0;
    uint32_t endLineOffset_ = 0;
    bool maybeKeyword_ = false;

    // Tokens returned from the tokenizer are actually references to
    // a repeatedly used token contained within the tokenizer.
    // These debug variables track whether a token returned by
    // readToken() has been properly used (via a copy to another Token
    // value, or an explicit call to debug_markUsed()), before the next call
    // to readToken().
    mutable DebugVal<bool> debug_used_;
    mutable DebugVal<bool> debug_pushedBack_;

  public:
    Token() : debug_used_(true), debug_pushedBack_(false) {}

    Token(Type type, Flags flags, uint32_t offset, uint32_t length,
          uint32_t startLine, uint32_t startLineOffset,
          uint32_t endLine, uint32_t endLineOffset)
      : type_(type), flags_(flags), offset_(offset), length_(length),
        startLine_(startLine), startLineOffset_(startLineOffset),
        endLine_(endLine), endLineOffset_(endLineOffset),
        debug_used_(false), debug_pushedBack_(false)
    {}

    Token(Type type, uint32_t offset, uint32_t length,
          uint32_t startLine, uint32_t startLineOffset,
          uint32_t endLine, uint32_t endLineOffset)
      : type_(type), offset_(offset), length_(length),
        startLine_(startLine), startLineOffset_(startLineOffset),
        endLine_(endLine), endLineOffset_(endLineOffset),
        debug_used_(false), debug_pushedBack_(false)
    {}

    Token(const Token &other)
      : type_(other.type_), flags_(other.flags_),
        offset_(other.offset_), length_(other.length_),
        startLine_(other.startLine_), startLineOffset_(other.startLineOffset_),
        endLine_(other.endLine_), endLineOffset_(other.endLineOffset_),
        debug_used_(false), debug_pushedBack_(false)
    {
        WH_ASSERT(other.debug_pushedBack_ == false);
        other.debug_used_ = true;
    }

    enum PreserveDebugUsed_ {
        PreserveDebugUsed
    };
    Token(const Token &other, PreserveDebugUsed_ preserve)
      : type_(other.type_), flags_(other.flags_),
        offset_(other.offset_), length_(other.length_),
        startLine_(other.startLine_), startLineOffset_(other.startLineOffset_),
        endLine_(other.endLine_), endLineOffset_(other.endLineOffset_),
        debug_used_(other.debug_used_),
        debug_pushedBack_(other.debug_pushedBack_)
    {
    }

    Token &operator =(const Token &other)
    {
        type_ = other.type_;
        offset_ = other.offset_;
        flags_ = other.flags_;
        length_ = other.length_;
        startLine_ = other.startLine_;
        startLineOffset_ = other.startLineOffset_;
        endLine_ = other.endLine_;
        endLineOffset_ = other.endLineOffset_;
        maybeKeyword_ = other.maybeKeyword_;
        debug_used_ = other.debug_used_;
        debug_pushedBack_ = other.debug_pushedBack_;
        other.debug_used_ = true;
        return *this;
    }

#if defined(ENABLE_DEBUG)
    inline bool isValid() const {
        return (type_ > INVALID) && (type_ < LIMIT);
    }
#endif

    inline bool isINVALID() const {
        return type_ == INVALID;
    }

    inline Type type() const {
        return type_;
    }

    inline Flags flags() const {
        return flags_;
    }
    inline bool hasFlag(Flags flag) const {
        return flags_ & flag;
    }

    inline const char *typeString() const {
        return TypeString(type_);
    }

    inline uint32_t offset() const {
        return offset_;
    }
    inline uint32_t length() const {
        return length_;
    }
    inline uint32_t endOffset() const {
        return offset_ + length_;
    }

    inline uint32_t startLine() const {
        return startLine_;
    }
    inline uint32_t startLineOffset() const {
        return startLineOffset_;
    }

    inline uint32_t endLine() const {
        return endLine_;
    }
    inline uint32_t endLineOffset() const {
        return endLineOffset_;
    }

    inline const uint8_t *text(const CodeSource &src) const {
        return src.data() + offset_;
    }

    // Define type check methods
#define DEF_CHECKER_(tok) \
    inline bool is##tok() const { \
        return type_ == tok; \
    }
    WHISPER_DEFN_TOKENS(DEF_CHECKER_)
#undef DEF_CHECKER_

    inline bool isKeyword() const {
        return IsKeywordType(type_);
    }

    // explicitly mark this token as being used.
    // This is a no-op in production code.
    inline void debug_markUsed() const {
        debug_used_ = true;
    }

    inline bool debug_isUsed() const {
        return debug_used_;
    }
    inline void debug_clearUsed() const {
        debug_used_ = false;
    }
    inline bool debug_isPushedBack() const {
        return debug_pushedBack_;
    }
    inline void debug_markPushedBack() const {
        debug_pushedBack_ = true;
    }
    inline void debug_clearPushedBack() const {
        debug_pushedBack_ = false;
    }
};


//
// TypedToken
//
// A typed token class that only instantiates from tokens of the
// matching type.
//
template <Token::Type... TYPES>
class TypedToken : public Token
{
  private:
    template <Token::Type TP>
    inline static bool CheckType(Token::Type tp) {
        return tp == TP;
    }
    template <Token::Type TP1, Token::Type TP2, Token::Type... TPS>
    inline static bool CheckType(Token::Type tp) {
        return tp == TP1 || CheckType<TP2, TPS...>(tp);
    }

  public:
    inline explicit TypedToken(const Token &token)
      : Token(token)
    {
        WH_ASSERT(CheckType<TYPES...>(type_) || token.isINVALID());
    }

    inline TypedToken() : Token() {}
};

#define DEF_TYPEDEF_(tok)   typedef TypedToken<Token::tok> tok##Token;
        WHISPER_DEFN_TOKENS(DEF_TYPEDEF_)
#undef DEF_TYPEDEF_

typedef TypedToken<Token::PublicKeyword, Token::PrivateKeyword>
        VisibilityToken;

//
// Tokenizer
//
// Parses tokens from a source stream.
//
class TokenizerError {
  friend class Tokenizer;
  private:
    inline TokenizerError() {}
};

class TokenizerMark {
  private:
    uint32_t position_;
    uint32_t line_;
    uint32_t lineOffset_;
    bool pushedBackToken_;
    Token tok_;

  public:
    inline TokenizerMark(uint32_t position,
                         uint32_t line,
                         uint32_t lineOffset,
                         bool pushedBackToken,
                         const Token &tok)
      : position_(position),
        line_(line),
        lineOffset_(lineOffset),
        pushedBackToken_(pushedBackToken),
        tok_(tok, Token::PreserveDebugUsed)
    {}

    inline uint32_t position() const {
        return position_;
    }

    inline uint32_t line() const {
        return line_;
    }

    inline uint32_t lineOffset() const {
        return lineOffset_;
    }

    inline bool pushedBackToken() const {
        return pushedBackToken_;
    }

    inline const Token &token() const {
        return tok_;
    }
};

class Tokenizer
{
  private:
    STLBumpAllocator<uint8_t> allocator_;
    CodeSource &source_;
    SourceStream stream_;
    Token tok_;

    // Parsing state.
    uint32_t line_ = 0;
    const uint8_t *lineStart_ = 0;

    // Current token state.
    const uint8_t *tokStart_ = nullptr;
    uint32_t tokStartLine_ = 0;
    uint32_t tokStartLineOffset_ = 0;

    // Error message.
    const char *error_ = nullptr;

    // Flag indicating pushed-back token.
    bool pushedBackToken_ = false;

  public:
    Tokenizer(const STLBumpAllocator<uint8_t> &allocator, CodeSource &source)
      : allocator_(allocator),
        source_(source),
        stream_(source_),
        tok_()
    {
        // Mark the initial token used.
        tok_.debug_markUsed();
    }

    inline ~Tokenizer() {}

    const STLBumpAllocator<uint8_t> &allocator() const {
        return allocator_;
    }

    inline CodeSource &source() const {
        return source_;
    }

    inline uint32_t line() const {
        return line_;
    }

    TokenizerMark mark() const;
    void gotoMark(const TokenizerMark &mark);
    Token getAutomaticSemicolon() const;
    void pushBackLastToken();

    inline bool hasError() const {
        return error_ != nullptr;
    }
    inline const char *error() const {
        WH_ASSERT(hasError());
        return error_;
    }

    const Token &readTokenImpl();
    const Token &readToken();
    void rewindToToken(const Token &tok);
    void advancePastToken(const Token &tok);

  private:
    // Token parsing.
    const Token &readWhitespace();
    const Token &readLineTerminatorSequence(unic_t ch);
    const Token &readMultiLineComment();
    const Token &readSingleLineComment();

    const Token &readIdentifier(unic_t firstChar);
    const Token &readIdentifierName();

    // Consume a unicode escape sequence.
    void consumeUnicodeEscapeSequence();

    const Token &readNumericLiteral(bool startsWithZero);
    const Token &readBinIntegerLiteral();
    const Token &readOctIntegerLiteral();
    const Token &readDecIntegerLiteral();
    const Token &readHexIntegerLiteral();

    // Emit methods.
    const Token &emitToken(Token::Type type, Token::Flags flags);
    inline const Token &emitToken(Token::Type type) {
        return emitToken(type, Token::NoFlags);
    }
    const Token &emitError(const char *msg);

    // Token tracking during parsing.
    inline void startToken() {
        tokStart_ = stream_.cursor();
        tokStartLine_ = line_;
        tokStartLineOffset_ = tokStart_ - lineStart_;
    }

    inline void startNewLine() {
        line_++;
        lineStart_ = stream_.cursor();
    }

    // Character reading.
    static constexpr unic_t NonAscii = -1;
    static constexpr unic_t End = -2;

    inline unic_t readAsciiChar() {
        if (stream_.atEnd())
            return End;

        uint8_t b = stream_.readByte();
        if (b <= 0x7Fu)
            return b;

        return NonAscii;
    }
    inline unic_t readAsciiNonEndChar() {
        if (stream_.atEnd())
            emitError("Unexpected end of input.");

        uint8_t b = stream_.readByte();
        if (b <= 0x7Fu)
            return b;

        return NonAscii;
    }
    inline unic_t readChar() {
        if (stream_.atEnd())
            return End;

        uint8_t b = stream_.readByte();
        if (b <= 0x7Fu)
            return b;

        return readCharSlow(b);
    }
    inline unic_t readNonEndChar() {
        unic_t ch = readChar();
        if (ch == End)
            emitError("Unexpected end of input.");
        return ch;
    }
    unic_t readCharSlow(unic_t firstByte);
    uint8_t readCharNextByte();

    // Character un-reading.
    inline void unreadAsciiChar(unic_t ch) {
        WH_ASSERT(ch == NonAscii || ch == End || (ch >= 0 && ch <= 0x7f));
        if (ch >= NonAscii)
            stream_.rewindBy(1);
    }
    inline void unreadChar(unic_t ch) {
        if (ch >= -1 && ch <= 0x7f)
            stream_.rewindBy(1);
        else
            slowUnreadChar(ch);
    }
    void slowUnreadChar(unic_t ch);

    // Re-read a NonAscii char into a full char, if needed.
    inline unic_t maybeRereadNonAsciiToFull(unic_t ch) {
        if (ch == NonAscii) {
            stream_.rewindBy(1);
            ch = readChar();
        }
        return ch;
    }

    // Helpers.
    inline void finishLineTerminator(unic_t ch) {
        if (ch == '\r') {
            unic_t ch2 = readChar();
            if (ch2 != '\n')
                unreadChar(ch2);
        }
    }

    // Character predicates.
    template <unic_t Char0>
    inline static bool CharIn(unic_t ch) {
        return ch == Char0;
    }

    template <unic_t Char0, unic_t Char1, unic_t... Rest>
    inline static bool CharIn(unic_t ch) {
        return ch == Char0 || CharIn<Char1, Rest...>(ch);
    }

    template <unic_t From, unic_t To>
    inline static bool CharInRange(unic_t ch) {
        return (ch >= From) && (ch <= To);
    }

    inline static bool IsWhitespace(unic_t ch) {
        return CharIn<' ','\t'>(ch) || IsWhitespaceSlow(ch);
    }
    static bool IsWhitespaceSlow(unic_t ch);

    inline static bool IsAsciiLineTerminator(unic_t ch) {
        return CharIn<'\r','\n'>(ch);
    }
    inline static bool IsNonAsciiLineTerminator(unic_t ch) {
        static constexpr unic_t LS = 0x2028;
        static constexpr unic_t PS = 0x2029;
        WH_ASSERT(!(CharIn<'\r', '\n'>(ch)));
        return CharIn<LS, PS>(ch);
    }
    inline static bool IsLineTerminator(unic_t ch) {
        return IsAsciiLineTerminator(ch) || IsNonAsciiLineTerminator(ch);
    }

    inline static bool IsAscii(unic_t ch) {
        unsigned chu = static_cast<unsigned>(ch);
        return (chu & 0x7f) == chu;
    }
    inline static bool IsAsciiLetter(unic_t ch) {
        return CharInRange<'a','z'>(ch) || CharInRange<'A','Z'>(ch);
    }

    inline static bool IsKeywordChar(unic_t ch) {
        return CharInRange<'a','z'>(ch);
    }
    inline static bool IsNonKeywordSimpleIdentifierStart(unic_t ch) {
        WH_ASSERT(!IsKeywordChar(ch));
        return CharInRange<'A','Z'>(ch) || CharIn<'$','_'>(ch);
    }
    inline static bool IsSimpleIdentifierStart(unic_t ch) {
        return IsKeywordChar(ch) || IsNonKeywordSimpleIdentifierStart(ch);
    }
    static bool IsComplexIdentifierStart(unic_t ch);
    inline static bool IsIdentifierStart(unic_t ch) {
        return IsSimpleIdentifierStart(ch) || IsComplexIdentifierStart(ch);
    }

    inline static bool IsNonKeywordSimpleIdentifierContinue(unic_t ch) {
        WH_ASSERT(!IsKeywordChar(ch));
        return CharInRange<'A','Z'>(ch) || IsDecDigit(ch) ||
               CharIn<'$','_'>(ch);
    }
    inline static bool IsSimpleIdentifierContinue(unic_t ch) {
        return IsKeywordChar(ch) || IsNonKeywordSimpleIdentifierContinue(ch);
    }
    static bool IsComplexIdentifierContinue(unic_t ch);
    inline static bool IsIdentifierContinue(unic_t ch) {
        return IsSimpleIdentifierContinue(ch) ||
               IsComplexIdentifierContinue(ch);
    }


    inline static bool IsHexDigit(unic_t ch) {
        return CharInRange<'0', '9'>(ch) ||
               CharInRange<'A', 'F'>(ch) ||
               CharInRange<'a', 'f'>(ch);
    }

    inline static bool IsDecDigit(unic_t ch) {
        return CharInRange<'0', '9'>(ch);
    }

    inline static bool IsOctDigit(unic_t ch) {
        return CharInRange<'0', '7'>(ch);
    }

    inline static bool IsBinDigit(unic_t ch) {
        return CharIn<'0', '1'>(ch);
    }
};


} // namespace Whisper

#endif // WHISPER__PARSER__TOKENIZER_HPP
