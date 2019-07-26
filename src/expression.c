#include "expression.h"

#include "gb/gb.h"
#include "util.h"
#include "parse_common.h"
#include "error.h"

Expr *make_expression(gbAllocator allocator, Expr_Kind kind, Expr *parent)
{
    Expr *expr = gb_alloc_item(allocator, Expr);

    expr->kind = kind;
    expr->parent = parent;

    return expr;
}

Expr *expr_constant(u64 val, Constant_Type type, Constant_Format format, b32 is_set, Expr *parent, gbAllocator allocator)
{
    Expr *expr = make_expression(allocator, Expr_Constant, parent);

    expr->constant.val = val;
    expr->constant.type = type;
    expr->constant.format = format;
    expr->constant.is_set = is_set;

    return expr;
}

/* Expr *expr_type(Node *type, Expr *parent, gbAllocator allocator) */
/* { */
/*     Expr *expr = make_expression(allocator, Expr_Type, parent); */
    
/*     expr->type.type = type; */

/*     return expr; */
/* } */

Expr *expr_string(String str, Expr *parent, gbAllocator allocator)
{
    Expr *expr = make_expression(allocator, Expr_String, parent);

    expr->string.str = str;

    return expr;
}

Expr *expr_unary(Op op, Expr *operand, Expr *parent, gbAllocator allocator)
{
    Expr *expr = make_expression(allocator, Expr_Unary, parent);

    expr->unary.op = op;
    expr->unary.operand = operand;

    return expr;
}

Expr *expr_binary(Expr *lhs, Op op, Expr *rhs, Expr *parent, gbAllocator allocator)
{
    Expr *expr = make_expression(allocator, Expr_Binary, parent);

    expr->binary.lhs = lhs;
    expr->binary.op = op;
    expr->binary.rhs = rhs;

    return expr;
}

String int_string(String str, Constant_Type *type, Constant_Format *format)
{
    String num;
    char *curr = str.start;
    
    num.start = curr;
    format->base = 10;
    if (gb_strncmp(curr, "0x", 2) == 0)
    {
        curr+=2;
        format->base = 16;
    }
    else if (gb_strncmp(curr, "0b", 2) == 0)
    {
        curr+=2;
        format->base = 2;
    }
    else if (curr[0] == '0' && str.len > curr-str.start+1)
    {
        curr++;
        format->base = 8;
    }

    format->sig_figs = 0;
    while ((format->base == 2  && gb_is_between(curr[0], '0', '1')) ||
           (format->base == 8  && gb_is_between(curr[0], '0', '7')) ||
           (format->base == 10 && gb_char_is_digit(curr[0]))        ||
           (format->base == 16 && gb_char_is_hex_digit(curr[0])))
    {
        format->sig_figs++;
        curr++;
    }

    num.len = curr - num.start;
    if (cstring_cmp(num, "0") == 0 && format->base == 8)
        format->base = 10;

    type->size = Constant_32;
    type->is_signed = true;
    for (;;)
    {
        if (curr[0] == 'u' || curr[0] == 'U')
            type->is_signed = false;
        else if (curr[0] == 'l' || curr[0] == 'L')
            type->size = type->size << 1;
        else
            break;
        curr++;
    }
    return num;
}

b32 num_positive(Constant_Expr expr)
{
    return (expr.val & (u64)1 << (expr.type.size*8 - 1)) == 0;
}

b32 num_greater_eq(Constant_Expr lhs, Constant_Expr rhs)
{
    b32 is_signed = lhs.type.is_signed && rhs.type.is_signed;
    if (is_signed)
    {
        b32 is_pos = num_positive(lhs);
        if (is_pos != num_positive(rhs))
            return is_pos;
    }

    return (lhs.val >= rhs.val);
}

char const *op_string(Op op)
{
    if (op.kind == Op_typecast)
        return "";
    return Op_Kind_Strings[op.kind];
}

Constant_Type convert_int_types(Constant_Type *lhs, Constant_Type *rhs)
{
    if (lhs->size < sizeof(int)) lhs->size = sizeof(int);
    if (rhs->size < sizeof(int)) rhs->size = sizeof(int);

    if (lhs->size == rhs->size && lhs->is_signed == rhs->is_signed) return *lhs;
    if (lhs->size == rhs->size)
    {
        lhs->is_signed = false;
        rhs->is_signed = false;
        return *lhs;
    }
    else if (lhs->size > rhs->size)
    {
        *rhs = *lhs;
        return *lhs;
    }
    else
    {
        *lhs = *rhs;
        return *lhs;
    }
}

/* void _print_expr(Expr *expr) */
/* { */
/*     if (expr->parens) */
/*         gb_printf("("); */
/*     if (expr->kind == Expr_Constant) */
/*     { */
/*         gb_printf("%ld", expr->constant.val); */
/*     } */
/*     else if (expr->kind == Expr_String) */
/*     { */
/*         gb_printf("\"%.*s\"", LIT(expr->string.str)); */
/*     } */
/*     else if(expr->kind == Expr_Unary) */
/*     { */
/*         gb_printf("%s ", Op_Kind_Strings[expr->unary.op.kind]); */
/*         _print_expr(expr->unary.operand); */
/*     } */
/*     else if (expr->kind == Expr_Binary) */
/*     { */
/*         _print_expr(expr->binary.lhs); */
/*         gb_printf(" %s ", Op_Kind_Strings[expr->binary.op.kind]); */
/*         _print_expr(expr->binary.rhs); */
/*     } */
/*     else if (expr->kind == Expr_Ternary) */
/*     { */
/*         _print_expr(expr->ternary.rhs); */
/*         gb_printf(" ? "); */
/*         _print_expr(expr->ternary.mid); */
/*         gb_printf(" : "); */
/*         _print_expr(expr->ternary.rhs); */
/*     } */
/*     if (expr->parens) */
/*         gb_printf(")"); */
/* } */

/* void print_expr(Expr *expr) */
/* { */
/*     _print_expr(expr); */
/*     gb_printf("\n"); */
/* } */

Op expr_get_op(Expr expr)
{
    switch (expr.kind)
    {
    case Expr_Unary: return expr.unary.op;
    case Expr_Binary: return expr.binary.op;
    case Expr_Ternary: return expr.ternary.lop;
    default: return (Op){0};
    }
}

Expr *pp_eval_expression(Expr *expr)
{
    switch (expr->kind)
    {
    case Expr_Constant:
    case Expr_String:
        return gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));
        break;
        
    case Expr_Unary: {
        if (expr->unary.op.kind == Op_sizeof)
            return gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));

        Expr *operand_expr = pp_eval_expression(expr->unary.operand);
        if (operand_expr->kind != Expr_Constant)
        {
            Expr *ret = gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));
            ret->unary.operand = operand_expr;
            return ret;
        }
        
        Constant_Expr operand = operand_expr->constant;
        u64 res;
        switch (expr->unary.op.kind)
        {
        case Op_Not    : res = !operand.val; break;
        case Op_Sub    : res = -operand.val; break;
        default: res = 0;
        }
        return expr_constant(res, DEFAULT_TYPE, DEFAULT_FORMAT, 1, 0, gb_heap_allocator());
    } break;

    case Expr_Binary: {
        Expr *lhs_expr = pp_eval_expression(expr->binary.lhs);
        Expr *rhs_expr = pp_eval_expression(expr->binary.rhs);
        if (lhs_expr->kind != Expr_Constant || rhs_expr->kind != Expr_Constant)
        {
            Expr *ret = gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));
            ret->binary.lhs = lhs_expr;
            ret->binary.rhs = rhs_expr;
            return ret;
        }
                
        Constant_Expr lhs = lhs_expr->constant;
        Constant_Expr rhs = rhs_expr->constant;
        u64 res;
        Constant_Type result_type = convert_int_types(&lhs.type, &rhs.type);
        switch (expr->binary.op.kind)
        {
        case Op_Add    : res = lhs.val +  rhs.val; break;
        case Op_Sub    : res = lhs.val -  rhs.val; break;
        case Op_Mul    : res = lhs.val *  rhs.val; break;
        case Op_Div    : res = lhs.val /  rhs.val; break;
        case Op_BOr    : res = lhs.val |  rhs.val; break;
        case Op_BAnd   : res = lhs.val &  rhs.val; break;
        case Op_Shl    : res = lhs.val << rhs.val; break;
        case Op_Shr    : res = lhs.val >> rhs.val; break;
        case Op_Xor    : res = lhs.val ^  rhs.val; break;
        case Op_EQ     : res = lhs.val == rhs.val; break;
        case Op_NEQ    : res = lhs.val != rhs.val; break;
        case Op_LT     : res = !num_greater_eq(lhs, rhs); break;
        case Op_GT     : res =  num_greater_eq(lhs, rhs) && lhs.val != rhs.val; break;
        case Op_LE     : res = !num_greater_eq(lhs, rhs) || lhs.val == rhs.val; break;
        case Op_GE     : res =  num_greater_eq(lhs, rhs); break;
        case Op_And    : res = lhs.val && rhs.val; break;
        case Op_Or     : res = lhs.val || rhs.val; break;
        default: res = 0;
        }
        return expr_constant(res, result_type, DEFAULT_FORMAT, 1, 0, gb_heap_allocator());
    }

    case Expr_Ternary: {
        Expr *lhs_expr = pp_eval_expression(expr->ternary.lhs);
        Expr *mid = pp_eval_expression(expr->ternary.mid);
        Expr *rhs = pp_eval_expression(expr->ternary.rhs);

        if (lhs_expr->kind != Expr_Constant)
        {
            Expr *ret = gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));
            ret->ternary.lhs = lhs_expr;
            ret->ternary.mid = mid;
            ret->ternary.rhs = rhs;
            return ret;
        }

        Constant_Expr lhs = lhs_expr->constant;
        Expr *res = lhs.val ? mid : rhs;
        return res;
    }
        
    default:
        return expr_constant(0, DEFAULT_TYPE, DEFAULT_FORMAT, 0, 0, gb_heap_allocator());
    }
}

int op_prec(Op_Kind kind)
{
    switch (kind)
    {
    case Op_Not: case Op_BNot: case Op_sizeof:
        return 10;
    case Op_Mul: case Op_Div: case Op_Mod:
        return 9; 
    case Op_Add: case Op_Sub:
        return 8;
    case Op_Shl: case Op_Shr:
        return 7;
    case Op_GT: case Op_LT: case Op_GE: case Op_LE:
        return 6;
    case Op_EQ: case Op_NEQ:
        return 5;
    case Op_BAnd:
        return 4;
    case Op_Xor:
        return 3;
    case Op_BOr:
        return 2;
    case Op_And:
        return 1;
    case Op_Or:
        return 0;
    case Op_TernL: case Op_TernR:
        return -1;
    default:
        return -2;
    }
}

Op token_operator(Token token)
{
    Op op = {0};
    String op_string = token.str;
    for (int i = 1; i < __Op_LiteralEnd; i++)
    {
        if (cstring_cmp(op_string, Op_Kind_Strings[i]) == 0)
        {
            op.kind = i;
            op.precedence = op_prec(i);
            return op;
        }
    }
    return op;
}

u64 char_lit_val(Token lit)
{
    GB_ASSERT(lit.kind == Token_Char || lit.kind == Token_Wchar);
    
    String lit_str = lit.str;
    switch (lit.kind)
    {
    case Token_Char:
        lit_str = (String){lit_str.start+1, lit_str.len-2};
        break;
    case Token_Wchar:
        lit_str = (String){lit_str.start+2, lit_str.len-3};
        break;
    default:
        break;
    }
    
    const u8 charconsts[] = {7, 8, 27, 12, 10, 13, 9, 11};
    if (lit_str.start[0] == '\\')
    {
        switch (lit_str.start[1])
        {
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            return str_to_int_base(string_slice(lit_str, 1, lit_str.len), 8);

        case 'x': return str_to_int_base(string_slice(lit_str, 2, lit_str.len), 16);

        case 'a': return charconsts[0];
        case 'b': return charconsts[1];
        case 'e':
        case 'E': return charconsts[2];
        case 'f': return charconsts[3];
        case 'n': return charconsts[4];
        case 'r': return charconsts[5];
        case 't': return charconsts[6];
        case 'v': return charconsts[7];
        }
        return 0;
    }
    else
    {
        return lit_str.start[0];
    }
}

void free_expr(gbAllocator alloc, Expr *expr)
{
    if (expr->kind == Expr_Unary)
    {
        free_expr(alloc, expr->unary.operand);
    }
    else if (expr->kind == Expr_Binary)
    {
        free_expr(alloc, expr->binary.lhs);
        free_expr(alloc, expr->binary.rhs);
    }
    else if (expr->kind == Expr_Ternary)
    {
        free_expr(alloc, expr->ternary.lhs);
        free_expr(alloc, expr->ternary.mid);
        free_expr(alloc, expr->ternary.rhs);
    }
    gb_free(alloc, expr);
}

int expr_precedence(Expr const *expr)
{
    switch (expr->kind)
    {
    case Expr_Unary:
        return expr->unary.op.precedence;
    case Expr_Binary:
        return expr->binary.op.precedence;
    default:
        return 0;
    }
}

void find_lower_precedence(Expr **curr_expr, const Op op)
{
    while ((*curr_expr)->parent && op.precedence < expr_precedence((*curr_expr)->parent))
        (*curr_expr) = (*curr_expr)->parent;
}

void update_parent_expr(Expr **curr_expr, Expr **new_expr, Expr **parent_expr)
{
    if (!(*curr_expr)->parent)
    {
        *parent_expr = *new_expr;
    }
    else
    {
        (*new_expr)->parent = (*curr_expr)->parent;
        if ((*curr_expr)->parent->kind == Expr_Ternary)
        {
            if ((*curr_expr)->parent->ternary.mid == (*curr_expr))
                (*curr_expr)->parent->ternary.mid = (*new_expr);
            else
                (*curr_expr)->parent->ternary.rhs = (*new_expr);
        }
        else if ((*curr_expr)->kind == Expr_Constant)
        {
            return;
        }
        else if ((*curr_expr)->parent->kind == Expr_Binary)
        {
            (*curr_expr)->parent->binary.rhs = (*new_expr);
        }
        else if ((*curr_expr)->parent->kind == Expr_Unary)
        {
            (*curr_expr)->parent->unary.operand = (*new_expr);
        }
    }
}

Expr *pp_parse_expression(Token_Run expr, gbAllocator allocator, Preprocessor *pp, b32 is_directive)
{
    if (!expr.start)
        return 0;
    
    u64 val = 0;
    Constant_Type val_type;
    Constant_Format val_format;

    Expr *parent_expr = gb_alloc_item(allocator, Expr);
    Expr *curr_expr = parent_expr;
    curr_expr->kind = Expr_Constant;

    Expr *val_expr = 0;

    while (expr.curr <= expr.end)
    {
        val_type = DEFAULT_TYPE;
        val_format = DEFAULT_FORMAT;
        val = 0;
        val_expr = 0;
        switch (expr.curr[0].kind)
        {
        case Token_Integer:
            val = str_to_int(int_string(expr.curr[0].str, &val_type, &val_format));
            val_expr = expr_constant(val, val_type, val_format, true, 0, allocator);
            expr.curr++;
            break;

        case Token_Wchar:
            val_type.is_signed = false;
            val_type.size = sizeof(wchar_t);
            val_format.is_char = true;
            val_expr = expr_constant(char_lit_val(expr.curr[0]), val_type, val_format, true, 0, allocator);
            expr.curr++;
            break;
            
        case Token_Char:
            val_type.is_signed = false;
            val_type.size = sizeof(char);
            val_format.is_char = true;
            val_expr = expr_constant(char_lit_val(expr.curr[0]), val_type, val_format, true, 0, allocator);
            expr.curr++;
            break;

        case Token_String:
            val_expr = expr_string(expr.curr[0].str, 0, allocator);
            expr.curr++;
            break;
            
        case Token_Invalid:
            expr.curr++;
            continue;

        case Token_Ident: {
            String ident_str = expr.curr[0].str;
            expr.curr++;
            if (cstring_cmp(ident_str, "defined") == 0 && is_directive)
            {
                Token open_paren = accept_token(&expr, Token_OpenParen);
                Token ident = *expr.curr++; //expect_token(&expr, Token_Ident);
                if (open_paren.str.start)
                    accept_token(&expr, Token_CloseParen);
                    
                ident_str = ident.str;
                Define def = pp_get_define(pp, ident_str);
                if (def.in_use)
                    val_expr = expr_constant(1, val_type, val_format, 1, 0, allocator);
                else
                    val_expr = expr_constant(0, val_type, val_format, 1, 0, allocator);
            }
            else
            {

                Define def = {0};
                if (pp)
                    def = pp_get_define(pp, ident_str);
                if (def.in_use && def.params && expr.curr[0].kind == Token_OpenParen)
                {
                    Token_Run macro = {expr.curr, expr.curr, expr.curr};
                    Token name = expr.curr[-1];
                    if (macro.end[0].kind == Token_OpenParen)
                    {
                        while(macro.end[0].kind != Token_CloseParen)
                            macro.end++;
                    }
                    expr.curr = macro.end+1;
                    gbArray(Token) res = pp_do_sandboxed_macro(pp, &macro, def, name);
                    Token_Run res_run = {res, res, res+gb_array_count(res)-1};
                    Expr *temp = pp_parse_expression(res_run, allocator, pp, is_directive);
                        
                    val_expr = pp_eval_expression(temp);
                    GB_ASSERT(val_expr->kind != Expr_String);
                    gb_array_free(res);
                    free_expr(allocator, temp);
                }
                else if (def.in_use)
                {
                    Expr *def_expr = pp_parse_expression(def.value, allocator, pp, is_directive);
                    Expr *def_expr_res = pp_eval_expression(def_expr);
                    GB_ASSERT(def_expr_res->constant.is_set);
                    val_expr = expr_constant(def_expr_res->constant.val, val_type, val_format, 1, 0, allocator);
                    free_expr(allocator, def_expr);
                    free_expr(allocator, def_expr_res);
                }
                else
                {
                    /* if (is_directive) */
                    /* { */
                    val_expr = expr_constant(0, val_type, val_format, 1, 0, allocator);
                    /* } */
                        
                    /*
                    else
                    {
                        expr.curr--;
                        Token type_name = expr.curr[0];
                        expr.curr++;
                        
                        int stars = 0;
                        while (expr.curr[0].kind == Token_Mul)
                        {
                            stars++;
                            expr.curr++;
                        }

                        Token_Run array = {0};
                        if (expr.curr[0].kind == Token_OpenBracket)
                        {
                            expr.curr++;  // [
                            array.start = array.curr = expr.curr;
                            while (expr.curr[0].kind != Token_CloseBracket)
                                expr.curr++;
                            array.end = expr.curr-1;
                            expr.curr++; // ]
                        }
                        Expr *array_expr = pp_parse_expression(array, allocator, pp, is_directive);
                        Node *type_identifier = gb_alloc_item(allocator, Node);
                        type_identifier->kind = NodeKind_Ident;
                        type_identifier->ident.ident = type_name.str;

                        Node *type = gb_alloc_item(allocator, Node);
                        type->kind = NodeKind_Type;
                        type->type.name = type_identifier;
                        type->type.stars = stars;
                        type->type.array = array_expr;
                        type->type.is_const = false;

                        Expr *type_expr = gb_alloc_item(allocator, Expr);
                        type_expr->kind = Expr_Type;
                        type_expr->type.type = type;

                        val_expr = expr_type(type, 0, allocator);
                    }
                    */
                }
            }
        } break;

        case Token_OpenParen: {
            int skip_parens = 0;
            Token_Run sub_expr = {expr.curr+1, expr.curr+1, 0};
            do {
                if      (expr.curr[0].kind == Token_OpenParen)  skip_parens++;
                else if (expr.curr[0].kind == Token_CloseParen) skip_parens--;
                expr.curr++;
            } while (skip_parens > 0);
            sub_expr.end = expr.curr-2;
            Expr *temp = pp_parse_expression(sub_expr, allocator, pp, is_directive);
            temp->parens = 1;
            /*
            if (temp->kind == Expr_Type && expr_get_op(*curr_expr).kind != Op_sizeof)
            {
                Op operator;
                operator.precedence = 10;
                operator.kind = Op_typecast;
                operator.type = temp;
                switch (curr_expr->kind)
                {
                case Expr_Constant: {
                    GB_ASSERT(!curr_expr->constant.is_set);
                    Expr *new_expr = expr_unary(operator, 0, 0, allocator);
                    if (!curr_expr->parent)
                        parent_expr = new_expr;
                    else
                    {
                        new_expr->parent = curr_expr->parent;
                        if (curr_expr->parent->kind == Expr_Ternary)
                        {
                            if (curr_expr->parent->ternary.mid == curr_expr)
                                curr_expr->parent->ternary.mid = new_expr;
                            else
                                curr_expr->parent->ternary.rhs = new_expr;
                        }
                    }
                    gb_free(allocator, curr_expr);
                    curr_expr = new_expr;
                } break;
                case Expr_Unary: {
                    Expr *new_expr = expr_unary(operator, 0, curr_expr, allocator);
                    curr_expr->unary.operand = new_expr;
                    curr_expr = new_expr;
                } break;
                case Expr_Binary: {
                    Expr *new_expr = expr_unary(operator, 0, curr_expr, allocator);
                    curr_expr->binary.rhs = new_expr;
                    curr_expr = new_expr;
                } break;
                default: break;
                }
                continue;
            }
            */
            val_expr = temp;
        } break;

            // Ignore backslash / Comments / Semi-colons
        case Token_BackSlash:
        case Token_Comment:
        case Token_Semicolon:
            expr.curr++;
            continue;
            
            // Assume next token is operator
        default: {
            
            // Parse operator
            Op operator = token_operator(expr.curr[0]);
            if (operator.kind == Op_Invalid)
            {
                error(expr.curr[0], "Got Invalid operator '%s'", TokenKind_Strings[expr.curr->kind]);
                gb_exit(1);
            }

            expr.curr++;

            // Special case: Beginning of ternary expression
            if (operator.kind == Op_TernL)
            {
                // Allocate new expression to be parent of current expression
                Expr *new_expr = gb_alloc_item(allocator, Expr);
                new_expr->kind = Expr_Ternary;
                Expr *lhs = curr_expr;

                // Climb the lhs expression until we find the top-most expression that is not a ternary
                while (lhs->parent && lhs->parent->kind != Expr_Ternary)
                    lhs = lhs->parent;
                new_expr->ternary.lhs = lhs;
                new_expr->ternary.lop = operator;
                
                // Set new parent expression if necessary
                if (!lhs->parent)
                    parent_expr = new_expr;
                else
                    new_expr->parent = lhs->parent;
                lhs->parent = new_expr;
                
                // Allocate new expression for 'true' path of ternary
                Expr *new_curr_expr = expr_constant(0, DEFAULT_TYPE, DEFAULT_FORMAT, true, new_expr, allocator);
                new_expr->ternary.mid = new_curr_expr;
                curr_expr = new_curr_expr;
            }
            // Special case: End of ternary expression
            else if (operator.kind == Op_TernR)
            {
                // Allocate new expression for 'false' path of ternary
                Expr *new_curr_expr = expr_constant(0, DEFAULT_TYPE, DEFAULT_FORMAT, true, 0, allocator);
                
                // climb current expression's parents until we find the parent ternary
                Expr *parent_ternary = curr_expr;
                while (parent_ternary->kind != Expr_Ternary)
                    parent_ternary = parent_ternary->parent;

                // Assign new expression to rhs of ternary
                new_curr_expr->parent = parent_ternary;
                parent_ternary->ternary.rhs = new_curr_expr;
                parent_ternary->ternary.rop = operator;
                
                // Set to current expression
                curr_expr = new_curr_expr;
            }
            // LHS Expr is Constant:
            else if (curr_expr->kind == Expr_Constant)
            {
                // If the constant is not set:
                //   Replace it with a unary expression
                if (!curr_expr->constant.is_set) // TODO: Replace with Expr_None (?)
                {
                    operator.precedence = 10; // General unary operator precedence
                    Expr *new_expr = expr_unary(operator, 0, 0, allocator);

                    // Update pointer to parent expression
                    update_parent_expr(&curr_expr, &new_expr, &parent_expr);

                    gb_free(allocator, curr_expr);
                    curr_expr = new_expr;
                }
                else
                {
                    Expr *new_expr = expr_binary(curr_expr, operator, 0, 0, allocator);

                    // Update pointer to parent expression
                    update_parent_expr(&curr_expr, &new_expr, &parent_expr);
                    
                    curr_expr->parent = new_expr;
                    curr_expr = new_expr;
                }
            }
            // LHS Expr is Unary
            else if (curr_expr->kind == Expr_Unary)
            {
                // If unary operand is not set:
                //   Create new expression as operand
                if (!curr_expr->unary.operand)
                {
                    Expr *new_expr = expr_unary(operator, 0, curr_expr, allocator);
                    curr_expr->unary.operand = new_expr;
                    curr_expr = new_expr;
                }
                // If current expression is parenthesized or has no parent:
                //   Create new binary expression as parent, LHS = curr_expr;
                else if (curr_expr->parens || !curr_expr->parent)
                {
                    Expr *new_expr = expr_binary(curr_expr, operator, 0, 0, allocator);
                    parent_expr = new_expr;
                    curr_expr->parent = new_expr;
                    curr_expr = new_expr;
                }
                else
                {
                    // Climb expression's parents until a precedence
                    // lower than the current operator's is found
                    curr_expr = curr_expr->parent; // Unary op precedence is always higher
                    find_lower_precedence(&curr_expr, operator);

                    Expr *new_expr = expr_binary(curr_expr, operator, 0, 0, allocator);

                    // Update pointer to parent expression
                    update_parent_expr(&curr_expr, &new_expr, &parent_expr);
                   
                    curr_expr->parent = new_expr;
                    curr_expr = new_expr;
                }
            }
            // LHS Expr is Binary
            else if (curr_expr->kind == Expr_Binary)
            {
                // If binary RHS operand is not set:
                //   Create a new unary expression as RHS
                if (!curr_expr->binary.rhs)
                {
                    Expr *new_expr = expr_unary(operator, 0, curr_expr, allocator);
                    curr_expr->binary.rhs = new_expr;
                    curr_expr = new_expr;
                }
                // If current expression is parenthesized or has LEQ precedence:
                //   Make RHS of current expression LHS of new_expr; Set to RHS of curr_expr;
                else if (curr_expr->parens || operator.precedence >= curr_expr->binary.op.precedence)
                {
                    Expr *new_expr = expr_binary(curr_expr->binary.rhs, operator, 0, curr_expr, allocator);
                    curr_expr->binary.rhs->parent = new_expr;
                    curr_expr->binary.rhs = new_expr;
                    curr_expr = new_expr;
                }
                else
                {
                    // Climb expression's parents until a precedence
                    // lower than the current operator's is found
                    find_lower_precedence(&curr_expr, operator);
                    
                    Expr *new_expr = expr_binary(curr_expr, operator, 0, 0, allocator);

                    // Update pointer to parent expression
                    update_parent_expr(&curr_expr, &new_expr, &parent_expr);
                    
                    curr_expr->parent = new_expr;
                    curr_expr = new_expr;
                }
            }
            continue;
        }
        }

        // If we parsed a constant:
        //   Assign it to the appropriate spot
        //   in the current expression
        if (curr_expr->kind == Expr_Constant)
        {
            update_parent_expr(&curr_expr, &val_expr, &parent_expr);
            free_expr(allocator, curr_expr);
            curr_expr = val_expr;
        }
        else if (curr_expr->kind == Expr_Unary)
        {
            curr_expr->unary.operand = val_expr;
            val_expr->parent = curr_expr;
        }
        else
        {
            curr_expr->binary.rhs = val_expr;
            val_expr->parent = curr_expr;
        }
    }
    
    return parent_expr;
}
