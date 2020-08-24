#include "expression.h"

#include "gb/gb.h"
#include "util.h"
#include "parse_common.h"
#include "error.h"

Token advance_expr(Token_Run *expr)
{
	Token prev = *(expr->curr++);
	while (expr->curr <= expr->end && 
           (expr->curr->kind == Token_Comment
            || expr->curr->kind == Token_BackSlash))
		expr->curr++;
	return prev;
}

int pp_op_precedence(TokenKind k)
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
        
        default: break;
    }
    return 0;
}

Expr *make_expression(Preprocessor *pp, ExprKind kind)
{
	Expr *expr = gb_alloc_item(pp->allocator, Expr);
    
	expr->kind = kind;
    
	return expr;
}

Expr *constant_expr(Preprocessor *pp, u64 val, Constant_Type type, Constant_Format fmt)
{
	Expr *expr = make_expression(pp, ExprKind_Constant);
    
	expr->Constant.val  = val;
	expr->Constant.type = type;
	expr->Constant.fmt  = fmt;
    
	return expr;
}

Expr *string_expr(Preprocessor *pp, String str)
{
	Expr *expr = make_expression(pp, ExprKind_String);
    
	expr->String.str = str;
    
	return expr;
}

Expr *macro_expr(Preprocessor *pp, Token name, Token_Run args)
{
	Expr *expr = make_expression(pp, ExprKind_Macro);
    
	expr->Macro.name = name;
	expr->Macro.args = args;
    
	return expr;
}

Expr *paren_expr(Preprocessor *pp, Expr *sub_expr)
{
	Expr *expr = make_expression(pp, ExprKind_Paren);
    
	expr->Paren.expr = sub_expr;
    
	return expr;
}

Expr *unary_expr(Preprocessor *pp, Token op, Expr *operand)
{
	Expr *expr = make_expression(pp, ExprKind_Unary);
    
	expr->Unary.op = op;
	expr->Unary.operand = operand;
    
	return expr;
}

Expr *binary_expr(Preprocessor *pp, Token op, Expr *lhs, Expr *rhs)
{
	Expr *expr = make_expression(pp, ExprKind_Binary);
    
	expr->Binary.op  = op;
	expr->Binary.lhs = lhs;
	expr->Binary.rhs = rhs;
    
	return expr;
}

Expr *ternary_expr(Preprocessor *pp, Expr *cond, Expr *then, Expr *els_)
{
	Expr *expr = make_expression(pp, ExprKind_Ternary);
    
	expr->Ternary.cond = cond;
	expr->Ternary.then = then;
	expr->Ternary.els_ = els_;
    
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

Expr *_pp_parse_expression(Token_Run *expr, Preprocessor *pp);
Expr *pp_parse_operand(Token_Run *expr, Preprocessor *pp)
{
	Constant_Type type = DEFAULT_TYPE;
	Constant_Format fmt = DEFAULT_FORMAT;
    
	switch (expr->curr->kind)
	{
        case Token_Integer: {
            u64 val = str_to_int(int_string(expr->curr->str, &type, &fmt));
            advance_expr(expr);
            return constant_expr(pp, val, type, fmt);
        }
        
        case Token_Char: {
            type.is_signed = false;
            type.size = sizeof(char);
            fmt.is_char = true;
            u64 val = char_lit_val(expr->curr[0]);
            advance_expr(expr);
            return constant_expr(pp, val, type, fmt);
        }
        
        case Token_Wchar: {
            type.is_signed = false;
            type.size = sizeof(wchar_t);
            fmt.is_char = true;
            u64 val = char_lit_val(expr->curr[0]);
            advance_expr(expr);
            return constant_expr(pp, val, type, fmt);
        }
        
        case Token_String: {
            String str = expr->curr->str;
            advance_expr(expr);
            return string_expr(pp, str);
        }
        
        default: 
		if (!gb_is_between(expr->curr->kind, Token__KeywordBegin, Token__KeywordEnd))
			break;
        case Token_Ident: {
            Token name = *expr->curr;
            advance_expr(expr);
            Token_Run args = {0};
            if (expr->curr->kind == Token_OpenParen)
            {
                args.start = args.curr = expr->curr;
                int skip_parens = 0;
                do
                {
                    if      (expr->curr->kind == Token_OpenParen)  skip_parens++;
                    else if (expr->curr->kind == Token_CloseParen) skip_parens--;
                    advance_expr(expr);
                } while (skip_parens > 0);
                args.end = expr->curr-1;
            }
            return macro_expr(pp, name, args);
        }
        
        case Token_OpenParen: {
            advance_expr(expr);
            Expr *expression = paren_expr(pp, _pp_parse_expression(expr, pp));
            Token close = advance_expr(expr);
            if (close.kind != Token_CloseParen)
                syntax_error(close, "Expected ')', got '%.*s'", LIT(TokenKind_Strings[close.kind]));
            return expression;
        }
	}
	return 0;
}

Expr *pp_parse_unary_expression(Token_Run *expr, Preprocessor *pp)
{
	Node *expression;
	switch (expr->curr->kind)
	{
        case Token_Ident:
		if (cstring_cmp(expr->curr->str, "defined") != 0)
			break;
        case Token_And:
        case Token_Add:
        case Token_Sub:
        case Token_Mul:
        case Token_Not:
        case Token_BitNot: {
            Token op = advance_expr(expr);
            return unary_expr(pp, op, pp_parse_unary_expression(expr, pp));
        }
        
        default: break;
	}
    
	return pp_parse_operand(expr, pp);
}
Expr *pp_parse_ternary_expr(Token_Run *expr, Preprocessor *pp, Expr *cond)
{
	Expr *then, *els_;
	then = _pp_parse_expression(expr, pp);
	Token colon = advance_expr(expr);
	els_ = _pp_parse_expression(expr, pp);
    
	if (colon.kind != Token_Colon)
		syntax_error(colon, "Expected ':', got '%.*s'", LIT(TokenKind_Strings[colon.kind]));
    
	return ternary_expr(pp, cond, then, els_);
}
Expr *pp_parse_binary_expr(Token_Run *expr, Preprocessor *pp, int max_prec)
{
	Expr *expression = pp_parse_unary_expression(expr, pp);
	if (expr->curr > expr->end) return expression;
	for (int prec = pp_op_precedence(expr->curr->kind); prec >= max_prec; prec--)
	{
		if (expr->curr > expr->end) break;
		for (;;)
		{
			Token op = *expr->curr;
			int op_prec = pp_op_precedence(op.kind);
			if (op_prec != prec) break;
			if (op_prec == 0) error(op, "Expected operator, got '%.*s'", LIT(TokenKind_Strings[expr->curr->kind]));
			advance_expr(expr);
            
			if (op.kind == Token_Question)
			{
				expression = pp_parse_ternary_expr(expr, pp, expression);
			}
			else
			{
				Expr *rhs = pp_parse_binary_expr(expr, pp, prec +1);
				if (!rhs)
					syntax_error(op, "Expected expression after binary operator");
				expression = binary_expr(pp, op, expression, rhs);
			}
		}
	}
    
	return expression;
}

Expr *_pp_parse_expression(Token_Run *expr, Preprocessor *pp)
{
	if (expr->curr->kind == Token_BackSlash || expr->curr->kind == Token_Comment)
		advance_expr(expr);
	return pp_parse_binary_expr(expr, pp, 0+1);
}
Expr *pp_parse_expression(Token_Run expr, gbAllocator allocator, Preprocessor *pp, b32 is_directive)
{
	return _pp_parse_expression(&expr, pp);
}


b32 num_positive(Expr_Constant expr)
{
    return (expr.val & (u64)1 << (expr.type.size*8 - 1)) == 0;
}

b32 num_greater_eq(Expr_Constant lhs, Expr_Constant rhs)
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

u64 _pp_expr_defined(Preprocessor *pp, Expr *expr)
{
	while (expr->kind == ExprKind_Paren)
		expr = expr->Paren.expr;
    
	if (expr->kind != ExprKind_Macro)
		gb_printf_err("ERROR: Operand of 'defined' is not an identifer\n");
    
	Define def = pp_get_define(pp, expr->Macro.name.str);
	return def.in_use;
}

Expr *_pp_eval_expression(Preprocessor *pp, Expr *expr);
Expr *_pp_expand_macro_expr(Preprocessor *pp, Expr *expr)
{
	Define def = pp_get_define(pp, expr->Macro.name.str);
	if (!def.in_use)
		return constant_expr(pp, 0, DEFAULT_TYPE, DEFAULT_FORMAT);
    //error(expr->Macro.name, "Macro '%.*s' is not defined", LIT(expr->Macro.name.str));
    //gb_printf_err("ERROR: Macro '%.*s' is not defined\n", LIT(expr->Macro.name.str));
    
	Expr *parsed = 0;
	if (def.params)
	{
		if (!expr->Macro.args.start)
			gb_printf_err("ERROR: Macro '%.*s' requires arguments\n", LIT(expr->Macro.name.str));
	    gbArray(Token) res = pp_do_sandboxed_macro(pp, &expr->Macro.args, def, expr->Macro.name);
	    Token_Run res_run = {res, res, res+gb_array_count(res)-1};
		parsed = _pp_parse_expression(&res_run, pp);
	}
	else if (def.value.start)
	{
		Token_Run value = def.value;
		parsed = _pp_parse_expression(&value, pp);
	}
    
    return _pp_eval_expression(pp, parsed);
}

Expr *_pp_eval_expression(Preprocessor *pp, Expr *expr)
{
	switch (expr->kind)
    {
        case ExprKind_Constant:
        return gb_alloc_copy(gb_heap_allocator(), expr, sizeof(Expr));
        break;
        
        case ExprKind_Macro: {
            return _pp_expand_macro_expr(pp, expr);
        }
        
        case ExprKind_Unary: {
            if (expr->Unary.op.kind == Token_Ident &&
                cstring_cmp(expr->Unary.op.str, "defined") == 0)
            {
                // gb_printf("OPERATOR: defined\n");
                u64 res = _pp_expr_defined(pp, expr->Unary.operand);
                return constant_expr(pp, res, DEFAULT_TYPE, DEFAULT_FORMAT);
            }
            
            Expr *operand_expr = _pp_eval_expression(pp, expr->Unary.operand);
            
            Expr_Constant operand = operand_expr->Constant;
            u64 res;
            switch (expr->Unary.op.kind)
            {
                case Token_Not: res = !operand.val; break;
                case Token_Sub: res = -operand.val; break;
                default: res = 0; break;
            }
            return constant_expr(pp, res, DEFAULT_TYPE, DEFAULT_FORMAT);
        } break;
        
        case ExprKind_Paren: {
            return _pp_eval_expression(pp, expr->Paren.expr);
        }
        case ExprKind_Binary: {
            Expr *lhs_expr = _pp_eval_expression(pp, expr->Binary.lhs);
            Expr *rhs_expr = _pp_eval_expression(pp, expr->Binary.rhs);
            Expr_Constant lhs = lhs_expr->Constant;
            Expr_Constant rhs = rhs_expr->Constant;
            
            u64 res;
            Constant_Type result_type = convert_int_types(&lhs.type, &rhs.type);
            
            switch (expr->Binary.op.kind)
            {
                case Token_Add   : res = lhs.val +  rhs.val; break;
                case Token_Sub   : res = lhs.val -  rhs.val; break;
                case Token_Mul   : res = lhs.val *  rhs.val; break;
                case Token_Quo   : res = lhs.val /  rhs.val; break;
                case Token_Or    : res = lhs.val |  rhs.val; break;
                case Token_And   :   res = lhs.val &  rhs.val; break;
                case Token_Shl   : res = lhs.val << rhs.val; break;
                case Token_Shr   : res = lhs.val >> rhs.val; break;
                case Token_Xor   : res = lhs.val ^  rhs.val; break;
                case Token_CmpEq : res = lhs.val == rhs.val; break;
                case Token_NotEq : res = lhs.val != rhs.val; break;
                case Token_Lt    : res = !num_greater_eq(lhs, rhs); break;
                case Token_Gt    : res =  num_greater_eq(lhs, rhs) && lhs.val != rhs.val; break;
                case Token_LtEq  : res = !num_greater_eq(lhs, rhs) || lhs.val == rhs.val; break;
                case Token_GtEq  : res =  num_greater_eq(lhs, rhs); break;
                case Token_CmpAnd: res = lhs.val && rhs.val; break;
                case Token_CmpOr : res = lhs.val || rhs.val; break;
                default: res = 0;
            }
            return constant_expr(pp, res, result_type, DEFAULT_FORMAT);
        }
        
        case ExprKind_Ternary: {
            Expr *cond = _pp_eval_expression(pp, expr->Ternary.cond);
            Expr *then = _pp_eval_expression(pp, expr->Ternary.then);
            Expr *els_ = _pp_eval_expression(pp, expr->Ternary.els_);
            
            u64 cond_val = cond->Constant.val;
            return cond_val ? then : els_;
        }
        
        default:
    	gb_printf_err("ERROR: Invalid Expression type in pp_eval_expression\n");
        return constant_expr(pp, 0, DEFAULT_TYPE, DEFAULT_FORMAT);
    }
}

u64 pp_eval_expression(Preprocessor *pp, Expr *expr)
{
    Expr *res = _pp_eval_expression(pp, expr);
    return res->Constant.val;
}

void free_expr(gbAllocator a, Expr *expr)
{
    
}