#ifndef _C_BIND_TYPES_H
#define _C_BIND_TYPES_H

#include "gb/gb.h"
#include "strings.h"
#include "ast.h"
#include "symbol.h"

typedef struct Package
{
    String name;
    gbArray(Lib) libs;

    gbArray(Ast_File) files;
} Package;

typedef enum Case {
    Case_NONE,
    Case_SNAKE,
    Case_SCREAMING_SNAKE,
    Case_ADA,
    Case_CAMEL,
} Case;

#endif /* ifndef _C_BIND_TYPES_H */
