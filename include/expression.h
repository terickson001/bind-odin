#ifndef C_PREPROCESSOR_EXPRESSION_H
#define C_PREPROCESSOR_EXPRESSION_H 1

#include "types.h"
#include "preprocess.h"

typedef enum Op_Kind
{
    Op_Invalid,

    Op_Add,
    Op_Sub,
    Op_Mul,
    Op_Div,
    Op_Mod,

    Op_BOr,
    Op_BAnd,
    Op_BNot,
    Op_Shl,
    Op_Shr,
    Op_Xor,

    Op_EQ,
    Op_NEQ,
    Op_LE,
    Op_GE,
    Op_LT,
    Op_GT,

    Op_Not,
    Op_And,
    Op_Or,

    Op_TernL,
    Op_TernR,

    Op_Semicolon,

    Op_sizeof,

    __Op_LiteralEnd,

    Op_typecast,

    Op_Count
} Op_Kind;

gb_global char const *Op_Kind_Strings[] =
{
    "Op_Invalid",

    "+",
    "-",
    "*",
    "/",
    "%",

    "|",
    "&",
    "~",
    "<<",
    ">>",
    "^",

    "==",
    "!=",
    "<=",
    ">=",
    "<",
    ">",

    "!",
    "&&",
    "||",

    "?",
    ":",

    ";",

    "sizeof",
    "typecast"
};

typedef struct Expr Expr;
typedef struct Op
{
    int precedence;
    Op_Kind kind;
    Expr *type;
} Op;

typedef enum Expr_Kind
{
    Expr_Invalid,
    Expr_Constant,
    Expr_Type,
    Expr_String,
    Expr_Unary,
    Expr_Binary,
    Expr_Ternary
} Expr_Kind;

typedef enum Constant_Size
{
    Constant_Unknown,
    Constant_8   = 1,
    Constant_16  = 2,
    Constant_32  = 4,
    Constant_64  = 8
} Constant_Size;

typedef struct Constant_Type
{
    Constant_Size size;
    b32 is_signed;
} Constant_Type;
gb_global Constant_Type DEFAULT_TYPE = {Constant_32, true};

typedef struct Constant_Format
{
    int is_char;
    int base;
    int sig_figs;
} Constant_Format;
gb_global Constant_Format DEFAULT_FORMAT = {false, 10, -1};

typedef struct Constant_Expr
{
    u64 val;
    Constant_Type type;
    b32 is_set;
    Constant_Format format;
} Constant_Expr;

typedef struct String_Expr
{
    String str;
    b32 wide;
} String_Expr;

/* typedef struct Type_Expr */
/* { */
/*     Node *type; */
/* } Type_Expr; */

typedef struct Unary_Expr
{
    Op op;
    struct Expr *operand;
} Unary_Expr;

typedef struct Binary_Expr
{
    struct Expr *lhs;
    Op op;
    struct Expr *rhs;
} Binary_Expr;;

typedef struct Ternary_Expr
{
    struct Expr *lhs;
    Op lop;
    struct Expr *mid;
    Op rop;
    struct Expr *rhs;
} Ternary_Expr;

struct Expr
{
    Expr_Kind kind;
    Expr *parent;
    int parens;
    union
    {
        Constant_Expr  constant;
        String_Expr    string;
        Unary_Expr     unary;
        Binary_Expr    binary;
        Ternary_Expr   ternary;
    };
};

Expr *pp_eval_expression(Expr *expr);
Op token_operator(Token token);
// Op parse_operator(char **str);
// void print_expression(Expr *expr);
char const *op_string(Op op);
Expr *pp_parse_expression(Token_Run expr, gbAllocator allocator, Preprocessor *pp, b32 is_directive);
void free_expr(gbAllocator allocator, Expr *expr);
//void print_expr(Expr *expr);
b32 num_positive(Constant_Expr expr);

#endif /* ifndef C_PREPROCESSOR_EXPRESSION_H */
