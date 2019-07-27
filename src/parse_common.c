#include "parse_common.h"

#include "error.h"

#include <stdarg.h>

Token accept_token(Token_Run *run, TokenKind kind)
{
    if (run->curr[0].kind == kind)
        return (run->curr++)[0];
    return (Token){0};
}

Token expect_token(Token_Run *run, TokenKind kind)
{
    Token tok = run->curr[0];
	if (tok.kind != kind)
    {
        syntax_error(tok, "Expected '%.*s', got '%.*s'", LIT(TokenKind_Strings[kind]), LIT(TokenKind_Strings[tok.kind]));
        return (Token){0};
    }
    run->curr++;
    return tok;    
}

Token expect_tokens(Token_Run *run, isize n, ...)
{
    va_list va;
    Token ret;
    b32 found = false;
    
    va_start(va, n);
    for (int i = 0; i < n; i++)
    {
        TokenKind test = va_arg(va, TokenKind);
        if (run->curr[0].kind == test)
        {
            ret = (run->curr++)[0];
            found = true;
            break;
        }
    }
    va_end(va);

    if (!found)
    {
        syntax_error(run->curr[0], "Unexpected token '%s'(%d)", TokenKind_Strings[run->curr->kind], run->curr->kind);
        return (Token){0};
    }
    
    return ret;
}
