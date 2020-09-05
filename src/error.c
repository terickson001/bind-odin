#include "error.h"

#include "gb/gb.h"
#include "stdarg.h"
#include <signal.h>

void warning(Token tok, char const *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gb_printf_err("%.*s(%ld:%ld): \x1b[35mWARNING:\x1b[0m %s\n",
                  LIT(tok.loc.file), tok.loc.line, tok.loc.column,
                  gb_bprintf_va(fmt, va));
    if (tok.from_loc.file.start)
        gb_printf_err("=== From %.*s(%ld:%ld)\n",
                       LIT(tok.from_loc.file), tok.from_loc.line, tok.from_loc.column);
    va_end(va);
}

void error(Token tok, char const *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gb_printf_err("%.*s(%ld:%ld): \x1b[31mERROR:\x1b[0m %s\n",
                  LIT(tok.loc.file), tok.loc.line, tok.loc.column,
                  gb_bprintf_va(fmt, va));
    if (tok.from_loc.line)
        gb_printf_err("=== From %.*s(%ld:%ld)\n",
                       LIT(tok.from_loc.file), tok.from_loc.line, tok.from_loc.column);
    va_end(va);
    gb_exit(1); // Just exit for now, because we have no way for skipping past the error
}

void syntax_error(Token tok, char const *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gb_printf_err("%.*s(%ld:%ld): \x1b[31mSYNTAX ERROR:\x1b[0m %s\n",
                  LIT(tok.loc.file), tok.loc.line, tok.loc.column,
                  gb_bprintf_va(fmt, va));
    if (tok.from_loc.line)
        gb_printf_err("=== From %.*s(%ld:%ld)\n",
                       LIT(tok.from_loc.file), tok.from_loc.line, tok.from_loc.column);
    va_end(va);
    gb_exit(1); // Just exit for now, because we have no way for skipping past the error
}
