#include "parse.h"

#include "error.h"
#include "util.h"

#include <signal.h>

Parser make_parser(Tokenizer *t)
{
    Parser p;
    p.tokenizer = t;
    
    p.node_index = 0;
    p.alloc = gb_heap_allocator();

    p.type_table = hashmap_new(p.alloc);
    
    gb_array_init(p.file.all_nodes, p.alloc);
    gb_array_init(p.file.tpdefs,    p.alloc);
    gb_array_init(p.file.records,   p.alloc);
    gb_array_init(p.file.functions, p.alloc);
    gb_array_init(p.file.variables, p.alloc);

    gb_array_init(p.file.defines,   p.alloc);

    return p;
}

void init_parser(Parser *p)
{
    gb_array_init(p->start, gb_heap_allocator());

    for (;;)
    {
        Token token = get_token(p->tokenizer);
        gb_array_append(p->start, token);

        if (token.kind == Token_EOF)
            break;
    }
    p->curr = p->start;
    p->end = p->start + gb_array_count(p->start)-1;
}

void destroy_parser(Parser p)
{
    gb_array_free(p.start);

    gb_array_free(p.file.all_nodes);
    gb_array_free(p.file.tpdefs);
    gb_array_free(p.file.records);
    gb_array_free(p.file.functions);
    gb_array_free(p.file.variables);
}

Node *_make_node(Parser *p, NodeKind k)
{
    Node *n = gb_alloc_item(p->alloc, Node);
    n->kind = k;

    return n;
}
#define make_node(p_, k_) _make_node(p_, NodeKind_##k_)

Node *node_ident(Parser *p, Token token, String ident)
{
    Node *node = make_node(p, Ident);
    
    node->Ident.token = token;
    node->Ident.ident = ident;
    
    return node;
}

Node *node_typedef(Parser *p, Token token, Node *var_list)
{
    Node *node = make_node(p, Typedef);
    
    node->Typedef.token = token;
    node->Typedef.var_list = var_list;
    
    return node;
}

Node *node_basic_lit(Parser *p, Token token)
{
    Node *node = make_node(p, BasicLit);
    
    node->BasicLit.token = token;
    
    return node;
}

Node *node_string(Parser *p, gbArray(Token) strings)
{
    Node *node = make_node(p, String);

    node->String.strings = strings;

    return node;
}

Node *node_compound_lit(Parser *p, Node *fields, Token open, Token close)
{
    Node *node = make_node(p, CompoundLit);
    
    node->CompoundLit.fields = fields;
    node->CompoundLit.open   = open;
    node->CompoundLit.close  = close;
    
    return node;
}

Node *node_attribute(Parser *p, Node *name, Node *ident, Node *args)
{
    Node *node = make_node(p, Attribute);
    
    node->Attribute.name  = name;
    node->Attribute.ident = ident;
    node->Attribute.args  = args;
    
    return node;
}

Node *node_attr_list(Parser *p, gbArray(Node *) list)
{
    Node *node = make_node(p, AttrList);
    
    node->AttrList.list = list;
    
    return node;
}

Node *node_invalid_expr(Parser *p, Token start, Token end)
{
    Node *node = make_node(p, InvalidExpr);

    node->InvalidExpr.start = start;
    node->InvalidExpr.end   = end;

    return node;
}

Node *node_unary_expr(Parser *p, Token op, Node *operand)
{
    Node *node = make_node(p, UnaryExpr);

    node->UnaryExpr.op      = op;
    node->UnaryExpr.operand = operand;

    return node;
}

Node *node_binary_expr(Parser *p, Token op, Node *left, Node *right)
{
    Node *node = make_node(p, BinaryExpr);

    node->BinaryExpr.op    = op;
    node->BinaryExpr.left  = left;
    node->BinaryExpr.right = right;

    return node;
}

Node *node_ternary_expr(Parser *p, Node *cond, Node *then, Node *els_)
{
    Node *node = make_node(p, TernaryExpr);

    node->TernaryExpr.cond = cond;
    node->TernaryExpr.then = then;
    node->TernaryExpr.els_ = els_;

    return node;
}

Node *node_paren_expr(Parser *p, Node *expr, Token open, Token close)
{
    Node *node = make_node(p, ParenExpr);

    node->ParenExpr.expr  = expr;
    node->ParenExpr.open  = open;
    node->ParenExpr.close = close;

    return node;
}

Node *node_selector_expr(Parser *p, Token token, Node *expr, Node *selector)
{
    Node *node = make_node(p, SelectorExpr);

    node->SelectorExpr.token    = token;
    node->SelectorExpr.expr     = expr;
    node->SelectorExpr.selector = selector;

    return node;
}

Node *node_index_expr(Parser *p, Node *expr, Node *index, Token open, Token close)
{
    Node *node = make_node(p, IndexExpr);

    node->IndexExpr.expr  = expr;
    node->IndexExpr.index = index;
    node->IndexExpr.open  = open;
    node->IndexExpr.close = close;

    return node;
}

Node *node_call_expr(Parser *p, Node *func, Node *args, Token open, Token close)
{
    Node *node = make_node(p, CallExpr);

    node->CallExpr.func  = func;
    node->CallExpr.args  = args;
    node->CallExpr.open  = open;
    node->CallExpr.close = close;

    return node;
}

Node *node_type_cast(Parser *p, Node *type, Node *expr, Token open, Token close)
{
    Node *node = make_node(p, TypeCast);

    node->TypeCast.type  = type;
    node->TypeCast.expr  = expr;
    node->TypeCast.open  = open;
    node->TypeCast.close = close;

    return node;
}

Node *node_inc_dec_expr(Parser *p, Node *expr, Token op)
{
    Node *node = make_node(p, IncDecExpr);

    node->IncDecExpr.expr = expr;
    node->IncDecExpr.op = op;

    return node;
}

Node *node_expr_list(Parser *p, gbArray(Node *) list)
{
    Node *node = make_node(p, ExprList);

    node->ExprList.list = list;

    return node;
}

Node *node_var_decl(Parser *p, Node *type, Node *name, VarDeclKind kind)
{
    Node *node = make_node(p, VarDecl);
    node->VarDecl.type = type;
    node->VarDecl.name = name;
    node->VarDecl.kind = kind;
    return node;
}

Node *node_var_decl_list(Parser *p, gbArray(Node *) list, VarDeclKind kind)
{
    Node *node = make_node(p, VarDeclList);
    node->VarDeclList.list = list;
    node->VarDeclList.kind = kind;
    return node;
}

Node *node_enum_field(Parser *p, Node *name, Node *value)
{
    Node *node = make_node(p, EnumField);
    node->EnumField.name  = name;
    node->EnumField.value = value;
    return node;
}

Node *node_enum_field_list(Parser *p, gbArray(Node *) fields)
{
    Node *node = make_node(p, EnumFieldList);
    node->EnumFieldList.fields = fields;
    return node;
}

Node *node_function_decl(Parser *p, Node *ret_type, Node *name, Node *params, Node *body, b32 inlined)
{
    Node *node = make_node(p, FunctionDecl);
    node->FunctionDecl.ret_type = ret_type;
    node->FunctionDecl.name = name;
    node->FunctionDecl.params = params;
    node->FunctionDecl.body = body;
    node->FunctionDecl.inlined = inlined;
    return node;
}

Node *node_va_args(Parser *p, Token token)
{
    Node *node = make_node(p, VaArgs);
    node->VaArgs.token = token;
    return node;
}

Node *node_integer_type(Parser *p, gbArray(Token) specifiers)
{
    Node * node = make_node(p, IntegerType);

    node->IntegerType.specifiers = specifiers;

    return node;
}

Node *node_pointer_type(Parser *p, Token token, Node *type)
{
    Node *node = make_node(p, PointerType);
    node->PointerType.token = token;
    node->PointerType.type  = type;
    return node;
}

Node *node_array_type(Parser *p, Node *type, Node *count, Token open, Token close)
{
    Node *node = make_node(p, ArrayType);
    node->ArrayType.type  = type;
    node->ArrayType.count = count;
    node->ArrayType.open  = open;
    node->ArrayType.close = close;
    return node;
}

Node *node_const_type(Parser *p, Token token, Node *type)
{
    Node *node = make_node(p, ConstType);
    node->ConstType.token = token;
    node->ConstType.type  = type;
    return node;
}

Node *node_struct_type(Parser *p, Token token, Node *name, Node *fields)
{
    Node *node = make_node(p, StructType);
    node->StructType.token  = token;
    node->StructType.name   = name;
    node->StructType.fields = fields;
    return node;
}

Node *node_union_type(Parser *p, Token token, Node *name, Node *fields)
{
    Node *node = make_node(p, UnionType);
    node->UnionType.token  = token;
    node->UnionType.name   = name;
    node->UnionType.fields = fields;
    return node;
}

Node *node_enum_type(Parser *p, Token token, Node *name, Node *fields)
{
    Node *node = make_node(p, EnumType);
    node->EnumType.token  = token;
    node->EnumType.name   = name;
    node->EnumType.fields = fields;
    return node;
}

Node *node_function_type(Parser *p, Node *ret_type, Node *params, Token open, Token close, Token pointer)
{
    Node *node = make_node(p, FunctionType);
    node->FunctionType.ret_type = ret_type;
    node->FunctionType.params   = params;
    node->FunctionType.open     = open;
    node->FunctionType.close    = close;
    node->FunctionType.pointer  = pointer;
    return node;
}

Node *node_define(Parser *p, String name, Node *value)
{
    Node *node = make_node(p, Define);
    node->Define.name = name;
    node->Define.value = value;
    return node;
}

b32 try(Node *(*parse_func)(Parser *), Parser *p, Node **ret)
{
    Token *reset = p->curr;
    *ret = (*parse_func)(p);

    if ((*ret)->kind == NodeKind_Invalid)
    {
        p->curr = reset;
        *ret = 0;
        return false;
    }

    return true;
}


Node *alloc_ast_node(Parser *p, NodeKind k)
{
    Node *n = gb_alloc(p->alloc, gb_size_of(Node));
    n->kind = k;
    return n;
}

Node *get_base_type(Node *n)
{
    Node *type = n;
    for (;;)
    {
        switch (type->kind)
        {
        case NodeKind_PointerType:
            type = type->PointerType.type;
            continue;
        case NodeKind_ArrayType:
            type = type->ArrayType.type;
            continue;
        case NodeKind_ConstType:
            type = type->ConstType.type;
            continue;
        default: break;
        }
        break;
    }
    return type;
}

Token advance(Parser *p)
{
    Token prev = *(p->curr++);
    while (p->curr->kind == Token_Comment)
        p->curr++;
    return prev;
}

b32 allow(TokenKind k, Parser *p)
{
    if (p->curr->kind == k)
    {
        advance(p);
        return true;
    }

    return false;
}

b32 accept(TokenKind k, Parser *p, Token *t)
{
    if (p->curr->kind == k)
    {
        Token temp = advance(p);
        if (t) *t = temp;
        return true;
    }

    return false;
}

#define accept_one_of(_p, _tok_ref, ...)                                 \
    _accept_one_of((TokenKind[])__VA_ARGS__, sizeof((TokenKind[])__VA_ARGS__)/sizeof(TokenKind), _p, _tok_ref)
#define allow_ordered(_p, ...)                                              \
    _allow_ordered((TokenKind[])__VA_ARGS__, sizeof((TokenKind[])__VA_ARGS__)/sizeof(TokenKind), _p)
#define allow_unordered(_p, ...)                                            \
    _allow_unordered((TokenKind[])__VA_ARGS__, sizeof((TokenKind[])__VA_ARGS__)/sizeof(TokenKind), _p)


b32 _accept_one_of(TokenKind *kinds, int count, Parser *p, Token *ret)
{
    for (int i = 0; i < count; i++)
        if (accept(kinds[i], p, ret))
            return true;
    return false;
}

u8 _allow_ordered(TokenKind *kinds, int count, Parser *p)
{
    GB_ASSERT(count <= 8);
    u8 ret = 0;
    for (int i = 0; i < count; i++)
        if (allow(kinds[i], p))
            ret |= GB_BIT(i);
    return ret;
}

u8 _allow_unordered(TokenKind *kinds, int count, Parser *p)
{
    GB_ASSERT(count <= 8);
    u8 ret = 0;
    b32 found;
    for(;;)
    {
        found = false;
        for (int i = 0; i < count; i++)
        {
            if (ret & GB_BIT(i))
                continue;
            if (allow(kinds[i], p))
            {
                ret |= GB_BIT(i);
                found = true;
            }
        }
        if (!found) break;
    }
    return ret;
}

Token require(TokenKind k, Parser *p)
{
    if (p->curr->kind == k)
        return advance(p);

    String e = TokenKind_Strings[k];
    String g = TokenKind_Strings[p->curr->kind];
    syntax_error(*p->curr, "Expected '%.*s', got '%.*s'\n", LIT(e), LIT(g));

    if (p->curr->kind == Token_EOF)
        gb_exit(1);

    return (Token){0};
}

Token require_op(Parser *p)
{
    if (gb_is_between(p->curr->kind, Token__OperatorBegin, Token__OperatorEnd))
        return advance(p);

    String g = TokenKind_Strings[p->curr->kind];
    syntax_error(*p->curr, "Expected operator, got '%.*s'\n", LIT(g));

    if (p->curr->kind == Token_EOF)
        gb_exit(1);

    return (Token){0};
}

b32 expect(TokenKind k, Parser *p, Token *tok)
{
    if (p->no_backtrack)
    {
        *tok = require(k, p);
        return tok->kind != 0;
    }
    
    if (p->curr->kind == k)
    {
        if (tok)
            *tok = advance(p);
        else
            advance(p);
        return true;
    }
    
    if (p->curr->kind == Token_EOF)
        gb_exit(1);

    if (tok) *tok = (Token){0};
    return false;
}

b32 disallow_backtracking(Parser *p)
{
    b32 prev_state = p->no_backtrack;
    p->no_backtrack = true;
    return prev_state;
}

void restore_backtracking(Parser *p, b32 no_backtrack)
{
    p->no_backtrack = no_backtrack;
}

Node *parse_compound_literal(Parser *p);
Node *parse_operand(Parser *p);
Node *parse_expr_list(Parser *p);
Node *parse_index_expr(Parser *p, Node *operand);
Node *parse_call_expr(Parser *p, Node *operand);
Node *parse_selector_expr(Parser *p, Node *operand);
Node *parse_postfix_expr(Parser *p);
Node *parse_paren_expr(Parser *p);
Node *parse_cast_expr(Parser *p);
Node *parse_unary_expr(Parser *p, b32 parent_is_sizeof);
Node *parse_ternary_expr(Parser *p, Node *cond);
Node *parse_binary_expr(Parser *p, int max_prec);
Node *parse_assign_expr(Parser *p);
Node *parse_expression(Parser *p);
Node *parse_ident(Parser *p);
Node *parse_compound_statement(Parser *p);
Node *parse_record_fields(Parser *p);
Node *parse_record(Parser *p);
Node *parse_enum_fields(Parser *p);
Node *parse_enum(Parser *p);
Node *parse_parameter(Parser *p);
Node *parse_parameter_list(Parser *p);
Node *parse_integer_type(Parser *p);
Node *parse_type(Parser *p);
Node *parse_typedef(Parser *p);
Node *parse_attribute(Parser *p);
Node *parse_attributes(Parser *p);
Node *parse_function_type(Parser *p, Node *base_type, NodeKind *base_kind_out);
Node *parse_var_decl_list(Parser *p, Node *base_type, Node *first, VarDeclKind kind);
Node *parse_var_decl(Parser *p, VarDeclKind kind);
Node *parse_decl(Parser *p);

Node *parse_compound_literal(Parser *p)
{
    return &NODE_INVALID;
}
Node *parse_operand(Parser *p)
{
    switch (p->curr->kind)
    {
    case Token_Ident:
        return parse_ident(p);
        
    case Token_Integer:
    case Token_Float:
    case Token_Char:
    case Token_Wchar:
    case Token_String:
        return node_basic_lit(p, advance(p));

    case Token_OpenBrace:
        return parse_compound_literal(p);
        
    case Token_OpenParen:
        return parse_paren_expr(p);
        
    default: break;
    }
    return &NODE_INVALID;
}

Node *parse_expr_list(Parser *p)
{
    gbArray(Node *) expressions;
    gb_array_init(expressions, p->alloc);

    Node *expr;
    do {
        gb_array_append(expressions, parse_expression(p));
    } while (allow(Token_Comma, p));

    return node_expr_list(p, expressions);
}

Node *parse_index_expr(Parser *p, Node *operand)
{
    Token open, close;
    Node *index;
    open  = require(Token_OpenBracket,  p);
    index = parse_expression(p);
    close = require(Token_CloseBracket, p);

    return node_index_expr(p, operand, index, open, close);
}
Node *parse_call_expr(Parser *p, Node *operand)
{
    Token open, close;
    Node *expr_list;
    open  = require(Token_OpenParen,  p);
    if (p->curr->kind != Token_CloseParen)
        expr_list = parse_expr_list(p);
    close = require(Token_CloseParen, p);

    return node_call_expr(p, operand, expr_list, open, close);
}
Node *parse_selector_expr(Parser *p, Node *operand)
{
    Token token;
    Node *selector;
    token = advance(p);
    selector = parse_ident(p);
    return node_selector_expr(p, token, operand, selector);
}
Node *parse_postfix_expr(Parser *p)
{
    Node *expr = parse_operand(p);

    b32 loop = true;
    while (loop)
    {
        switch (p->curr->kind)
        {
        case Token_OpenBracket:
            expr = parse_index_expr(p, expr);
            break;

        
        case Token_OpenParen:
            expr = parse_call_expr(p, expr);
            break;
        
        case Token_Period:
        case Token_ArrowRight:
            expr = parse_selector_expr(p, expr);
            break;
        
        case Token_Inc:
        case Token_Dec:
            expr = node_inc_dec_expr(p, expr, advance(p));
            break;
        default: loop = false;
        }
    }
    
    return expr;
}

Node *parse_paren_expr(Parser *p)
{
    Token open, close;
    Node *expr;

    open  = require(Token_OpenParen,  p);
    expr  = parse_expression(p);
    close = require(Token_CloseParen, p);

    return node_paren_expr(p, expr, open, close);
}
Node *parse_cast_expr(Parser *p)
{
    Token open, close;
    Node *type, *expr;

    open  = require(Token_OpenParen,  p);
    type  = parse_type(p);
    close = require(Token_CloseParen, p);
    expr  = parse_expression(p);

    return node_type_cast(p, type, expr, open, close);
}

Node *try_type_expr(Parser *p)
{
    Token *reset = p->curr;
    
    i8 res = allow_unordered(p, {Token_const, Token_volatile});
    b32 is_const    = res & GB_BIT(0);
    b32 is_volatile = res & GB_BIT(1);

    Node *type;
    switch (p->curr->kind)
    {
    case Token_struct:
    case Token_union:
        type = parse_record(p);
        break;
        
    case Token_enum:
        type = parse_enum(p);
        break;
        
    case Token_int:
    case Token_char:
    case Token_signed:
    case Token_unsigned:
    case Token_short:
    case Token_long:
    case Token__int8:
    case Token__int16:
    case Token__int32:
    case Token__int64:
        type = parse_integer_type(p);
        break;
        
    case Token_Ident: {
        type = parse_ident(p);
        if (!hashmap_exists(p->type_table, type->Ident.token.str))
        {
            p->curr = reset;
            return 0;
        }
    } break;
        
    default:
        p->curr = reset;
        return 0;
    }

    if (p->curr[0].kind == Token_OpenParen
     && p->curr[1].kind == Token_Mul
     && p->curr[2].kind == Token_CloseParen)
        return parse_function_type(p, type, 0);
    

    Token specifier;
    while (accept_one_of(p, &specifier, {Token_Mul, Token_const, Token_volatile, Token_register, Token_unaligned}))
    {
        if (specifier.kind == Token_unaligned)
            specifier = require(Token_Mul, p);

        if (specifier.kind == Token_Mul)
            type = node_pointer_type(p, specifier, type);
        else if (specifier.kind == Token_const)
            type = node_const_type(p, specifier, type);
        // Discard `volatile` and `register`
    }

    Token open, close;
    Node *count;
    while (accept(Token_OpenBracket, p, &open))
    {
        count = parse_expression(p);
        close = require(Token_CloseBracket, p);
        type = node_array_type(p, type, count, open, close);
    }
    
    return type;
}

Node *parse_unary_expr(Parser *p, b32 parent_is_sizeof)
{
    Node *expr;
    b32 is_sizeof = false;
    switch (p->curr->kind)
    {
    case Token_sizeof:
    case Token_Alignof:
        is_sizeof = true;
    case Token_And:
    case Token_Add:
    case Token_Sub:
    case Token_Mul:
    case Token_Inc:
    case Token_Dec:
    case Token_Not:
    case Token_BitNot: {
        Token op = advance(p);
        return node_unary_expr(p, op, parse_unary_expr(p, is_sizeof));
    }
        
    case Token_OpenParen: {
        Token open, close;
        Node *type;
        open = require(Token_OpenParen, p);
        if ((type = try_type_expr(p)))
        {
            close = require(Token_CloseParen, p);
            if (parent_is_sizeof)
                return node_paren_expr(p, type, open, close);
            else
                return node_type_cast(p, type, parse_unary_expr(p, is_sizeof), open, close);
        }
        p->curr--; // re-add '('
    } break;
        
    default: break;
    }
    
    return parse_postfix_expr(p);
}
Node *parse_ternary_expr(Parser *p, Node *cond)
{
    Node *then, *els_;
    
    then = parse_expression(p);
    require(Token_Colon, p);
    els_ = parse_expression(p);
    
    return node_ternary_expr(p, cond, then, els_);
}
int op_precedence(TokenKind k)
{
    switch (k)
    {
    case Token_Mul:
    case Token_Quo:
    case Token_Mod:
        return 13;

    case Token_Add:
    case Token_Sub:
        return 12;

    case Token_Shl:
    case Token_Shr:
        return 11;

    case Token_Lt:
    case Token_LtEq:
    case Token_Gt:
    case Token_GtEq:
        return 10;

    case Token_CmpEq:
    case Token_NotEq:
        return 9;

    case Token_And:
        return 8;

    case Token_Xor:
        return 7;

    case Token_Or:
        return 6;

    case Token_CmpAnd:
        return 5;

    case Token_CmpOr:
        return 4;

    case Token_Question:
        return 3;

    case Token_Eq:
    case Token_AddEq:
    case Token_SubEq:
    case Token_MulEq:
    case Token_QuoEq:
    case Token_ModEq:
    case Token_AndEq:
    case Token_XorEq:
    case Token_OrEq:
    case Token_ShlEq:
    case Token_ShrEq:
        return 2;

    /*
    case Token_Comma:
        return 1;
    */

    default: break;
    }
    return 0;
}

Node *parse_binary_expr(Parser *p, int max_prec)
{
    Node *expr = parse_unary_expr(p, false);
    for (int prec = op_precedence(p->curr->kind); prec >= max_prec; prec--)
    {
        for (;;)
        {
            Token op = *p->curr;
            int op_prec = op_precedence(op.kind);
            if (op_prec != prec) break;
            require_op(p);
            
            if (op.kind == Token_Question)
            {
                expr = parse_ternary_expr(p, expr);
            }
            else
            {
                Node *rhs = parse_binary_expr(p, prec +1);
                if (!rhs)
                    syntax_error(op, "Expected epxression after binary operator");
                expr = node_binary_expr(p, op, expr, rhs);
            }
        }
    }
    
    return expr;
}
Node *parse_assign_expr(Parser *p)
{
    return &NODE_INVALID;
}
Node *parse_expression(Parser *p)
{
    return parse_binary_expr(p, 0+1);
}

Node *parse_ident(Parser *p)
{
    Token ident;
    ident = require(Token_Ident, p);

    return node_ident(p, ident, ident.str);
}

// Skipping for now
Node *parse_compound_statement(Parser *p)
{
    if (p->curr->kind != Token_OpenBrace)
        return &NODE_INVALID;

    int skip_parens = 0;
    do
    {
        if      (p->curr->kind == Token_OpenBrace)  skip_parens++;
        else if (p->curr->kind == Token_CloseBrace) skip_parens--;
        p->curr++;
    } while (skip_parens > 0);
    allow(Token_Comment, p);
    return make_node(p, CompoundStmt);
}

/*
Node *parse_var_decl_names(Parser *p, Node *base_type)
{
    
    gbArray(Node *) vars;
    Node *ident;

    gb_array_init(vars, p->alloc);

    Node *curr_type;
    do
    {
        curr_type = base_type;

        Node *fp;
        if (try(parse_function_type__variable, p, &fp))
        {
            Node *temp = fp;
            while (temp->kind != NodeKind_FunctionType)
            {
                switch (temp->kind)
                {
                case NodeKind_PointerType: temp = temp->PointerType.type; break;
                case NodeKind_ArrayType:   temp = temp->ArrayType.type;   break;
                case NodeKind_VarDecl:     temp = temp->VarDecl.type;     break;
                default: break;
                }
            }
            temp->FunctionType.ret_type = curr_type;
            gb_array_append(vars, fp);
        }
        else
        {
            Token pointer;
            while (expect(Token_Mul, p, &pointer))
                curr_type = node_pointer_type(p, pointer, curr_type);

            Node *name;
            if (try(parse_ident, p, &name));
            else return &NODE_INVALID;
            
            Token open, close;
            Node *count;
            while (expect(Token_OpenBracket, p, &open))
            {
                try(parse_expression, p, &count);
                if (expect(Token_CloseBracket, p, &close));
                else
                {
                    syntax_error(open, "Invalid count in array type declaration");
                    break;
                }
                curr_type = node_array_type(p, curr_type, count, open, close);
            }
            gb_array_append(vars, node_var_decl(p, curr_type, name, VarDecl_Variable)); 
        }
    } while (allow(Token_Comma, p));

    return node_var_decl_list(p, vars, VarDecl_Variable);
}
*/

/*
Node *parse_var_decl(Parser *p)
{
    allow(Token_extern, p);
    
    Node *base_type;
    Node *ident;

    if (try(parse_type, p, &base_type));
    else return &NODE_INVALID;

    Node *vars = parse_var_decl_names(p, base_type);
    
    return vars;
}
*/

/*
Node *parse_statement(Parser *p)
{
    Node *res;
    if (try(parse_var_decl, p, &res))
        return res;
        
    switch (p->curr.kind)
    {
    case Token_if:        return parse_if(p);
    case Token_for:       return parse_for(p);
    case Token_while:     return parse_while(p);
    case Token_return:    return parse_return(p);
    case Token_switch:    return parse_switch(p);
        
    case Token_continue:
    case Token_break:
        return parse_branch(p);
        
    case Token_Semicolon: return node_empty_statement(p, require(Token_Semicolon, p));
    case Token_OpenBrace: return parse_block(p);
    default: break;
    }
           
    if (!type)
        return &NODE_INVALID;
           
}
*/

/*
Node *parse_anonymous_record(Parser *p)
{
    Node *record;

    if      (try(parse_record, p, &record));
    else if (try(parse_enum,   p, &record));
    else return &NODE_INVALID;

    return node_var_decl(p, record, 0, VarDecl_AnonRecord);
}
*/

Node *parse_record_fields(Parser *p)
{
    Token open, close;
    open = require(Token_OpenBrace, p);

    if (p->curr->kind == Token_CloseBrace)
    {
        advance(p);
        return &NODE_INVALID;
    }
    
    gbArray(Node *) vars;
    gb_array_init(vars, p->alloc);

    Node *record;
    Node *var;
    do
    {
        record = 0;
        switch (p->curr->kind)
        {
        case Token_struct:
        case Token_union:
            record = parse_record(p);
            break;
        case Token_enum:
            record = parse_enum(p);
            break;
        default: break;
        }

        if (record)
        {
            if (p->curr->kind == Token_Semicolon)
                var = node_var_decl(p, record, 0 /* name */, VarDecl_AnonRecord);
            else
                var = parse_var_decl_list(p, record, 0 /* first */, VarDecl_Field);
        }
        else
        {
            var = parse_var_decl(p, VarDecl_Field);
        }

        switch (var->kind)
        {
        case NodeKind_VarDecl: gb_array_append(vars, var); break;
        case NodeKind_VarDeclList:
            for (int i = 0; i < gb_array_count(var->VarDeclList.list); i++)
                gb_array_append(vars, var->VarDeclList.list[i]);
            break;
        default: break;
        }

        require(Token_Semicolon, p);

    } while (!accept(Token_CloseBrace, p, &close));

    return node_var_decl_list(p, vars, VarDecl_Field);
}

Node *parse_record(Parser *p)
{
    Token t;
    switch (p->curr->kind)
    {
    case Token_struct: t = require(Token_struct, p); break;
    case Token_union:  t = require(Token_union,  p); break;
    default:
        error(*p->curr, "Expected 'struct' or 'union', got '%.*s'", LIT(TokenKind_Strings[p->curr->kind]));
        return &NODE_INVALID;
    }
    
    Node *attrs;
    if (p->curr->kind == Token_attribute)
        attrs = parse_attributes(p);
    
    Node *name = 0, *fields = 0;
    if (p->curr->kind == Token_Ident)
        name = parse_ident(p);
    if (p->curr->kind == Token_OpenBrace)
        fields = parse_record_fields(p);

    if (!name && !fields)
    {
        String prev = TokenKind_Strings[t.kind];
        String got  = TokenKind_Strings[p->curr->kind];
        syntax_error(t, "Expected name or start of fields after '%.*s', got '%.*s'\n", LIT(prev), LIT(got));
        return &NODE_INVALID;
    }
    
    switch(t.kind)
    {
    case Token_struct: return node_struct_type(p, t, name, fields);
    case Token_union:  return node_union_type (p, t, name, fields);
    default: break;
    }

    return &NODE_INVALID; // This shouldn't happen
}

Node *parse_enum_fields(Parser *p)
{
    Token open, close;
    close = require(Token_OpenBrace, p);

    if (p->curr->kind == Token_CloseBrace)
        return &NODE_INVALID;
    
    gbArray(Node *) fields;
    gb_array_init(fields, p->alloc);
    Node *name, *value;
    do {
        value = 0;
        name = parse_ident(p);

        if (allow(Token_Eq, p))
            value = parse_expression(p);

        gb_array_append(fields, node_enum_field(p, name, value));

        if (!allow(Token_Comma, p))
        {
            close = require(Token_CloseBrace, p);
            break;
        }
    } while (!accept(Token_CloseBrace, p, &close));

    return node_enum_field_list(p, fields);
}

Node *parse_enum(Parser *p)
{
    Token t;
    t = require(Token_enum, p);
    
    Node *attrs;
    if (p->curr->kind == Token_attribute)
        attrs = parse_attributes(p);
    
    Node *name = 0, *fields = 0;
    if (p->curr->kind == Token_Ident)
        name = parse_ident(p);
    if (p->curr->kind == Token_OpenBrace)
        fields = parse_enum_fields(p);

    if (!name && !fields)
    {
        String prev = TokenKind_Strings[t.kind];
        String got = TokenKind_Strings[p->curr->kind];
        syntax_error(t, "Expected name or start of fields after '%.*s', got '%.*s'\n", LIT(prev), LIT(got));
        return &NODE_INVALID;
    }

    return node_enum_type(p, t, name, fields);
}

/*
Node *parse_function_decl(Parser *p)
{
    allow(Token_extern, p);
    b32 inlined = allow(Token_inline, p);
    
    Node *ret_type, *name, *params, *body;
    ret_type = parse_type;
    if (try(parse_type, p, &ret_type));
    else return &NODE_INVALID;

    Node *func;
    if (try(parse_function_type__function, p, &func))
    {
        func->FunctionDecl.ret_type->FunctionType.ret_type = ret_type;
        func->FunctionDecl.inlined = inlined;
    }
    else if (try(parse_ident,          p, &name)
     &&      try(parse_parameter_list, p, &params))
    {
        func = node_function_decl(p, ret_type, name, params, 0, inlined);
    }
    else return &NODE_INVALID;

    Node *attrs;
    try_opt(parse_attributes, p, &attrs);
    
    if      (try(parse_compound_statement, p, &body));
    else if (allow(Token_Semicolon, p));
    else return &NODE_INVALID;

    func->FunctionDecl.body = body;
    return func;
}
*/

Node *parse_parameter(Parser *p)
{
    Node *type;
    Node *name = 0;

    if (p->curr->kind == Token_Ellipsis)
        return node_var_decl(p, node_va_args(p, require(Token_Ellipsis, p)), name, VarDecl_VaArgs);
    
    type = parse_type(p);

    if (p->curr->kind == Token_OpenParen)
    {
        Token open = *p->curr;
        NodeKind base_kind;
        Node *var = parse_function_type(p, type, &base_kind);
        switch (var->kind)
        {
        case NodeKind_VarDecl:
            var->VarDecl.kind = VarDecl_Parameter;
            return var;
        case NodeKind_FunctionType:
            return node_var_decl(p, var, 0 /* name */, VarDecl_NamelessParameter);
        case NodeKind_FunctionDecl:
            error(open, "Function Declaration is not allowed in parameter list");
        default:
            error(open,
             "Invalid node '%.*s' returned from parse_function_type in parse_parameter",
             LIT(node_strings[var->kind]));
            return &NODE_INVALID;
        }
    }
    
    if (p->curr->kind == Token_Ident)
        name = parse_ident(p);
    else
        return node_var_decl(p, type, name, VarDecl_NamelessParameter); 
    
    Token open, close;
    Node *count;
    while (accept(Token_OpenBracket, p, &open))
    {
        count = parse_expression(p);
        close = require(Token_CloseBracket, p);
        type = node_array_type(p, type, count, open, close);
    }
    
    return node_var_decl(p, type, name, VarDecl_Parameter);
}

Node *parse_parameter_list(Parser *p)
{
    Token open, close;
    open = require(Token_OpenParen, p);

    if (accept(Token_CloseParen, p, &close))
        return &NODE_INVALID;

    gbArray(Node *) params = 0;
    gb_array_init(params, p->alloc);
    Node *var_decl;
    do {
        var_decl = parse_parameter(p);
        gb_array_append(params, var_decl);
    } while (allow(Token_Comma, p));
    
    close = require(Token_CloseParen, p);
    
    return node_var_decl_list(p, params, VarDecl_Parameter);
}

// void *(*foo)(int, char)
//     -- Function pointer 'foo'
// void *(*foo(float, double))(int, char)
//     -- Function 'foo' with FP return type
// void *(*(*foo)(float, double))(int, char)
//     -- Function pointer 'foo' with FP return type
// void *(*(*(*foo)(char, float))(float, double))(int, char)
//     -- Function pointer 'foo' with FP return type, with FP return type [This one's a little much]
// void *(*foo(float, void *(*)(double)))(int, char)
//     -- Function 'foo' with FP param and return

// NOTE: returns a FunctionDecl with a FunctionType as the return type
//       e.g.: (*name)(fp_params, ...)
/* Node *parse_function_type__function(Parser *p) */
/* { */
/*     Node *fp_params; */
/*     Token fp_open, fp_close, fp_pointer; */
    
/*     Node *name, *params; */

/*     if (expect(Token_OpenParen,      p, &fp_open) */
/*      && expect(Token_Mul,            p, &fp_pointer) */
/*      &&    try(parse_ident,          p, &name) */
/*      &&    try(parse_parameter_list, p, &params) */
/*      && expect(Token_CloseParen,     p, &fp_close) */
/*      &&    try(parse_parameter_list, p, &fp_params)); */
/*     else return &NODE_INVALID; */

/*     Node *fp = node_function_type(p, 0 /\* ret_type *\/, fp_params, fp_open, fp_close, fp_pointer); */
/*     return node_function_decl(p, fp, name, params, 0 /\* body *\/, false /\* inlined *\/); */
/* } */

/* // NOTE: returns a VarDecl with a FunctionType as the type */
/* //       e.g.: (*name(params, ...))(fp_params, ...) */
/* Node *parse_function_type__variable(Parser *p) */
/* { */
/*     Node *fp_params; */
/*     Token fp_open, fp_close, fp_pointer; */

/*     Node *name; */

/*     if (expect(Token_OpenParen,      p, &fp_open) */
/*      && expect(Token_Mul,            p, &fp_pointer) */
/*      &&    try(parse_ident,          p, &name) */
/*      && expect(Token_CloseParen,     p, &fp_close) */
/*      &&    try(parse_parameter_list, p, &fp_params)); */
/*     else return &NODE_INVALID; */

/*     Node *fp = node_function_type(p, 0 /\* ret_type *\/, fp_params, fp_open, fp_close, fp_pointer); */
/*     return node_var_decl(p, fp, name, VarDecl_Variable); */
/* } */

/* Node *parse_function_type(Parser *p) */
/* { */
/*     Node *params; */
/*     Token open, close, pointer; */
    
/*     if (expect(Token_OpenParen,      p, &open) */
/*      && expect(Token_Mul,            p, &pointer) */
/*      && expect(Token_CloseParen,     p, &close) */
/*      &&    try(parse_parameter_list, p, &params)); */
/*     else return &NODE_INVALID; */

/*     return node_function_type(p, 0 /\* ret_type *\/, params, open, close, pointer); */
/* } */

Node *parse_integer_type(Parser *p)
{
    gbArray(Token) specifiers;
    gb_array_init(specifiers, p->alloc);
    
    for (;;)
    {
        switch (p->curr->kind)
        {
        case Token_int:
        case Token_char:
        case Token_signed:
        case Token_unsigned:
        case Token_long:
        case Token_short:
        case Token__int8:
        case Token__int16:
        case Token__int32:
        case Token__int64:
            gb_array_append(specifiers, advance(p));
            continue;
        default: break;
        }
        return node_integer_type(p, specifiers);
    }
}

Node *parse_type(Parser *p)
{
    u8 res = allow_unordered(p, {Token_volatile, Token_const, Token_register});
    b32 is_volatile = res & GB_BIT(0);
    b32 is_const    = res & GB_BIT(1);
    b32 is_register = res & GB_BIT(2);

    Node *type;
    switch (p->curr->kind)
    {
    case Token_struct:
    case Token_union:
        type = parse_record(p);
        break;
    case Token_enum:
        type = parse_enum(p);
        break;
    case Token_int:
    case Token_char:
    case Token_signed:
    case Token_unsigned:
    case Token_short:
    case Token_long:
    case Token__int8:
    case Token__int16:
    case Token__int32:
    case Token__int64:
        type = parse_integer_type(p);
        break;
    case Token_Ident:
        type = parse_ident(p);
        break;
    default:
        error(*p->curr, "Invalid token '%.*s' in type", LIT(TokenKind_Strings[p->curr->kind]));
        break;
    }

    if (p->curr[0].kind == Token_OpenParen
     && p->curr[1].kind == Token_Mul
     && p->curr[2].kind == Token_CloseParen)
        return parse_function_type(p, type, 0);
    

    Token specifier;
    while (accept_one_of(p, &specifier, {Token_Mul, Token_const, Token_volatile, Token_register, Token_unaligned}))
    {
        if (specifier.kind == Token_unaligned)
            specifier = require(Token_Mul, p);

        if (specifier.kind == Token_Mul)
            type = node_pointer_type(p, specifier, type);
        else if (specifier.kind == Token_const)
            type = node_const_type(p, specifier, type);
        // Discard `volatile` and `register`
    }

    Token open, close;
    Node *count;
    while (accept(Token_OpenBracket, p, &open))
    {
        count = parse_expression(p);
        close = require(Token_CloseBracket, p);
        type  = node_array_type(p, type, count, open, close);
    }
    
    return type;
}

Node *parse_typedef(Parser *p)
{
    Token token;
    Node *var_list;

    token    = require(Token_typedef, p);
    var_list = parse_var_decl(p, VarDecl_Variable);

    if (var_list->kind != NodeKind_VarDeclList)
        error(token, "Expected type declaration after 'typedef', got none");

    // Add to type table
    for (int i = 0; i < gb_array_count(var_list->VarDeclList.list); i++)
        hashmap_put(p->type_table, var_list->VarDeclList.list[i]->VarDecl.name->Ident.token.str, 0);
        
    
    require(Token_Semicolon, p);
    
    return node_typedef(p, token, var_list);
}

Node *parse_attribute(Parser *p)
{
    Node *name;
    Node *arg_ident = 0;
    Node *arg_expressions = 0;

    name = parse_ident(p);

    if (allow(Token_OpenParen, p))
    {
        arg_ident = parse_ident(p);
        if (allow(Token_Comma, p))
            arg_expressions = parse_expr_list(p);
    }

    return node_attribute(p, name, arg_ident, arg_expressions);
}

Node *parse_attributes(Parser *p)
{
    Token token, open, close;
    token = require(Token_attribute, p);
    open  = require(Token_OpenParen, p);
    require(Token_OpenParen, p);

    if (p->curr->kind == Token_CloseParen)
    {
        require(Token_CloseParen, p);
        close = require(Token_CloseParen, p);
        return &NODE_INVALID;
    }
    
    gbArray(Node *) list;
    gb_array_init(list, p->alloc);
    Node *attribute;
    do
    {
        gb_array_append(list, parse_attribute(p));
    } while(allow(Token_Comma, p));
    
    require(Token_CloseParen, p);
    close = require(Token_CloseParen, p);

    return node_attr_list(p, list);
}


Node *parse_function_type(Parser *p, Node *ret_type, NodeKind *base_kind_out)
{
    Token open, pointer, close;
    Node *params;
    open    = require(Token_OpenParen, p);
    pointer = require(Token_Mul,       p);

    Node *base_type = 0;
    Node *type = 0;
    Token pointer_type_pointer;
    while (accept(Token_Mul, p, &pointer_type_pointer))
    {
        type = node_pointer_type(p, pointer_type_pointer, type);
        if (!base_type) base_type = type;
    }
      
    Node *name = 0;
    Node *func_params = 0;
    if (p->curr->kind == Token_Ident)
    {
        name = parse_ident(p);
        if (p->curr->kind == Token_OpenParen)
            func_params = parse_parameter_list(p);
        
    }
    
    Token arr_open, arr_close;
    Node *arr_count;
    while (accept(Token_OpenBracket, p, &arr_open))
    {
        arr_count = parse_expression(p);
        arr_close = require(Token_CloseBracket, p);
        type  = node_array_type(p, type, arr_count, arr_open, arr_close);
        if (!base_type) base_type = type;
    }
    
    close  = require(Token_CloseParen, p);
    params = parse_parameter_list(p);

    Node *fp = node_function_type(p, ret_type, params, open, close, pointer);
    if (base_type)
    {
        switch (base_type->kind)
        {
        case NodeKind_ArrayType:   base_type->ArrayType.type   = fp; break;
        case NodeKind_PointerType: base_type->PointerType.type = fp; break;
        default: break;
        }
    }
    else
    {
        type = fp;
    }
    
    if (func_params)
    {
        if (base_kind_out) *base_kind_out = NodeKind_FunctionDecl;
        return node_function_decl(p, type, name, func_params, 0 /* body */, false /* inlined */);
    }
    else if (name)
    {
        if (base_kind_out) *base_kind_out = NodeKind_VarDecl;
        return node_var_decl(p, type, name, VarDecl_Variable);
    }
    else
    {
        if (base_kind_out) *base_kind_out = NodeKind_FunctionType;
        return type;
    }
}

Node *parse_var_decl_list(Parser *p, Node *base_type, Node *first, VarDeclKind kind)
{
    if (!base_type)
    {
        if (first)
        {
            gb_printf_err("\e[31mINTERNAL ERROR:\e[0m No base_type supplied to parse_var_decl_list\n");
            gb_exit(1);
        }
        return parse_var_decl(p, kind);
    }

    b32 got_first = first != 0;
    
    gbArray(Node *) vars;
    gb_array_init(vars, p->alloc);
    if (got_first)
    {
        gb_array_append(vars, first);
        if (!allow(Token_Comma, p))
            return node_var_decl_list(p, vars, kind);
    }
    
    Node *curr_type, *name;
    do
    {
        curr_type = base_type;

        if (!got_first)
        {
            Token pointer;
            while (accept(Token_Mul, p, &pointer))
                curr_type = node_pointer_type(p, pointer, curr_type);
            got_first = true;
        }
        
        if (p->curr->kind == Token_OpenParen) // Function pointer
        {
            Token open = *p->curr;
            NodeKind base_kind;
            Node *node = parse_function_type(p, curr_type, &base_kind);
            switch (base_kind)
            {
            case NodeKind_VarDecl:
                curr_type = node->VarDecl.type;
                name = node->VarDecl.name;
                break;
            case NodeKind_FunctionType:
                error(open, "Expected name in function pointer declaration");
                return &NODE_INVALID;
            default:
                error(open, "Expected variable declaration, got a %.*s", LIT(node_strings[node->kind]));
                return &NODE_INVALID;
            }
            gb_free(p->alloc, node);
        }
        else
        {
            Token pointer;
            while (accept(Token_Mul, p, &pointer))
                curr_type = node_pointer_type(p, pointer, curr_type);

            name = parse_ident(p);

            Token open, close;
            Node *count;
            while (accept(Token_OpenBracket, p, &open))
            {
                count = parse_expression(p);
                close = require(Token_CloseBracket, p);
                curr_type = node_array_type(p, curr_type, count, open, close);
            }
        }
        
        gb_array_append(vars, node_var_decl(p, curr_type, name, kind));
    } while (allow(Token_Comma, p));

    return node_var_decl_list(p, vars, kind);
}

Node *parse_var_decl(Parser *p, VarDeclKind kind)
{
    Node *type, *name;
    type = parse_type(p);

    Node *base_type = get_base_type(type);

    if (p->curr->kind == Token_OpenParen) // Function pointer
    {
        Token open = *p->curr;
        NodeKind base_kind;
        Node *node = parse_function_type(p, type, &base_kind);
        switch (base_kind)
        {
        case NodeKind_VarDecl:
            type = node->VarDecl.type;
            name = node->VarDecl.name;
            break;
        case NodeKind_FunctionType:
            error(open, "Expected name in function pointer declaration");
            return &NODE_INVALID;
        default:
            error(open, "Expected variable declaration, got a %.*s", LIT(node_strings[node->kind]));
            return &NODE_INVALID;
        }
        gb_free(p->alloc, node);
    }
    else
    {
        name = parse_ident(p);
        /* if (name->Ident.token.loc.line == 61) */
        /*     raise(SIGINT); */
    }
    
    Token open, close;
    Node *count;
    while (accept(Token_OpenBracket, p, &open))
    {
        count = parse_expression(p);
        close = require(Token_CloseBracket, p);
        type  = node_array_type(p, type, count, open, close);
    }
    
    Node *first = node_var_decl(p, type, name, kind);
    Node *list = parse_var_decl_list(p, base_type, first, kind);

    return list;
}

Node *parse_string(Parser *p)
{
    gbArray(Token) strings;
    gb_array_init(strings, p->alloc);

    Token curr_str;
    while (accept(Token_String, p, &curr_str))
        gb_array_append(strings, curr_str);

    return node_string(p, strings); 
}

Node *parse_decl_spec(Parser *p)
{
    Token tok = require(Token_declspec, p);
    Token open, close;
    open = require(Token_OpenParen, p);

    Node *name;
    Node *arg = 0;

    name = parse_ident(p);
    if (allow(Token_OpenParen, p))
    {
        if (cstring_cmp(name->Ident.token.str, "align") == 0)
            arg = parse_expression(p);
        else if (cstring_cmp(name->Ident.token.str, "allocate") == 0)
            arg = parse_string(p);
        else if (cstring_cmp(name->Ident.token.str, "code_seg") == 0)
            arg = parse_string(p);
        else if (cstring_cmp(name->Ident.token.str, "property") == 0)
            arg = parse_expr_list(p);
        else if (cstring_cmp(name->Ident.token.str, "uuid") == 0)
            arg = parse_string(p);
        else if (cstring_cmp(name->Ident.token.str, "deprecated") == 0)
            arg = parse_string(p);
        else
            error(tok, "'__declspec(%.*s)' takes no arguments", LIT(name->Ident.token.str));
        require(Token_CloseParen, p);
    }
    close = require(Token_CloseParen, p);

    return 0;
}

Node *parse_decl(Parser *p)
{
    if (p->curr->kind == Token_declspec)
        parse_decl_spec(p);

    allow(Token_extern, p);
    allow(Token_static, p);
    b32 is_inline = accept_one_of(p, 0, {Token_inline, Token__inline, Token__inline_, Token__forceinline});

    Node *type, *name;
    type = parse_type(p);

    Token call_conv;
    accept_one_of(p, &call_conv, {Token_cdecl, Token_stdcall});

    Token sc;
    if (accept(Token_Semicolon, p, &sc))
    {
        switch (type->kind)
        {
        case NodeKind_StructType:
        case NodeKind_UnionType:
        case NodeKind_EnumType:
            return type;
        default:
            error(sc, "Expected name after type declaration, got ';'");
            gb_free(p->alloc, type);
            return &NODE_INVALID;
        }
    }

    b32 is_variable = false;
    b32 is_function = false;
    
    Node *base_type = get_base_type(type); // base type for remaining elements of var list
    Node *params;
    if (p->curr->kind == Token_OpenParen) // function pointer
    {
        Token open = *p->curr;
        NodeKind base_kind;
        Node *node = parse_function_type(p, type, &base_kind);
        switch (node->kind)
        {
        case NodeKind_FunctionDecl:
            is_function = true;
            type   = node->FunctionDecl.ret_type;
            name   = node->FunctionDecl.name;
            params = node->FunctionDecl.params;
            break;
        case NodeKind_VarDecl:
            is_variable = true;
            type = node->VarDecl.type;
            name = node->VarDecl.name;
            break;
        case NodeKind_FunctionType:
            error(open, "Expected name in function pointer declaration");
            return &NODE_INVALID;
        default:
            error(open, "Expected function pointer declaration");
            return &NODE_INVALID;
        }
        gb_free(p->alloc, node);
    }
    else
    {
        name = parse_ident(p);
        if (p->curr->kind == Token_OpenParen) // function declaration
        {
            is_function = true;
            params = parse_parameter_list(p);
        }
        else
        {
            is_variable = true;
            Token open, close;
            Node *count;
            while (accept(Token_OpenBracket, p, &open))
            {
                count = parse_expression(p);
                close = require(Token_CloseBracket, p);
                type = node_array_type(p, type, count, open, close);
            }
        }
    }
    
    Node *body = 0;
    Node *list = 0;
    if (is_function && p->curr->kind == Token_OpenBrace)  // function body
        body = parse_compound_statement(p);
    else if (is_variable) // variable list
        list = parse_var_decl_list(p, type, node_var_decl(p, type, name, VarDecl_Variable), VarDecl_Variable);

    if (!(is_function && body))
        sc = require(Token_Semicolon, p);

    if (is_function)
        return node_function_decl(p, type, name, params, body, is_inline);
    else if (is_variable)
        return list;
    else
        error(sc, "Declaration is not variable or function");
    return &NODE_INVALID;
}

void parse_file(Parser *p)
{
    Node *n;
    int index = 0;
    while (p->curr->kind != Token_EOF)
    {
        switch (p->curr->kind)
        {
        case Token_typedef:
            n = parse_typedef(p);
            break;
        default:
            n = parse_decl(p);
            break;
        }

        if (!n)
        {
            error(*p->curr,
             "Top level element is neither a declaration, nor function definition. Got '%.*s'",
             LIT(p->curr->str));
        }

        n->index = index++;
        switch (n->kind)
        {
        case NodeKind_VarDeclList:
            for (int i = 0; i < gb_array_count(n->VarDeclList.list); i++)
            {
                gb_array_append(p->file.variables, n->VarDeclList.list[i]);
                gb_array_append(p->file.all_nodes, n->VarDeclList.list[i]);
            }
            continue;
        case NodeKind_FunctionDecl:
            gb_array_append(p->file.functions, n);
            break;
        case NodeKind_Typedef:
            gb_array_append(p->file.tpdefs, n);
            break;
        case NodeKind_StructType:
        case NodeKind_UnionType:
        case NodeKind_EnumType:
            gb_array_append(p->file.records, n);
            break;
        default:
            error(*p->curr, "Unexpected top level element, '%.*s'", LIT(node_strings[n->kind]));
            gb_exit(1);
        }

        gb_array_append(p->file.all_nodes, n);
    }
}

Node *parse_token_run(Parser *p, Node *(*parse_func)(Parser *), Token_Run run)
{
    Token_Run old = {p->start, p->curr, p->end};
    
    p->start = run.start;
    p->curr  = run.curr;
    p->end   = run.end;
    Node *ret = (*parse_func)(p);
    p->start = old.start;
    p->curr  = old.curr;
    p->end   = old.end;
    
    return ret;
}

void parse_defines(Parser *p, gbArray(Define) defines)
{
    String name;
    Node *value;
    for (int i = 0; i < gb_array_count(defines); i++)
    {
        name = defines[i].key;
        value = parse_token_run(p, parse_expression, defines[i].value);
        gb_array_append(p->file.defines, node_define(p, name, value));
    }
}
