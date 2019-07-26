#ifndef _C_BIND_TYPES_H
#define _C_BIND_TYPES_H

#include "gb/gb.h"
#include "string.h"
#include "ast.h"

typedef struct Package
{
    String name;
    String lib_name;

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