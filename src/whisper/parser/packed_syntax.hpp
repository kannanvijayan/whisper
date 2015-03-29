#ifndef WHISPER__PARSER__PACKED_SYNTAX_HPP
#define WHISPER__PARSER__PACKED_SYNTAX_HPP

#include <unordered_map>
#include "allocators.hpp"
#include "runtime.hpp"
#include "fnv_hash.hpp"
#include "parser/code_source.hpp"
#include "parser/syntax_tree.hpp"
#include "vm/string.hpp"
#include "vm/array.hpp"

namespace Whisper {
namespace AST {


class PackedBaseNode;
class PackedBlock;
class PackedSizedBlock;

#define PACKED_NODE_DEF_(ntype) class Packed##ntype##Node;
    WHISPER_DEFN_SYNTAX_NODES(PACKED_NODE_DEF_)
#undef PACKED_NODE_DEF_

class PackedSyntaxElement
{
  public:
    class Position {
      private:
        const uint32_t *ptr_;
      public:
        Position(const uint32_t *ptr) : ptr_(ptr) {}
        const uint32_t *ptr() const { return ptr_; }
    };

  protected:
    VM::Array<uint32_t> *text_;
    uint32_t offset_;

    PackedSyntaxElement(VM::Array<uint32_t> *text, uint32_t offset)
      : text_(text), offset_(offset)
    {}

  public:
    const VM::Array<uint32_t> *text() const {
        return text_;
    }
    uint32_t offset() const {
        return offset_;
    }

  protected:
    const uint32_t valAt(uint32_t idx) const {
        WH_ASSERT(offset_ + idx < text_->length());
        return text_->get(offset_ + idx);
    }
    const uint32_t adjustedOffset(uint32_t idx) const {
        WH_ASSERT(offset_ + idx < text_->length());
        return offset_ + idx;
    }

    inline PackedBaseNode nodeAt(uint32_t idx) const;
    inline PackedBlock blockAt(uint32_t idx, uint32_t stmts) const;
    inline PackedSizedBlock sizedBlockAt(uint32_t idx) const;

    inline PackedBaseNode indirectNodeAt(uint32_t idx) const;
    inline PackedSizedBlock indirectSizedBlockAt(uint32_t idx) const;
};

class PackedBaseNode : public PackedSyntaxElement
{
  protected:
    typedef typename Bitfield<uint32_t, uint16_t, 12, 0>::Const TypeBitfield;
    typedef typename Bitfield<uint32_t, uint32_t, 20, 12>::Const ExtraBitfield;

  public:
    PackedBaseNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedSyntaxElement(text, offset)
    {}

    NodeType type() const {
        uint32_t val = valAt(0);
        return static_cast<NodeType>(TypeBitfield(val).value());
    }
    uint32_t extra() const {
        uint32_t val = valAt(0);
        return ExtraBitfield(val).value();
    }

#define PACKED_NODE_PRED_(ntype) \
    bool is##ntype() const { \
        return type() == ntype; \
    }
    WHISPER_DEFN_SYNTAX_NODES(PACKED_NODE_PRED_)
#undef PACKED_NODE_PRED_

#define PACKED_NODE_CAST_(ntype) \
    inline Packed##ntype##Node as##ntype() const;
    WHISPER_DEFN_SYNTAX_NODES(PACKED_NODE_CAST_)
#undef PACKED_NODE_CAST_
};

class PackedBlock : public PackedSyntaxElement
{
  private:
    uint32_t numStatements_;

  public:
    PackedBlock(VM::Array<uint32_t> *text,
                uint32_t offset,
                uint32_t numStatements)
      : PackedSyntaxElement(text, offset),
        numStatements_(numStatements)
    {}

    uint32_t numStatements() const {
        return numStatements_;
    }

    PackedBaseNode statement(uint32_t idx) const {
        WH_ASSERT(idx < numStatements());
        if (idx == 0)
            return nodeAt(numStatements() - 1);
        return indirectNodeAt(idx - 1);
    }
};

class PackedSizedBlock : public PackedSyntaxElement
{
  public:
    PackedSizedBlock(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedSyntaxElement(text, offset)
    {}

    uint32_t numStatements() const {
        return valAt(0);
    }

    PackedBaseNode statement(uint32_t idx) const {
        WH_ASSERT(idx < numStatements());
        if (idx == 0)
            return nodeAt(numStatements());
        return indirectNodeAt(idx);
    }

    PackedBlock unsizedBlock() const {
        return PackedBlock(text_, adjustedOffset(1), numStatements());
    }
};

inline PackedBaseNode
PackedSyntaxElement::nodeAt(uint32_t idx) const
{
    return PackedBaseNode(text_, adjustedOffset(idx));
}

inline PackedBlock
PackedSyntaxElement::blockAt(uint32_t idx, uint32_t stmts) const
{
    return PackedBlock(text_, adjustedOffset(idx), stmts);
}

inline PackedSizedBlock
PackedSyntaxElement::sizedBlockAt(uint32_t idx) const
{
    return PackedSizedBlock(text_, adjustedOffset(idx));
}

inline PackedBaseNode
PackedSyntaxElement::indirectNodeAt(uint32_t idx) const
{
    return nodeAt(idx + valAt(idx));
}

inline PackedSizedBlock
PackedSyntaxElement::indirectSizedBlockAt(uint32_t idx) const
{
    return sizedBlockAt(idx + valAt(idx));
}


class PackedFileNode : public PackedBaseNode
{
    // Format:
    //      { <NumStatements:16 + Type>;
    //        StmtOffset1; ...; StmtOffsetN-1;
    //        Stmt0...;
    //        Stmt1...;
    //        ...
    //        StmtN-1... }
  public:
    static constexpr uint32_t MaxStatements = 0xffff;

    PackedFileNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == File);
    }

    uint32_t numStatements() const {
        WH_ASSERT(this->extra() <= MaxStatements);
        return extra();
    }
    PackedBaseNode statement(uint32_t idx) const {
        WH_ASSERT(idx < numStatements());
        if (idx == 0)
            return nodeAt(numStatements());
        return indirectNodeAt(idx);
    }
};

class PackedEmptyStmtNode : public PackedBaseNode
{
    // Format:
    //      { <Type> }
  public:
    PackedEmptyStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == EmptyStmt);
    }
};

class PackedExprStmtNode : public PackedBaseNode
{
    // Format:
    //      { <Type>;
    //        Expr... }
  public:
    PackedExprStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == ExprStmt);
    }

    PackedBaseNode expression() const {
        return nodeAt(1);
    }
};

class PackedReturnStmtNode : public PackedBaseNode
{
    // Format:
    //      { <HasExpression:1 | Type>;
    //        Expr... if HasExpression }
  public:
    PackedReturnStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == ReturnStmt);
    }

    bool hasExpression() const {
        return extra() & 1;
    }
    PackedBaseNode expression() const {
        WH_ASSERT(hasExpression());
        return nodeAt(1);
    }
};

class PackedIfStmtNode : public PackedBaseNode
{
    // Format:
    //      { <NumElsifs:16 | HasElse:1 | Type>;
    //        IfBlockOffset;
    //        ElsifCondOffset1; ElsifBlockOffset1;
    //        ...
    //        ElsifCondOffsetN; ElsifBlockOffsetN;
    //        ElseBlockOffset if HasElse;
    //          
    //        IfCond...; SizedIfBlock...;
    //        ElsifCond1...; SizedElsifBlock1...;
    //        ...
    //        ElsifCondN...; SizedElsifBlockN...;
    //        SizedElseBlock... if HasElse }
  public:
    static constexpr uint32_t MaxElsifs = 0xffff;

    PackedIfStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == IfStmt);
    }

    uint32_t numElsifs() const {
        WH_ASSERT((extra() >> 1) <= MaxElsifs);
        return extra() >> 1;
    }
    bool hasElse() const {
        return extra() & 1;
    }

    PackedBaseNode ifCond() const {
        return nodeAt(1 + 1 + (numElsifs() * 2) + (hasElse() ? 1 : 0));
    }
    PackedSizedBlock ifBlock() const {
        return indirectSizedBlockAt(1);
    }

    PackedBaseNode elsifCond(uint32_t idx) const {
        WH_ASSERT(idx < numElsifs());
        return indirectNodeAt(1 + 1 + (idx * 2));
    }
    PackedSizedBlock elsifBlock(uint32_t idx) const {
        WH_ASSERT(idx < numElsifs());
        return indirectSizedBlockAt(1 + 1 + (idx * 2) + 1);
    }

    PackedSizedBlock elseBlock() const {
        WH_ASSERT(hasElse());
        return indirectSizedBlockAt(1 + 1 + (numElsifs() * 2));
    }
};

class PackedDefStmtNode : public PackedBaseNode
{
    // Format:
    //      { <NumParams:16 | Type>;
    //        NameCid;
    //        ParamCid1; ...; ParamCidN;
    //        BodyBlock... }
  public:
    static constexpr uint32_t MaxParams = 0xffff;

    PackedDefStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == DefStmt);
    }

    uint32_t numParams() const {
        WH_ASSERT(extra() <= MaxParams);
        return extra();
    }
    uint32_t nameCid() const {
        return valAt(1);
    }
    uint32_t paramCid(uint32_t paramIdx) const {
        WH_ASSERT(paramIdx < numParams());
        return valAt(1 + 1 + paramIdx);
    }
    PackedSizedBlock bodyBlock() const {
        return sizedBlockAt(1 + 1 + numParams());
    }
};

class PackedConstStmtNode : public PackedBaseNode
{
    // Format:
    //      { <NumBindings:16 | Type>;
    //        VarnameCid1; VarexprOffset1;
    //        ...
    //        VarnameCidN; VarexprOffsetN;
    //        VarExpr1...;
    //        ...
    //        VarExprN... }
  public:
    static constexpr uint32_t MaxBindings = 0xffff;

    PackedConstStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == ConstStmt);
    }

    uint32_t numBindings() const {
        WH_ASSERT(extra() <= MaxBindings);
        return extra();
    }
    uint32_t varnameCid(uint32_t idx) const {
        WH_ASSERT(idx < numBindings());
        return valAt(1 + (idx * 2));
    }
    PackedBaseNode varexpr(uint32_t idx) const {
        WH_ASSERT(idx < numBindings());
        return indirectNodeAt(1 + (idx * 2) + 1);
    }
};

class PackedVarStmtNode : public PackedBaseNode
{
    // Format:
    //      { <NumBindings:16 | Type>;
    //        VarnameCid1; VarexprOffset1;
    //        ...
    //        VarnameCidN; VarexprOffsetN;
    //        VarExpr1...;
    //        ...
    //        VarExprN... }
    //
    // Note: If VarexprOffsetI is 0, it means that the corresponding
    // var has no initializer.
  public:
    static constexpr uint32_t MaxBindings = 0xffff;

    PackedVarStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == VarStmt);
    }

    uint32_t numBindings() const {
        WH_ASSERT(extra() <= MaxBindings);
        return extra();
    }
    uint32_t varnameCid(uint32_t idx) const {
        WH_ASSERT(idx < numBindings());
        return valAt(1 + (idx * 2));
    }
    bool hasVarexpr(uint32_t idx) const {
        WH_ASSERT(idx < numBindings());
        return valAt(1 + (idx * 2) + 1) > 0;
    }
    PackedBaseNode varexpr(uint32_t idx) const {
        WH_ASSERT(idx < numBindings());
        WH_ASSERT(hasVarexpr(idx));
        return indirectNodeAt(1 + (idx * 2) + 1);
    }
};

class PackedLoopStmtNode : public PackedBaseNode
{
    // Format:
    //      { <NumStmts:16 | Type>;
    //        Block... }
  public:
    PackedLoopStmtNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == LoopStmt);
    }

    PackedBlock bodyBlock() const {
        return blockAt(1, extra());
    }
};

class PackedCallExprNode : public PackedBaseNode
{
    // Format:
    //      { <NumArgs:16 | Type>;
    //        ArgOffset1, ...; ArgOffsetN,
    //        CalleeExpr...;
    //        ArgExpr1...;
    //        ...;
    //        ArgExprN }
  public:
    static constexpr uint32_t MaxArgs = 0xffff;

    PackedCallExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == CallExpr);
    }

    uint32_t numArgs() const {
        WH_ASSERT(extra() <= MaxArgs);
        return extra();
    }
    PackedBaseNode callee() const {
        return nodeAt(1 + numArgs());
    }
    PackedBaseNode arg(uint32_t idx) const {
        WH_ASSERT(idx < numArgs());
        return indirectNodeAt(1 + idx);
    }
};

class PackedDotExprNode : public PackedBaseNode
{
    // Format:
    //      { <Type>; NameCid; TargetExpr... }
  public:
    PackedDotExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == DotExpr);
    }

    uint32_t nameCid() const {
        return valAt(1);
    }
    PackedBaseNode target() const {
        return nodeAt(2);
    }
};

class PackedArrowExprNode : public PackedBaseNode
{
    // Format:
    //      { <Type>; NameCid; TargetExpr... }
  public:
    PackedArrowExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == ArrowExpr);
    }

    uint32_t nameCid() const {
        return valAt(1);
    }
    PackedBaseNode target() const {
        return nodeAt(2);
    }
};

class PackedPosExprNode : public PackedBaseNode
{
  public:
    PackedPosExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == PosExpr);
    }

    PackedBaseNode subexpr() const {
        return nodeAt(1);
    }
};

class PackedNegExprNode : public PackedBaseNode
{
  public:
    PackedNegExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == NegExpr);
    }

    PackedBaseNode subexpr() const {
        return nodeAt(1);
    }
};

class PackedAddExprNode : public PackedBaseNode
{
  public:
    PackedAddExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == AddExpr);
    }

    PackedBaseNode lhs() const {
        return nodeAt(2);
    }
    PackedBaseNode rhs() const {
        return indirectNodeAt(1);
    }
};

class PackedSubExprNode : public PackedBaseNode
{
  public:
    PackedSubExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == SubExpr);
    }

    PackedBaseNode lhs() const {
        return nodeAt(2);
    }
    PackedBaseNode rhs() const {
        return indirectNodeAt(1);
    }
};

class PackedMulExprNode : public PackedBaseNode
{
  public:
    PackedMulExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == MulExpr);
    }

    PackedBaseNode lhs() const {
        return nodeAt(2);
    }
    PackedBaseNode rhs() const {
        return indirectNodeAt(1);
    }
};

class PackedDivExprNode : public PackedBaseNode
{
  public:
    PackedDivExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == DivExpr);
    }

    PackedBaseNode lhs() const {
        return nodeAt(2);
    }
    PackedBaseNode rhs() const {
        return indirectNodeAt(1);
    }
};

class PackedParenExprNode : public PackedBaseNode
{
  public:
    PackedParenExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == ParenExpr);
    }

    PackedBaseNode subexpr() const {
        return nodeAt(1);
    }
};

class PackedNameExprNode : public PackedBaseNode
{
  public:
    PackedNameExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == NameExpr);
    }

    uint32_t nameCid() const {
        return valAt(1);
    }
};

class PackedIntegerExprNode : public PackedBaseNode
{
  public:
    PackedIntegerExprNode(VM::Array<uint32_t> *text, uint32_t offset)
      : PackedBaseNode(text, offset)
    {
        WH_ASSERT(type() == IntegerExpr);
    }

    int32_t value() const {
        return static_cast<int32_t>(valAt(1));
    }
};

#define PACKED_NODE_CAST_IMPL_(ntype) \
    inline Packed##ntype##Node PackedBaseNode::as##ntype() const { \
        WH_ASSERT(is##ntype()); \
        return *(reinterpret_cast<const Packed##ntype##Node *>(this)); \
    }
    WHISPER_DEFN_SYNTAX_NODES(PACKED_NODE_CAST_IMPL_)
#undef PACKED_NODE_CAST_IMPL_


} // namespace AST
} // namespace Whisper

#endif // WHISPER__PARSER__PACKED_SYNTAX_HPP
