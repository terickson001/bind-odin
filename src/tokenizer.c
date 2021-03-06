#include "tokenizer.h"

#include "error.h"

Tokenizer make_tokenizer(gbFileContents fc, String filename)
{
    Tokenizer t;
    t.start = (char *)fc.data;
    t.curr = t.start;
    t.end = t.start + fc.size;
    t.line_start = t.start;
    t.line = 1;
    t.file = filename;
    return t;
}

b32 try_increment_line(Tokenizer *t)
{
    if (t->curr[0] == '\n')
    {
        t->line++;
        t->line_start = ++t->curr;
        return true;
    }
    return false;
}

b32 skip_space(Tokenizer *t)
{
    for (;;)
    {
        if (gb_char_is_space(t->curr[0]) && t->curr[0] != '\n')
            t->curr++;
        else if (t->curr[0] == '\n')
            try_increment_line(t);
        else if (t->curr[0] == '\0' || t->curr >= t->end)
            return false;
        else
            return true;
    }
}

void trim_space_from_end_of_token(Token *token)
{
    char *end = token->str.start + (token->str.len-1);
    while (gb_char_is_space(*end))
        end--;
    token->str.len = (end+1) - token->str.start;
}

Token tokenize_number(Tokenizer *t)
{
    Token token = {0};
    token.str.start = t->curr;
    token.kind = Token_Integer;
    token.loc.line = t->line;
    token.loc.column = token.str.start - t->line_start;

    token.pp_loc = token.loc;

    int base = 10;
    if (t->curr[1] == 'x' || t->curr[1] == 'X')
    {
        t->curr+=2;
        base = 16;
    }
    else if (t->curr[1] == 'b')
    {
        t->curr+=2;
        base = 2;
    }
    else if (t->curr[0] == '0')
    {
        t->curr++;
        if (t->curr[0] != '.')
            base = 8;
        else
            base = 10;
    }

    while ((base == 10 && (gb_char_is_digit(t->curr[0]) || t->curr[0] == '.')) ||
           (base == 16 &&  gb_char_is_hex_digit(t->curr[0]))                   ||
           (base == 8  &&  t->curr[0] >= '0' && t->curr[0] <= '7')             ||
           (base == 2  &&  t->curr[0] >= '0' && t->curr[0] <= '1'))
    {
        if (t->curr[0] == '.')
            token.kind = Token_Float;
        t->curr++;
    }

    while (t->curr[0] == 'u' || t->curr[0] == 'U' ||
           t->curr[0] == 'l' || t->curr[0] == 'L' ||
           t->curr[0] == 'i')
    {
        if (t->curr[0] == 'i')
        {
            t->curr++;
            if      (gb_strncmp(t->curr, "8",   1) == 0)
                t->curr++;
            else if (gb_strncmp(t->curr, "16",  2) == 0 ||
                     gb_strncmp(t->curr, "32",  2) == 0 ||
                     gb_strncmp(t->curr, "64",  2) == 0)
                t->curr+=2;
            else if (gb_strncmp(t->curr, "128", 3) == 0)
                t->curr+=3;
            else
                gb_printf_err("%.*s(%ld:%ld): ERROR: illegal literal suffix\n", LIT(token.loc.file), token.loc.line, token.loc.column);
        }
        t->curr++;
    }

    token.str.len = t->curr - token.str.start;

    return token;
}

Token get_token(Tokenizer *t)
{
    Token token = {Token_Invalid};
    char c;

    if (!skip_space(t))
    {
        token.kind = Token_EOF;
        return token;
    }

    token.str.start = t->curr;
    token.str.len = 1;
    token.loc.line = t->line;
    token.loc.column = token.str.start - t->line_start;
    token.loc.file = t->file;
    c = *t->curr++;

    if (gb_char_is_alpha(c) || c == '_' || c == '$')
    {
        token.kind = Token_Ident;
        while (gb_char_is_alphanumeric(t->curr[0]) || t->curr[0] == '_' || t->curr[0] == '$')
            t->curr++;
        token.str.len = t->curr - token.str.start;

        for (int i = Token__KeywordBegin+1; i < Token__KeywordEnd; i++)
        {
            if (token.str.len == TokenKind_Strings[i].len && gb_strncmp(token.str.start, TokenKind_Strings[i].start, token.str.len) == 0)
            {
                token.kind = (TokenKind)i;
                break;
            }
        }

        // Not a Keyword
        if (token.kind == Token_Ident && cstring_cmp(token.str, "L") == 0)
        {
            if (t->curr[0] == '\'')
            {
                token.kind = Token_Wchar;
                t->curr++;
                token.str.start = t->curr;
                while (t->curr[0] != '\'')
                    t->curr += t->curr[0] == '\\' ? 2 : 1;

                token.str.len = t->curr - token.str.start;
            }
            else if (t->curr[0] == '"')
            {
                token.kind = Token_String;
                t->curr++;
                token.str.start = t->curr;

                while (t->curr[0] && t->curr[0] != '"')
                {
                    if (t->curr[0] == '\\' && t->curr[1])
                        t->curr++;
                    t->curr++;
                }

                token.str.len = t->curr - token.str.start;
                if (t->curr[0] == '"')
                    t->curr++;
            }
        }
    }
    else if (gb_char_is_digit(c))
    {
        t->curr--;
        token = tokenize_number(t);
    }
    else
    {
        switch (c)
        {
        case '\0':
            token.kind = Token_EOF;
            t->curr--;
            break;

        case '(':  token.kind = Token_OpenParen;    break;
        case ')':  token.kind = Token_CloseParen;   break;
        case '[':  token.kind = Token_OpenBracket;  break;
        case ']':  token.kind = Token_CloseBracket; break;
        case '{':  token.kind = Token_OpenBrace;    break;
        case '}':  token.kind = Token_CloseBrace;   break;
        case ',':  token.kind = Token_Comma;        break;
        case ';':  token.kind = Token_Semicolon;    break;
        case ':':  token.kind = Token_Colon;        break;
        case '\\': token.kind = Token_BackSlash;    break;
        case '~':  token.kind = Token_BitNot;       break;
        case '?':  token.kind = Token_Question;     break;
        case '@':  token.kind = Token_At;           break;

        case '#':  {
            if (t->curr[0] == '#')
            {
                token.kind = Token_Paste;
                t->curr++;
                token.str.len = 2;
            }
            else if (t->curr[0] == '@')
            {
                token.kind = Token_Charize;
                t->curr++;
                token.str.len = 2;
            }
            else
            {
                token.kind = Token_Hash;
            } break;
        }

        case '.':
            if (gb_char_is_digit(t->curr[0]))
            {
                token.kind = Token_Float;
                while (gb_char_is_digit(t->curr[0]))
                    t->curr++;
                token.str.len = t->curr - token.str.start;
            }
            else if (gb_strncmp(t->curr, "..", 2) == 0)
            {
                token.kind = Token_Ellipsis;
                t->curr += 2;
                token.str.len = 3;
            }
            else
            {
                token.kind = Token_Period;
            }
            break;

#define Triple_Token(c, single_token, double_token, equal_token)    \
            case c: {                                               \
                if (t->curr[0] == c)                                \
                {                                                   \
                    token.kind = double_token;                      \
                    token.str.len = 2;                              \
                    t->curr++;                                      \
                }                                                   \
                else if (t->curr[0] == '=')                         \
                {                                                   \
                    token.kind = equal_token;                       \
                    token.str.len = 2;                              \
                    t->curr++;                                      \
                }                                                   \
                else                                                \
                {                                                   \
                    token.kind = single_token;                      \
                }                                                   \
            } break

            Triple_Token('+',  Token_Add, Token_Inc,    Token_AddEq);
            Triple_Token('-',  Token_Sub, Token_Dec,    Token_SubEq);
            Triple_Token('|',  Token_Or,  Token_CmpOr,  Token_OrEq);
            Triple_Token('&',  Token_And, Token_CmpAnd, Token_AndEq);
            Triple_Token('<',  Token_Lt,  Token_Shl,   Token_LtEq);
            Triple_Token('>',  Token_Gt,  Token_Shr,   Token_GtEq);

#undef Triple_Token
#define Token_TokEq(c, single_token, equal_token)   \
            case c: {                               \
                if (t->curr[0] == '=')              \
                {                                   \
                    token.kind = equal_token;       \
                    token.str.len = 2;              \
                    t->curr++;                      \
                }                                   \
                else                                \
                {                                   \
                    token.kind = single_token;      \
                }                                   \
            } break;

            Token_TokEq('!', Token_Not, Token_NotEq);
            Token_TokEq('*', Token_Mul, Token_MulEq);
            Token_TokEq('=', Token_Eq,  Token_CmpEq);
            Token_TokEq('^', Token_Xor, Token_XorEq);
            Token_TokEq('%', Token_Mod, Token_ModEq);

#undef Token_TokEq

        case '\'': {
            token.kind = Token_Char;
            token.str.start = t->curr;

            if (t->curr[0] && t->curr[0] == '\\')
                token.str.len = 2;

            t->curr += token.str.len+1;

        } break;

        case '"': {
            token.kind = Token_String;
            token.str.start = t->curr;

            while (t->curr[0] && t->curr[0] != '"')
            {
                if (t->curr[0] == '\\' && t->curr[1])
                    t->curr++;
                t->curr++;
            }

            token.str.len = t->curr - token.str.start;
            if (t->curr[0] == '"')
                t->curr++;

        } break;

        case '/': {
            if (t->curr[0] == '/')
            {
                t->curr++;
                while (t->curr[0] == '\t' || t->curr[0] == ' ')
                    t->curr++;

                token.str.start = t->curr;
                token.kind = Token_Comment;
                while (t->curr[0] && t->curr[0] != '\n' && t->curr <= t->end)
                    t->curr++;

                token.str.len = t->curr - token.str.start;
                trim_space_from_end_of_token(&token);
            }
            else if (t->curr[0] == '*')
            {
                isize comment_scope = 1;
                t->curr++;
                skip_space(t);

                token.str.start = t->curr;
                token.kind = Token_Comment;
                while (t->curr[0] && comment_scope > 0 && t->curr < t->end)
                {
                    if (t->curr[0] == '/' && t->curr[1] == '*')
                        comment_scope++;
                    else if (t->curr[0] == '*' && t->curr[1] == '/')
                        comment_scope--;

                    if (!try_increment_line(t))
                        t->curr++;
                }

                token.str.len = t->curr-1 - token.str.start;
                trim_space_from_end_of_token(&token);
                t->curr++;
            }
            else if (t->curr[0] == '=')
            {
                token.kind = Token_QuoEq;
                token.str.len = 2;
                t->curr++;
            }
            else
            {
                token.kind = Token_Quo;
                break;
            }

            if (t->curr[-1] == '*' && t->curr[0] == '/')
                t->curr += 2;
        } break;

        default: break;
        }
    }

    if (token.kind == Token_Invalid)
    {
        error(token, "Got invalid token");
        gb_exit(1);
    }
    return token;
}

void print_token_run(Token_Run run)
{
    for (Token *t = run.start; t <= run.end; t++)
        gb_printf("%.*s ", LIT(t->str));
    gb_printf("\n");
}

String token_run_string(Token_Run run)
{
    if (!run.start || run.start > run.end) return (String){0};

	String start = run.start->str;
	String end = run.end->str;
	return (String){start.start, (end.start+end.len)-start.start};
}

String token_run_string_alloc(Token_Run run)
{
    if (!run.start) return (String){0};
    int len = 0;
    for (Token *t = run.start; t <= run.end; t++)
        len += t->str.len + 1;
    String ret = {0};
    ret.start = gb_alloc(gb_heap_allocator(), len);
    ret.len = len-1;

    char *curr = ret.start;
    for (Token *t = run.start; t <= run.end; t++)
    {
        gb_strncpy(curr, t->str.start, t->str.len);
        curr[t->str.len] = ' ';
        curr += t->str.len+1;
    }
    return ret;
}


Token *make_token(char *str, TokenKind kind)
{
    Token *token = gb_alloc_item(gb_heap_allocator(), Token);
    token->kind = kind;
    token->str.start = gb_alloc_str(gb_heap_allocator(), str);
    token->str.len = gb_strlen(str);

    return token;
}

Token_Run make_token_run(char *str, TokenKind kind)
{
    Token *token = make_token(str, kind);
    return (Token_Run){token, token, token};
}

Token_Run str_make_token_run(String str, TokenKind kind)
{
    Token *token = gb_alloc_item(gb_heap_allocator(), Token);
    token->kind = kind;
    token->str = str;
    return (Token_Run){token, token, token};
}
