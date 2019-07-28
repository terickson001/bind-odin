#ifndef C_PARSER_TOKENIZER_H_
#define C_PARSER_TOKENIZER_H_

#include "gb/gb.h"
#include "strings.h"

#define TOKEN_KINDS                             \
    TOKEN_KIND(Token_Invalid, "Invalid"),       \
    TOKEN_KIND(Token_EOF,     "EOF"),           \
    TOKEN_KIND(Token_Comment, "Comment"),       \
    TOKEN_KIND(Token_Paste,   "##"),            \
    TOKEN_KIND(Token_Charize, "#@"),            \
    TOKEN_KIND(Token_At,      "@"),             \
\
TOKEN_KIND(Token__LiteralBegin, "_LiteralBegin"), \
    TOKEN_KIND(Token_Ident,     "identifier"),    \
    TOKEN_KIND(Token_Integer,   "integer"),       \
    TOKEN_KIND(Token_Float,     "float"),         \
    TOKEN_KIND(Token_Char,      "char"),          \
    TOKEN_KIND(Token_Wchar,     "wchar"),         \
    TOKEN_KIND(Token_String,    "string"),        \
TOKEN_KIND(Token__LiteralEnd,   "_LiteralEnd"),   \
\
TOKEN_KIND(Token__OperatorBegin, "_OperatorBegin"), \
    TOKEN_KIND(Token_Not,      "!"),                \
    TOKEN_KIND(Token_Question, "?"),                \
    TOKEN_KIND(Token_Xor,      "^"),                \
    TOKEN_KIND(Token_Add,      "+"),                \
    TOKEN_KIND(Token_Inc,      "++"),               \
    TOKEN_KIND(Token_Sub,      "-"),                \
    TOKEN_KIND(Token_Dec,      "--"),               \
    TOKEN_KIND(Token_Mul,      "*"),                \
    TOKEN_KIND(Token_Quo,      "/"),                \
    TOKEN_KIND(Token_Mod,      "%"),                \
    TOKEN_KIND(Token_And,      "&"),                \
    TOKEN_KIND(Token_Or,       "|"),                \
    TOKEN_KIND(Token_BitNot,   "~"),                \
    TOKEN_KIND(Token_Shl,      "<<"),               \
    TOKEN_KIND(Token_Shr,      ">>"),               \
    TOKEN_KIND(Token_CmpAnd,   "&&"),               \
    TOKEN_KIND(Token_CmpOr,    "||"),               \
\
TOKEN_KIND(Token__ComparisonBegin, "_ComparisonBegin"), \
    TOKEN_KIND(Token_CmpEq, "=="),                      \
    TOKEN_KIND(Token_NotEq, "!="),                      \
    TOKEN_KIND(Token_Lt,    "<"),                       \
    TOKEN_KIND(Token_Gt,    ">"),                       \
    TOKEN_KIND(Token_LtEq,  "<="),                      \
    TOKEN_KIND(Token_GtEq,  ">="),                      \
TOKEN_KIND(Token__ComparisonEnd, "_ComparisonEnd"),     \
\
TOKEN_KIND(Token__AssignOpBegin, "_AssignOpBegin"), \
    TOKEN_KIND(Token_Eq,       "="),                \
    TOKEN_KIND(Token_AddEq,    "+="),               \
    TOKEN_KIND(Token_SubEq,    "-="),               \
    TOKEN_KIND(Token_MulEq,    "*="),               \
    TOKEN_KIND(Token_QuoEq,    "/="),               \
    TOKEN_KIND(Token_ModEq,    "%="),               \
    TOKEN_KIND(Token_AndEq,    "&="),               \
    TOKEN_KIND(Token_XorEq,    "^="),               \
    TOKEN_KIND(Token_OrEq,     "|="),               \
    TOKEN_KIND(Token_ShlEq,    "<<="),              \
    TOKEN_KIND(Token_ShrEq,    ">>="),              \
TOKEN_KIND(Token__AssignOpEnd, "_AssignOpEnd"),     \
\
    TOKEN_KIND(Token_OpenParen,     "("),       \
    TOKEN_KIND(Token_CloseParen,    ")"),       \
    TOKEN_KIND(Token_OpenBracket,   "["),       \
    TOKEN_KIND(Token_CloseBracket,  "]"),       \
    TOKEN_KIND(Token_OpenBrace,     "{"),       \
    TOKEN_KIND(Token_CloseBrace,    "}"),       \
    TOKEN_KIND(Token_Colon,         ":"),       \
    TOKEN_KIND(Token_Semicolon,     ";"),       \
    TOKEN_KIND(Token_Period,        "."),       \
    TOKEN_KIND(Token_ArrowRight,    "->"),      \
    TOKEN_KIND(Token_Comma,         ","),       \
    TOKEN_KIND(Token_Ellipsis,      "..."),     \
    TOKEN_KIND(Token_BackSlash,     "\\"),      \
    TOKEN_KIND(Token_Hash,          "#"),       \
TOKEN_KIND(Token__OperatorEnd, "_OperatorEnd"), \
\
TOKEN_KIND(Token__KeywordBegin, "_KeywordBegin"),        \
        TOKEN_KIND(Token_signed,       "signed"),        \
        TOKEN_KIND(Token_unsigned,     "unsigned"),      \
        TOKEN_KIND(Token_short,        "short"),         \
        TOKEN_KIND(Token_long,         "long"),          \
        TOKEN_KIND(Token_int,          "int"),           \
        TOKEN_KIND(Token_char,         "char"),          \
        TOKEN_KIND(Token__int8,        "__int8"),        \
        TOKEN_KIND(Token__int16,       "__int16"),       \
        TOKEN_KIND(Token__int32,       "__int32"),       \
        TOKEN_KIND(Token__int64,       "__int64"),       \
        TOKEN_KIND(Token_float,        "float"),         \
        TOKEN_KIND(Token_double,       "double"),        \
        TOKEN_KIND(Token_typedef,      "typedef"),       \
        TOKEN_KIND(Token_if,           "if"),            \
        TOKEN_KIND(Token_else,         "else"),          \
        TOKEN_KIND(Token_for,          "for"),           \
        TOKEN_KIND(Token_switch,       "switch"),        \
        TOKEN_KIND(Token_do,           "do"),            \
        TOKEN_KIND(Token_while,        "while"),         \
        TOKEN_KIND(Token_case,         "case"),          \
        TOKEN_KIND(Token_break,        "break"),         \
        TOKEN_KIND(Token_continue,     "continue"),      \
        TOKEN_KIND(Token_return,       "return"),        \
        TOKEN_KIND(Token_struct,       "struct"),        \
        TOKEN_KIND(Token_union,        "union"),         \
        TOKEN_KIND(Token_enum,         "enum"),          \
        TOKEN_KIND(Token_static,       "static"),        \
        TOKEN_KIND(Token_extern,       "extern"),        \
        TOKEN_KIND(Token_goto,         "goto"),          \
        TOKEN_KIND(Token_const,        "const"),         \
        TOKEN_KIND(Token_sizeof,       "sizeof"),        \
        TOKEN_KIND(Token_Alignof,      "_Alignof"),      \
        TOKEN_KIND(Token_volatile,     "volatile"),      \
        TOKEN_KIND(Token_register,     "register"),      \
        TOKEN_KIND(Token_inline,       "inline"),        \
        TOKEN_KIND(Token__inline_,     "__inline__"),    \
        TOKEN_KIND(Token__inline,      "__inline"),      \
        TOKEN_KIND(Token__forceinline, "__forceinline"), \
        TOKEN_KIND(Token_attribute,    "__attribute__"), \
        TOKEN_KIND(Token_restrict,     "__restrict"),    \
        TOKEN_KIND(Token_extension,    "__extension__"), \
        TOKEN_KIND(Token_asm,          "__asm__"),       \
        TOKEN_KIND(Token_pragma,       "__pragma"),      \
        TOKEN_KIND(Token_cdecl,        "__cdecl"),       \
        TOKEN_KIND(Token_clrcall,      "__clrcall"),     \
        TOKEN_KIND(Token_stdcall,      "__stdcall"),     \
        TOKEN_KIND(Token_fastcall,     "__fastcall"),    \
        TOKEN_KIND(Token_thiscall,     "__thiscall"),    \
        TOKEN_KIND(Token_vectorcall,   "__vectorcall"),  \
        TOKEN_KIND(Token_declspec,     "__declspec"),    \
        TOKEN_KIND(Token_unaligned,    "__unaligned"),   \
TOKEN_KIND(Token__KeywordEnd, "_KeywordEnd"),

typedef enum TokenKind
{
#define TOKEN_KIND(e, s) e
    TOKEN_KINDS
#undef TOKEN_KIND
    Token_Count
} TokenKind;

gb_global String const TokenKind_Strings[Token_Count] = {
#define TOKEN_KIND(e, s) {s, gb_size_of(s)-1}
    TOKEN_KINDS
#undef TOKEN_KIND
};

typedef struct Tokenizer
{
    char *start, *curr, *end;
    char *line_start;
    isize line;
    String file;
} Tokenizer;

typedef struct File_Location
{
    isize line, column;
    String file;
} File_Location;

typedef struct Token
{
    TokenKind kind;
    String str;
    File_Location loc;
    File_Location pp_loc;
    File_Location from_loc;
} Token;

typedef struct Token_Run
{
    Token *start, *curr, *end;
} Token_Run;

/* typedef struct TokenString */
/* { */
/*     Token *start; */
/*     isize len; */
/* } TokenString; */


Tokenizer make_tokenizer(gbFileContents fc, String filename);
b32 try_increment_line(Tokenizer *t);
b32 skip_space(Tokenizer *t);
void trim_space_from_end_of_token(Token *token);
Token get_token(Tokenizer *t);

// String get_token_string(Token token);
String token_run_string(Token_Run run);
Token_Run make_token_run(char *str, TokenKind kind);
Token_Run str_make_token_run(String str, TokenKind kind);

#endif /* ifndef C_PARSER_TOKENIZER_H_ */
