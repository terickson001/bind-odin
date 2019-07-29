#ifndef _PP_EXPRESSION_H_
#define _PP_EXPRESSION_H_

#include "gb/gb.h"

#include "string.h"
#include "preprocess.h"

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

typedef struct Expr Expr;
typedef struct Expr_Constant
{
	u64 val;
	Constant_Type type;
	Constant_Format fmt;
} Expr_Constant;
typedef struct Expr_String
{
	String str;
} Expr_String;
typedef struct Expr_Macro
{
	Token name;
    Token_Run args;
} Expr_Macro;
typedef struct Expr_Paren
{
	Expr *expr;
} Expr_Paren;
typedef struct Expr_Unary
{
	Token op;
	Expr *operand;
} Expr_Unary;
typedef struct Expr_Binary
{
	Token op;
	Expr *lhs;
	Expr *rhs;
} Expr_Binary;
typedef struct Expr_Ternary
{
	Expr *cond;
	Expr *then;
	Expr *els_;
} Expr_Ternary;

typedef enum ExprKind
{
    ExprKind_Invalid,
    ExprKind_Constant,
    ExprKind_String,
    ExprKind_Macro,
    ExprKind_Paren,
    ExprKind_Unary,
    ExprKind_Binary,
    ExprKind_Ternary
} ExprKind;

struct Expr
{
	ExprKind kind;
	union
	{
		Expr_Constant Constant;
		Expr_String   String;
		Expr_Macro    Macro;
		Expr_Paren    Paren;
		Expr_Unary    Unary;
		Expr_Binary   Binary;
		Expr_Ternary  Ternary;
	};
};

Expr *pp_parse_expression(Token_Run expr, gbAllocator allocator, Preprocessor *pp, b32 is_directive);
u64 pp_eval_expression(Preprocessor *pp, Expr *expr);
void free_expr(gbAllocator a, Expr *expr);

#endif