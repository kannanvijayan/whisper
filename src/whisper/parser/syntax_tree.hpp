#ifndef WHISPER__PARSER__SYNTAX_TREE_HPP
#define WHISPER__PARSER__SYNTAX_TREE_HPP

#include <list>
#include "parser/tokenizer.hpp"
#include "parser/syntax_defn.hpp"

namespace Whisper {
namespace AST {

enum NodeType : uint8_t
{
    INVALID,
#define DEF_ENUM_(node)   node,
    WHISPER_DEFN_SYNTAX_NODES(DEF_ENUM_)
#undef DEF_ENUM_
    LIMIT
};

const char *NodeTypeString(NodeType nodeType);

//
// Base syntax element.
//
class BaseNode
{
  public:
    template <typename T>
    using Allocator = STLBumpAllocator<T>;

    typedef Allocator<uint8_t> StdAllocator;

    template <typename T>
    using List = std::list<T, Allocator<T>>;

  protected:
    NodeType type_;

    BaseNode(NodeType type) : type_(type) {}

  public:
    inline NodeType type() const {
        return type_;
    }

#define DEF_CHECK_(node) \
    inline bool is##node() const { \
        return type_ == node; \
    }
    WHISPER_DEFN_SYNTAX_NODES(DEF_CHECK_)
#undef DEF_CHECK_
};

template <typename Printer>
void PrintNode(const CodeSource &source, const BaseNode *node, Printer printer,
               int tabDepth);

inline constexpr bool
IsValidAssignmentExpressionType(NodeType type)
{
    return (type >= WHISPER_SYNTAX_ASSIGN_MIN) &&
           (type <= WHISPER_SYNTAX_ASSIGN_MAX);
}

///////////////////////////////////////
//                                   //
//  Intermediate and Helper Classes  //
//                                   //
///////////////////////////////////////

class SourceElementNode : public BaseNode
{
  protected:
    SourceElementNode(NodeType type) : BaseNode(type) {}
};

class StatementNode : public SourceElementNode
{
  protected:
    StatementNode(NodeType type) : SourceElementNode(type) {}
};


class ExpressionNode : public BaseNode
{
  protected:
    ExpressionNode(NodeType type) : BaseNode(type) {}
};

class LiteralExpressionNode : public ExpressionNode
{
  protected:
    LiteralExpressionNode(NodeType type) : ExpressionNode(type) {}
};

class VariableDeclaration
{
  public:
    IdentifierNameToken name_;
    ExpressionNode *initialiser_;

  public:
    VariableDeclaration(const IdentifierNameToken &name,
                        ExpressionNode *initialiser)
      : name_(name),
        initialiser_(initialiser)
    {}

    inline const IdentifierNameToken &name() const {
        return name_;
    }
    inline ExpressionNode *initialiser() const {
        return initialiser_;
    }
};

typedef BaseNode::List<ExpressionNode *> ExpressionList;
typedef BaseNode::List<StatementNode *> StatementList;
typedef BaseNode::List<SourceElementNode *> SourceElementList;
typedef BaseNode::List<VariableDeclaration> DeclarationList;

///////////////////
//               //
//  Expressions  //
//               //
///////////////////

//
// ThisNode syntax element
//
class ThisNode : public ExpressionNode
{
  private:
    ThisKeywordToken token_;

  public:
    explicit ThisNode(const ThisKeywordToken &token)
      : ExpressionNode(This),
        token_(token)
    {}

    inline const ThisKeywordToken &token() const {
        return token_;
    }
};

//
// IdentifierNode syntax element
//
class IdentifierNode : public ExpressionNode
{
  private:
    IdentifierNameToken token_;

  public:
    explicit IdentifierNode(const IdentifierNameToken &token)
      : ExpressionNode(Identifier),
        token_(token)
    {}

    inline const IdentifierNameToken &token() const {
        return token_;
    }
};

//
// NullLiteralNode syntax element
//
class NullLiteralNode : public LiteralExpressionNode
{
  private:
    NullLiteralToken token_;

  public:
    explicit NullLiteralNode(const NullLiteralToken &token)
      : LiteralExpressionNode(NullLiteral),
        token_(token)
    {}

    inline const NullLiteralToken &token() const {
        return token_;
    }
};

//
// BooleanLiteralNode syntax element
//
class BooleanLiteralNode : public LiteralExpressionNode
{
  private:
    Either<FalseLiteralToken, TrueLiteralToken> token_;

  public:
    explicit BooleanLiteralNode(const FalseLiteralToken &token)
      : LiteralExpressionNode(BooleanLiteral),
        token_(token)
    {}

    explicit BooleanLiteralNode(const TrueLiteralToken &token)
      : LiteralExpressionNode(BooleanLiteral),
        token_(token)
    {}

    inline bool isFalse() const {
        return token_.hasFirst();
    }

    inline bool isTrue() const {
        return token_.hasSecond();
    }
};

//
// NumericLiteralNode syntax element
//
class NumericLiteralNode : public LiteralExpressionNode
{
  private:
    NumericLiteralToken value_;

  public:
    explicit NumericLiteralNode(const NumericLiteralToken &value)
      : LiteralExpressionNode(NumericLiteral),
        value_(value)
    {}

    inline const NumericLiteralToken value() const {
        return value_;
    }
};

//
// StringLiteralNode syntax element
//
class StringLiteralNode : public LiteralExpressionNode
{
  private:
    StringLiteralToken value_;

  public:
    explicit StringLiteralNode(const StringLiteralToken &value)
      : LiteralExpressionNode(StringLiteral),
        value_(value)
    {}

    inline const StringLiteralToken value() const {
        return value_;
    }
};

//
// RegularExpressionLiteralNode syntax element
//
class RegularExpressionLiteralNode : public LiteralExpressionNode
{
  private:
    RegularExpressionLiteralToken value_;

  public:
    explicit RegularExpressionLiteralNode(
            const RegularExpressionLiteralToken &value)
      : LiteralExpressionNode(RegularExpressionLiteral),
        value_(value)
    {}

    inline const RegularExpressionLiteralToken value() const {
        return value_;
    }
};

//
// ArrayLiteralNode syntax element
//
class ArrayLiteralNode : public LiteralExpressionNode
{
  private:
    ExpressionList elements_;

  public:
    explicit ArrayLiteralNode(ExpressionList &&elements)
      : LiteralExpressionNode(ArrayLiteral),
        elements_(elements)
    {}

    inline const ExpressionList &elements() const {
        return elements_;
    }
};

//
// ObjectLiteralNode syntax element
//
class ObjectLiteralNode : public LiteralExpressionNode
{
  public:
    enum SlotKind { Value, Getter, Setter };

    class ValueDefinition;
    class GetterDefinition;
    class SetterDefinition;

    class PropertyDefinition
    {
      private:
        SlotKind kind_;
        Token name_;

      public:
        PropertyDefinition(SlotKind kind, const Token &name)
          : kind_(kind), name_(name)
        {
            WH_ASSERT(name_.isIdentifierName() ||
                      name_.isStringLiteral() ||
                      name_.isNumericLiteral());
        }

        inline SlotKind kind() const {
            return kind_;
        }

        inline bool isValueSlot() const {
            return kind_ == Value;
        }

        inline const ValueDefinition *toValueSlot() const {
            WH_ASSERT(isValueSlot());
            return reinterpret_cast<const ValueDefinition *>(this);
        }

        inline bool isGetterSlot() const {
            return kind_ == Getter;
        }

        inline const GetterDefinition *toGetterSlot() const {
            WH_ASSERT(isGetterSlot());
            return reinterpret_cast<const GetterDefinition *>(this);
        }

        inline bool isSetterSlot() const {
            return kind_ == Setter;
        }

        inline const SetterDefinition *toSetterSlot() const {
            WH_ASSERT(isSetterSlot());
            return reinterpret_cast<const SetterDefinition *>(this);
        }

        inline bool hasIdentifierName() const {
            return name_.isIdentifierName();
        }

        inline bool hasStringName() const {
            return name_.isStringLiteral();
        }

        inline bool hasNumericName() const {
            return name_.isNumericLiteral();
        }

        inline const IdentifierNameToken &identifierName() const {
            WH_ASSERT(hasIdentifierName());
            return reinterpret_cast<const IdentifierNameToken &>(name_);
        }

        inline const StringLiteralToken &stringName() const {
            WH_ASSERT(hasStringName());
            return reinterpret_cast<const StringLiteralToken &>(name_);
        }

        inline const NumericLiteralToken &numericName() const {
            WH_ASSERT(hasNumericName());
            return reinterpret_cast<const NumericLiteralToken &>(name_);
        }

        inline const Token &name() const {
            return name_;
        }
    };

    class ValueDefinition : public PropertyDefinition
    {
      private:
        ExpressionNode *value_;

      public:
        ValueDefinition(const Token &name, ExpressionNode *value)
          : PropertyDefinition(Value, name), value_(value)
        {
        }

        inline ExpressionNode *value() const {
            return value_;
        }
    };

    class AccessorDefinition : public PropertyDefinition
    {
      protected:
        SourceElementList body_;

      public:
        AccessorDefinition(SlotKind kind, const Token &name,
                           SourceElementList &&body)
          : PropertyDefinition(kind, name), body_(body)
        {}

        inline const SourceElementList &body() const {
            return body_;
        }
    };

    class GetterDefinition : public AccessorDefinition
    {
      public:
        GetterDefinition(const Token &name, SourceElementList &&body)
          : AccessorDefinition(Getter, name, std::move(body))
        {}
    };

    class SetterDefinition : public AccessorDefinition
    {
      private:
        IdentifierNameToken parameter_;

      public:
        SetterDefinition(const Token &name,
                         const IdentifierNameToken &parameter,
                         SourceElementList &&body)
          : AccessorDefinition(Setter, name, std::move(body)),
            parameter_(parameter)
        {}

        const IdentifierNameToken &parameter() const {
            return parameter_;
        }
    };

    typedef List<PropertyDefinition *> PropertyDefinitionList;

  private:
    PropertyDefinitionList propertyDefinitions_;

  public:
    explicit ObjectLiteralNode(PropertyDefinitionList &&propertyDefinitions)
      : LiteralExpressionNode(ObjectLiteral),
        propertyDefinitions_(propertyDefinitions)
    {}

    inline const PropertyDefinitionList &propertyDefinitions() const {
        return propertyDefinitions_;
    }
};

//
// ParenthesizedExpressionNode syntax element
//
class ParenthesizedExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *subexpression_;

  public:
    explicit ParenthesizedExpressionNode(ExpressionNode *subexpression)
      : ExpressionNode(ParenthesizedExpression),
        subexpression_(subexpression)
    {}

    inline ExpressionNode *subexpression() const {
        return subexpression_;
    }
};

//
// FunctionExpression syntax element
//
class FunctionExpressionNode : public ExpressionNode
{
  public:
    typedef BaseNode::List<IdentifierNameToken> FormalParameterList;

  private:
    Maybe<IdentifierNameToken> name_;
    FormalParameterList formalParameters_;
    SourceElementList functionBody_;

  public:
    FunctionExpressionNode(FormalParameterList &&formalParameters,
                           SourceElementList &&functionBody)
      : ExpressionNode(FunctionExpression),
        name_(),
        formalParameters_(formalParameters),
        functionBody_(functionBody)
    {}

    FunctionExpressionNode(const IdentifierNameToken &name,
                           FormalParameterList &&formalParameters,
                           SourceElementList &&functionBody)
      : ExpressionNode(FunctionExpression),
        name_(name),
        formalParameters_(formalParameters),
        functionBody_(functionBody)
    {}

    inline const Maybe<IdentifierNameToken> &name() const {
        return name_;
    }

    inline const FormalParameterList &formalParameters() const {
        return formalParameters_;
    }

    inline const SourceElementList &functionBody() const {
        return functionBody_;
    }
};

//
// GetElementExpression syntax element
//
class GetElementExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *object_;
    ExpressionNode *element_;

  public:
    GetElementExpressionNode(ExpressionNode *object,
                             ExpressionNode *element)
      : ExpressionNode(GetElementExpression),
        object_(object),
        element_(element)
    {}

    inline ExpressionNode *object() const {
        return object_;
    }

    inline ExpressionNode *element() const {
        return element_;
    }
};

//
// GetPropertyExpression syntax element
//
class GetPropertyExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *object_;
    IdentifierNameToken property_;

  public:
    GetPropertyExpressionNode(ExpressionNode *object,
                              const IdentifierNameToken &property)
      : ExpressionNode(GetPropertyExpression),
        object_(object),
        property_(property)
    {}

    inline ExpressionNode *object() const {
        return object_;
    }

    inline const IdentifierNameToken &property() const {
        return property_;
    }
};

//
// NewExpression syntax element
//
class NewExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *constructor_;
    ExpressionList arguments_;

  public:
    NewExpressionNode(ExpressionNode *constructor, ExpressionList &&arguments)
      : ExpressionNode(NewExpression),
        constructor_(constructor),
        arguments_(arguments)
    {}

    inline ExpressionNode *constructor() const {
        return constructor_;
    }

    inline const ExpressionList &arguments() const {
        return arguments_;
    }
};

//
// CallExpression syntax element
//
class CallExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *function_;
    ExpressionList arguments_;

  public:
    CallExpressionNode(ExpressionNode *function, ExpressionList &&arguments)
      : ExpressionNode(CallExpression),
        function_(function),
        arguments_(arguments)
    {}

    inline ExpressionNode *function() const {
        return function_;
    }

    inline const ExpressionList &arguments() const {
        return arguments_;
    }
};

//
// UnaryExpression syntax element
//
template <NodeType TYPE>
class UnaryExpressionNode : public ExpressionNode
{
    static_assert(TYPE == PostIncrementExpression ||
                  TYPE == PreIncrementExpression ||
                  TYPE == PostDecrementExpression ||
                  TYPE == PreDecrementExpression ||
                  TYPE == DeleteExpression ||
                  TYPE == VoidExpression ||
                  TYPE == TypeOfExpression ||
                  TYPE == PositiveExpression ||
                  TYPE == NegativeExpression ||
                  TYPE == BitNotExpression ||
                  TYPE == LogicalNotExpression,
                  "Invalid IncDecExpressionNode type.");
  private:
    ExpressionNode *subexpression_;

  public:
    explicit UnaryExpressionNode(ExpressionNode *subexpression)
      : ExpressionNode(TYPE),
        subexpression_(subexpression)
    {}

    inline ExpressionNode *subexpression() const {
        return subexpression_;
    }
};
typedef UnaryExpressionNode<PostIncrementExpression>
        PostIncrementExpressionNode;
typedef UnaryExpressionNode<PreIncrementExpression>
        PreIncrementExpressionNode;
typedef UnaryExpressionNode<PostDecrementExpression>
        PostDecrementExpressionNode;
typedef UnaryExpressionNode<PreDecrementExpression>
        PreDecrementExpressionNode;

typedef UnaryExpressionNode<DeleteExpression>
        DeleteExpressionNode;
typedef UnaryExpressionNode<VoidExpression>
        VoidExpressionNode;
typedef UnaryExpressionNode<TypeOfExpression>
        TypeOfExpressionNode;
typedef UnaryExpressionNode<PositiveExpression>
        PositiveExpressionNode;
typedef UnaryExpressionNode<NegativeExpression>
        NegativeExpressionNode;
typedef UnaryExpressionNode<BitNotExpression>
        BitNotExpressionNode;
typedef UnaryExpressionNode<LogicalNotExpression>
        LogicalNotExpressionNode;

//
// BinaryExpression syntax element
//
template <NodeType TYPE>
class BinaryExpressionNode : public ExpressionNode
{
    static_assert(TYPE == MultiplyExpression ||
                  TYPE == DivideExpression ||
                  TYPE == ModuloExpression ||
                  TYPE == AddExpression ||
                  TYPE == SubtractExpression ||
                  TYPE == LeftShiftExpression ||
                  TYPE == RightShiftExpression ||
                  TYPE == UnsignedRightShiftExpression ||
                  TYPE == LessThanExpression ||
                  TYPE == GreaterThanExpression ||
                  TYPE == LessEqualExpression ||
                  TYPE == GreaterEqualExpression ||
                  TYPE == InstanceOfExpression ||
                  TYPE == InExpression ||
                  TYPE == EqualExpression ||
                  TYPE == NotEqualExpression ||
                  TYPE == StrictEqualExpression ||
                  TYPE == StrictNotEqualExpression ||
                  TYPE == BitAndExpression ||
                  TYPE == BitXorExpression ||
                  TYPE == BitOrExpression ||
                  TYPE == LogicalAndExpression ||
                  TYPE == LogicalOrExpression ||
                  TYPE == CommaExpression,
                  "Invalid IncDecExpressionNode type.");
  private:
    ExpressionNode *lhs_;
    ExpressionNode *rhs_;

  public:
    explicit BinaryExpressionNode(ExpressionNode *lhs, ExpressionNode *rhs)
      : ExpressionNode(TYPE),
        lhs_(lhs),
        rhs_(rhs)
    {}

    inline ExpressionNode *lhs() const {
        return lhs_;
    }

    inline ExpressionNode *rhs() const {
        return rhs_;
    }
};

typedef BinaryExpressionNode<MultiplyExpression>
        MultiplyExpressionNode;
typedef BinaryExpressionNode<DivideExpression>
        DivideExpressionNode;
typedef BinaryExpressionNode<ModuloExpression>
        ModuloExpressionNode;
typedef BinaryExpressionNode<AddExpression>
        AddExpressionNode;
typedef BinaryExpressionNode<SubtractExpression>
        SubtractExpressionNode;
typedef BinaryExpressionNode<LeftShiftExpression>
        LeftShiftExpressionNode;
typedef BinaryExpressionNode<RightShiftExpression>
        RightShiftExpressionNode;
typedef BinaryExpressionNode<UnsignedRightShiftExpression>
        UnsignedRightShiftExpressionNode;
typedef BinaryExpressionNode<LessThanExpression>
        LessThanExpressionNode;
typedef BinaryExpressionNode<GreaterThanExpression>
        GreaterThanExpressionNode;
typedef BinaryExpressionNode<LessEqualExpression>
        LessEqualExpressionNode;
typedef BinaryExpressionNode<GreaterEqualExpression>
        GreaterEqualExpressionNode;
typedef BinaryExpressionNode<InstanceOfExpression>
        InstanceOfExpressionNode;
typedef BinaryExpressionNode<InExpression>
        InExpressionNode;
typedef BinaryExpressionNode<EqualExpression>
        EqualExpressionNode;
typedef BinaryExpressionNode<NotEqualExpression>
        NotEqualExpressionNode;
typedef BinaryExpressionNode<StrictEqualExpression>
        StrictEqualExpressionNode;
typedef BinaryExpressionNode<StrictNotEqualExpression>
        StrictNotEqualExpressionNode;
typedef BinaryExpressionNode<BitAndExpression>
        BitAndExpressionNode;
typedef BinaryExpressionNode<BitXorExpression>
        BitXorExpressionNode;
typedef BinaryExpressionNode<BitOrExpression>
        BitOrExpressionNode;
typedef BinaryExpressionNode<LogicalAndExpression>
        LogicalAndExpressionNode;
typedef BinaryExpressionNode<LogicalOrExpression>
        LogicalOrExpressionNode;
typedef BinaryExpressionNode<CommaExpression>
        CommaExpressionNode;

//
// ConditionalExpression syntax element
//
class ConditionalExpressionNode : public ExpressionNode
{
  private:
    ExpressionNode *condition_;
    ExpressionNode *trueExpression_;
    ExpressionNode *falseExpression_;

  public:
    ConditionalExpressionNode(ExpressionNode *condition,
                              ExpressionNode *trueExpression,
                              ExpressionNode *falseExpression)
      : ExpressionNode(ConditionalExpression),
        condition_(condition),
        trueExpression_(trueExpression),
        falseExpression_(falseExpression)
    {}

    inline ExpressionNode *condition() const {
        return condition_;
    }

    inline ExpressionNode *trueExpression() const {
        return trueExpression_;
    }

    inline ExpressionNode *falseExpression() const {
        return falseExpression_;
    }
};

//
// AssignmentExpression syntax element
//
class AssignmentExpressionNode : public ExpressionNode
{
  protected:
    AssignmentExpressionNode(NodeType type) : ExpressionNode(type) {}
};

template <NodeType TYPE>
class BaseAssignExpressionNode : public AssignmentExpressionNode
{
    static_assert(IsValidAssignmentExpressionType(TYPE),
                  "Invalid AssignmentExpressionNode type.");

  private:
    ExpressionNode *lhs_;
    ExpressionNode *rhs_;

  public:
    BaseAssignExpressionNode(ExpressionNode *lhs,
                             ExpressionNode *rhs)
      : AssignmentExpressionNode(TYPE),
        lhs_(lhs),
        rhs_(rhs)
    {}

    inline ExpressionNode *lhs() const {
        return lhs_;
    }

    inline ExpressionNode *rhs() const {
        return rhs_;
    }
};
typedef BaseAssignExpressionNode<AssignExpression>
        AssignExpressionNode;
typedef BaseAssignExpressionNode<AddAssignExpression>
        AddAssignExpressionNode;
typedef BaseAssignExpressionNode<SubtractAssignExpression>
        SubtractAssignExpressionNode;
typedef BaseAssignExpressionNode<MultiplyAssignExpression>
        MultiplyAssignExpressionNode;
typedef BaseAssignExpressionNode<ModuloAssignExpression>
        ModuloAssignExpressionNode;
typedef BaseAssignExpressionNode<LeftShiftAssignExpression>
        LeftShiftAssignExpressionNode;
typedef BaseAssignExpressionNode<RightShiftAssignExpression>
        RightShiftAssignExpressionNode;
typedef BaseAssignExpressionNode<UnsignedRightShiftAssignExpression>
        UnsignedRightShiftAssignExpressionNode;
typedef BaseAssignExpressionNode<BitAndAssignExpression>
        BitAndAssignExpressionNode;
typedef BaseAssignExpressionNode<BitOrAssignExpression>
        BitOrAssignExpressionNode;
typedef BaseAssignExpressionNode<BitXorAssignExpression>
        BitXorAssignExpressionNode;
typedef BaseAssignExpressionNode<DivideAssignExpression>
        DivideAssignExpressionNode;


//////////////////
//              //
//  Statements  //
//              //
//////////////////

//
// Block syntax element
//
class BlockNode : public StatementNode
{
  private:
    SourceElementList sourceElements_;

  public:
    explicit BlockNode(SourceElementList &&sourceElements)
      : StatementNode(Block),
        sourceElements_(sourceElements)
    {}

    inline const SourceElementList &sourceElements() const {
        return sourceElements_;
    }
};

//
// VariableStatement syntax element
//
class VariableStatementNode : public StatementNode
{
  private:
    DeclarationList declarations_;

  public:
    explicit VariableStatementNode(DeclarationList &&declarations)
      : StatementNode(VariableStatement),
        declarations_(declarations)
    {}

    inline const DeclarationList &declarations() const {
        return declarations_;
    }
};

//
// EmptyStatement syntax element
//
class EmptyStatementNode : public StatementNode
{
  public:
    EmptyStatementNode() : StatementNode(EmptyStatement) {}
};

//
// ExpressionStatement syntax element
//
class ExpressionStatementNode : public StatementNode
{
  private:
    ExpressionNode *expression_;

  public:
    explicit ExpressionStatementNode(ExpressionNode *expression)
      : StatementNode(ExpressionStatement),
        expression_(expression)
    {}

    inline ExpressionNode *expression() const {
        return expression_;
    }
};

//
// IfStatement syntax element
//
class IfStatementNode : public StatementNode
{
  private:
    ExpressionNode *condition_;
    StatementNode *trueBody_;
    StatementNode *falseBody_;

  public:
    IfStatementNode(ExpressionNode *condition,
                    StatementNode *trueBody,
                    StatementNode *falseBody)
      : StatementNode(IfStatement),
        condition_(condition),
        trueBody_(trueBody),
        falseBody_(falseBody)
    {}

    inline ExpressionNode *condition() const {
        return condition_;
    }

    inline StatementNode *trueBody() const {
        return trueBody_;
    }

    inline StatementNode *falseBody() const {
        return falseBody_;
    }
};

//
// Base class for all iteration statements.
//
class IterationStatementNode : public StatementNode
{
  protected:
    IterationStatementNode(NodeType type) : StatementNode(type) {}
};

//
// DoWhileStatement syntax element
//
class DoWhileStatementNode : public IterationStatementNode
{
  private:
    StatementNode *body_;
    ExpressionNode *condition_;

  public:
    DoWhileStatementNode(StatementNode *body,
                         ExpressionNode *condition)
      : IterationStatementNode(DoWhileStatement),
        body_(body),
        condition_(condition)
    {}

    inline StatementNode *body() const {
        return body_;
    }

    inline ExpressionNode *condition() const {
        return condition_;
    }
};

//
// WhileStatement syntax element
//
class WhileStatementNode : public IterationStatementNode
{
  private:
    ExpressionNode *condition_;
    StatementNode *body_;

  public:
    WhileStatementNode(ExpressionNode *condition,
                       StatementNode *body)
      : IterationStatementNode(WhileStatement),
        condition_(condition),
        body_(body)
    {}

    inline ExpressionNode *condition() const {
        return condition_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// ForLoopStatement syntax element
//
class ForLoopStatementNode : public IterationStatementNode
{
  private:
    ExpressionNode *initial_;
    ExpressionNode *condition_;
    ExpressionNode *update_;
    StatementNode *body_;

  public:
    ForLoopStatementNode(ExpressionNode *initial,
                         ExpressionNode *condition,
                         ExpressionNode *update,
                         StatementNode *body)
      : IterationStatementNode(ForLoopStatement),
        initial_(initial),
        condition_(condition),
        update_(update),
        body_(body)
    {}

    inline ExpressionNode *initial() const {
        return initial_;
    }

    inline ExpressionNode *condition() const {
        return condition_;
    }

    inline ExpressionNode *update() const {
        return update_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// ForLoopVarStatement syntax element
//
class ForLoopVarStatementNode : public IterationStatementNode
{
  private:
    DeclarationList initial_;
    ExpressionNode *condition_;
    ExpressionNode *update_;
    StatementNode *body_;

  public:
    ForLoopVarStatementNode(DeclarationList &&initial,
                            ExpressionNode *condition,
                            ExpressionNode *update,
                            StatementNode *body)
      : IterationStatementNode(ForLoopVarStatement),
        initial_(initial),
        condition_(condition),
        update_(update),
        body_(body)
    {}

    inline const DeclarationList &initial() const {
        return initial_;
    }
    inline DeclarationList &initial() {
        return initial_;
    }

    inline ExpressionNode *condition() const {
        return condition_;
    }

    inline ExpressionNode *update() const {
        return update_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// ForInStatement syntax element
//
class ForInStatementNode : public IterationStatementNode
{
  private:
    ExpressionNode *lhs_;
    ExpressionNode *object_;
    StatementNode *body_;

  public:
    ForInStatementNode(ExpressionNode *lhs,
                       ExpressionNode *object,
                       StatementNode *body)
      : IterationStatementNode(ForInStatement),
        lhs_(lhs),
        object_(object),
        body_(body)
    {}

    inline ExpressionNode *lhs() const {
        return lhs_;
    }

    inline ExpressionNode *object() const {
        return object_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// ForInVarStatement syntax element
//
class ForInVarStatementNode : public IterationStatementNode
{
  private:
    IdentifierNameToken name_;
    ExpressionNode *object_;
    StatementNode *body_;

  public:
    ForInVarStatementNode(const IdentifierNameToken &name,
                          ExpressionNode *object,
                          StatementNode *body)
      : IterationStatementNode(ForInVarStatement),
        name_(name),
        object_(object),
        body_(body)
    {}

    inline const IdentifierNameToken &name() const {
        return name_;
    }

    inline ExpressionNode *object() const {
        return object_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// ContinueStatement syntax element
//
class ContinueStatementNode : public StatementNode
{
  private:
    Maybe<IdentifierNameToken> label_;

  public:
    ContinueStatementNode()
      : StatementNode(ContinueStatement),
        label_()
    {}

    explicit ContinueStatementNode(const IdentifierNameToken &label)
      : StatementNode(ContinueStatement),
        label_(label)
    {}

    inline const Maybe<IdentifierNameToken> &label() const {
        return label_;
    }
};

//
// BreakStatement syntax element
//
class BreakStatementNode : public StatementNode
{
  private:
    Maybe<IdentifierNameToken> label_;

  public:
    BreakStatementNode()
      : StatementNode(BreakStatement),
        label_()
    {}

    explicit BreakStatementNode(const IdentifierNameToken &label)
      : StatementNode(BreakStatement),
        label_(label)
    {}

    inline const Maybe<IdentifierNameToken> &label() const {
        return label_;
    }
};

//
// ReturnStatement syntax element
//
class ReturnStatementNode : public StatementNode
{
  private:
    ExpressionNode *value_;

  public:
    explicit ReturnStatementNode(ExpressionNode *value)
      : StatementNode(ReturnStatement),
        value_(value)
    {}

    inline ExpressionNode *value() const {
        return value_;
    }
};

//
// WithStatement syntax element
//
class WithStatementNode : public StatementNode
{
  private:
    ExpressionNode *value_;
    StatementNode *body_;

  public:
    WithStatementNode(ExpressionNode *value,
                      StatementNode *body)
      : StatementNode(WithStatement),
        value_(value),
        body_(body)
    {}

    inline ExpressionNode *value() const {
        return value_;
    }

    inline StatementNode *body() const {
        return body_;
    }
};

//
// SwitchStatement syntax element
//
class SwitchStatementNode : public StatementNode
{
  public:
    class CaseClause
    {
      private:
        ExpressionNode *expression_;
        StatementList statements_;

      public:
        CaseClause(ExpressionNode *expression,
                   StatementList &&statements)
          : expression_(expression),
            statements_(statements)
        {}

        CaseClause(const CaseClause &other)
          : expression_(other.expression_),
            statements_(other.statements_)
        {}

        CaseClause(CaseClause &&other)
          : expression_(other.expression_),
            statements_(std::move(other.statements_))
        {}

        inline ExpressionNode *expression() const {
            return expression_;
        }

        inline const StatementList &statements() const {
            return statements_;
        }
    };
    typedef List<CaseClause> CaseClauseList;

  private:
    ExpressionNode *value_;
    CaseClauseList caseClauses_;

  public:
    SwitchStatementNode(ExpressionNode *value,
                        CaseClauseList &&caseClauses)
      : StatementNode(SwitchStatement),
        value_(value),
        caseClauses_(caseClauses)
    {}

    inline ExpressionNode *value() const {
        return value_;
    }

    inline const CaseClauseList &caseClauses() const {
        return caseClauses_;
    }
};

//
// LabelledStatement syntax element
//
class LabelledStatementNode : public StatementNode
{
  private:
    IdentifierNameToken label_;
    StatementNode *statement_;

  public:
    LabelledStatementNode(const IdentifierNameToken &label,
                          StatementNode *statement)
      : StatementNode(LabelledStatement),
        label_(label),
        statement_(statement)
    {}

    inline const IdentifierNameToken &label() const {
        return label_;
    }

    inline StatementNode *statement() const {
        return statement_;
    }
};

//
// ThrowStatement syntax element
//
class ThrowStatementNode : public StatementNode
{
  private:
    ExpressionNode *value_;

  public:
    explicit ThrowStatementNode(ExpressionNode *value)
      : StatementNode(ThrowStatement),
        value_(value)
    {}

    inline ExpressionNode *value() const {
        return value_;
    }
};


//
// Base helper class for all try/catch?/finally? statements.
//
class TryStatementNode : public StatementNode
{
  protected:
    TryStatementNode(NodeType type) : StatementNode(type) {}
};


//
// TryCatchStatement syntax element
//
class TryCatchStatementNode : public TryStatementNode
{
  private:
    BlockNode *tryBlock_;
    IdentifierNameToken catchName_;
    BlockNode *catchBlock_;

  public:
    TryCatchStatementNode(BlockNode *tryBlock,
                          const IdentifierNameToken &catchName,
                          BlockNode *catchBlock)
      : TryStatementNode(TryCatchStatement),
        tryBlock_(tryBlock),
        catchName_(catchName),
        catchBlock_(catchBlock)
    {}

    inline BlockNode *tryBlock() const {
        return tryBlock_;
    }

    inline const IdentifierNameToken &catchName() const {
        return catchName_;
    }

    inline BlockNode *catchBlock() const {
        return catchBlock_;
    }
};

//
// TryFinallyStatement syntax element
//
class TryFinallyStatementNode : public TryStatementNode
{
  private:
    BlockNode *tryBlock_;
    BlockNode *finallyBlock_;

  public:
    TryFinallyStatementNode(BlockNode *tryBlock,
                            BlockNode *finallyBlock)
      : TryStatementNode(TryFinallyStatement),
        tryBlock_(tryBlock),
        finallyBlock_(finallyBlock)
    {}

    inline BlockNode *tryBlock() const {
        return tryBlock_;
    }

    inline BlockNode *finallyBlock() const {
        return finallyBlock_;
    }
};

//
// TryCatchFinallyStatement syntax element
//
class TryCatchFinallyStatementNode : public TryStatementNode
{
  private:
    BlockNode *tryBlock_;
    IdentifierNameToken catchName_;
    BlockNode *catchBlock_;
    BlockNode *finallyBlock_;

  public:
    TryCatchFinallyStatementNode(BlockNode *tryBlock,
                                 const IdentifierNameToken &catchName,
                                 BlockNode *catchBlock,
                                 BlockNode *finallyBlock)
      : TryStatementNode(TryCatchFinallyStatement),
        tryBlock_(tryBlock),
        catchName_(catchName),
        catchBlock_(catchBlock),
        finallyBlock_(finallyBlock)
    {}

    inline BlockNode *tryBlock() const {
        return tryBlock_;
    }

    inline const IdentifierNameToken &catchName() const {
        return catchName_;
    }

    inline BlockNode *catchBlock() const {
        return catchBlock_;
    }

    inline BlockNode *finallyBlock() const {
        return finallyBlock_;
    }
};

//
// DebuggerStatement syntax element
//
class DebuggerStatementNode : public StatementNode
{
  public:
    explicit DebuggerStatementNode()
      : StatementNode(DebuggerStatement)
    {}
};

/////////////////////////////
//                         //
//  Functions And Scripts  //
//                         //
/////////////////////////////

//
// FunctionDeclaration syntax element
//
class FunctionDeclarationNode : public SourceElementNode
{
  private:
    FunctionExpressionNode *func_;

  public:
    FunctionDeclarationNode(FunctionExpressionNode *func)
      : SourceElementNode(FunctionDeclaration),
        func_(func)
    {
        WH_ASSERT(func->name());
    }

    inline FunctionExpressionNode *func() const {
        return func_;
    }
};

//
// Program syntax element
//
class ProgramNode : public BaseNode
{
  private:
    SourceElementList sourceElements_;

  public:
    explicit ProgramNode(SourceElementList &&sourceElements)
      : BaseNode(Program),
        sourceElements_(sourceElements)
    {}

    inline const SourceElementList &sourceElements() const {
        return sourceElements_;
    }
};


/////////////////////////////
//                         //
//  BaseNode Cating Funcs  //
//                         //
/////////////////////////////

#define DEF_CAST_(node) \
    inline const node##Node * To##node(const BaseNode *n) { \
        WH_ASSERT(n->is##node()); \
        return reinterpret_cast<const node##Node *>(n); \
    } \
    inline node##Node * To##node(BaseNode *n) { \
        WH_ASSERT(n->is##node()); \
        return reinterpret_cast<node##Node *>(n); \
    }
    WHISPER_DEFN_SYNTAX_NODES(DEF_CAST_)
#undef DEF_CAST_

inline FunctionExpressionNode *MaybeToNamedFunction(BaseNode *node)
{
    FunctionExpressionNode *fun = nullptr;
    if (node->isFunctionExpression()) {
        fun = ToFunctionExpression(fun);
    } else if (node->isExpressionStatement()) {
        ExpressionStatementNode *exprStmt = ToExpressionStatement(node);
        if (exprStmt->expression()->isFunctionExpression())
            fun = ToFunctionExpression(exprStmt->expression());
    }

    if (fun && fun->name())
        return fun;

    return nullptr;
}

inline bool IsLeftHandSideExpression(BaseNode *node)
{
    return node->isIdentifier() || node->isGetElementExpression() ||
           node->isGetPropertyExpression();
}


} // namespace AST
} // namespace Whisper


#endif // WHISPER__PARSER__SYNTAX_TREE_HPP
