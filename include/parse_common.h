#ifndef C_PREPROCESSOR_PARSE_COMMON_H
#define C_PREPROCESSOR_PARSE_COMMON_H

#include "tokenizer.h"

Token accept_token(Token_Run *run, TokenKind kind);
Token expect_token(Token_Run *run, TokenKind kind);
Token expect_tokens(Token_Run *run, isize n, ...);

#endif // C_PREPROCESSOR_PARSE_COMMON_H
