#ifndef WHISPER__PARSER__SYNTAX_DEFN_HPP
#define WHISPER__PARSER__SYNTAX_DEFN_HPP


#define WHISPER_DEFN_SYNTAX_NODES(_) \
    /* Top level. */                        \
    _(Program)                              \
    _(FunctionDeclaration)                  \
    \
    /* Expressions. */                      \
    _(This)                                 \
    _(Identifier)                           \
    _(NullLiteral)                          \
    _(BooleanLiteral)                       \
    _(NumericLiteral)                       \
    _(StringLiteral)                        \
    _(RegularExpressionLiteral)             \
    _(ArrayLiteral)                         \
    _(ObjectLiteral)                        \
    _(ParenthesizedExpression)              \
    _(FunctionExpression)                   \
    _(GetElementExpression)                 \
    _(GetPropertyExpression)                \
    _(NewExpression)                        \
    _(CallExpression)                       \
    /* Unary Expressions. */                \
    _(PostIncrementExpression)              \
    _(PreIncrementExpression)               \
    _(PostDecrementExpression)              \
    _(PreDecrementExpression)               \
    _(DeleteExpression)                     \
    _(VoidExpression)                       \
    _(TypeOfExpression)                     \
    _(PositiveExpression)                   \
    _(NegativeExpression)                   \
    _(BitNotExpression)                     \
    _(LogicalNotExpression)                 \
    /* Binary Expressions. */               \
    _(MultiplyExpression)                   \
    _(DivideExpression)                     \
    _(ModuloExpression)                     \
    _(AddExpression)                        \
    _(SubtractExpression)                   \
    _(LeftShiftExpression)                  \
    _(RightShiftExpression)                 \
    _(UnsignedRightShiftExpression)         \
    _(LessThanExpression)                   \
    _(GreaterThanExpression)                \
    _(LessEqualExpression)                  \
    _(GreaterEqualExpression)               \
    _(InstanceOfExpression)                 \
    _(InExpression)                         \
    _(EqualExpression)                      \
    _(NotEqualExpression)                   \
    _(StrictEqualExpression)                \
    _(StrictNotEqualExpression)             \
    _(BitAndExpression)                     \
    _(BitXorExpression)                     \
    _(BitOrExpression)                      \
    _(LogicalAndExpression)                 \
    _(LogicalOrExpression)                  \
    _(CommaExpression)                      \
    /* Misc Expression. */                  \
    _(ConditionalExpression)                \
    /* Assignment Expressions. */           \
    _(AssignExpression)                     \
    _(AddAssignExpression)                  \
    _(SubtractAssignExpression)             \
    _(MultiplyAssignExpression)             \
    _(ModuloAssignExpression)               \
    _(LeftShiftAssignExpression)            \
    _(RightShiftAssignExpression)           \
    _(UnsignedRightShiftAssignExpression)   \
    _(BitAndAssignExpression)               \
    _(BitOrAssignExpression)                \
    _(BitXorAssignExpression)               \
    _(DivideAssignExpression)               \
    \
    /* Statements. */                       \
    _(Block)                                \
    _(VariableStatement)                    \
    _(EmptyStatement)                       \
    _(ExpressionStatement)                  \
    _(IfStatement)                          \
    _(DoWhileStatement)                     \
    _(WhileStatement)                       \
    _(ForLoopStatement)                     \
    _(ForLoopVarStatement)                  \
    _(ForInStatement)                       \
    _(ForInVarStatement)                    \
    _(ContinueStatement)                    \
    _(BreakStatement)                       \
    _(ReturnStatement)                      \
    _(WithStatement)                        \
    _(SwitchStatement)                      \
    _(LabelledStatement)                    \
    _(ThrowStatement)                       \
    _(TryCatchStatement)                    \
    _(TryFinallyStatement)                  \
    _(TryCatchFinallyStatement)             \
    _(DebuggerStatement)

#define WHISPER_SYNTAX_ASSIGN_MIN       AssignExpression
#define WHISPER_SYNTAX_ASSIGN_MAX       DivideAssignExpression

#endif // WHISPER__PARSER__SYNTAX_DEFN_HPP
