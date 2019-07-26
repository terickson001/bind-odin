#ifndef _C_BIND_ERROR_H_
#define _C_BIND_ERROR_H_

#include "tokenizer.h"

void warning(Token tok, char const *fmt, ...);
void error(Token tok, char const *fmt, ...);
void syntax_error(Token tok, char const*fmt, ...);

#endif
