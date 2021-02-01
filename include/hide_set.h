/* date = January 31st 2021 4:16 pm */

#ifndef HIDE_SET_H
#define HIDE_SET_H

#include "gb/gb.h"
#include "strings.h"

typedef struct Hide_Set
{
    struct Hide_Set *next;
    String ident;
} Hide_Set;

Hide_Set *make_hide_set(String ident);
bool hide_set_contains(Hide_Set *hs, String ident);
Hide_Set *hide_set_union(Hide_Set *a, Hide_Set *b);
Hide_Set *hide_set_intersection(Hide_Set *a, Hide_Set *b);

#endif //HIDE_SET_H
