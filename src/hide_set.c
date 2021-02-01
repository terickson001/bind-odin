#include "hide_set.h"

Hide_Set *make_hide_set(String ident)
{
    Hide_Set *hs = gb_alloc_item(gb_heap_allocator(), Hide_Set);
    hs->ident = ident;
    return hs;
}

bool hide_set_contains(Hide_Set *hs, String ident)
{
    for (Hide_Set *curr = hs;
         curr != 0;
         curr = curr->next)
    {
        if (string_cmp(curr->ident, ident) == 0)
            return true;
    }
    
    return false;
}

Hide_Set *hide_set_union(Hide_Set *a, Hide_Set *b)
{
    Hide_Set ret;
    Hide_Set *dst = &ret;
    
    for (Hide_Set *src = a;
         src != 0;
         src = src->next, dst = dst->next)
    {
        dst->next = make_hide_set(src->ident);
    }
    
    for (Hide_Set *src = b;
         src != 0;
         src = src->next)
    {
        if (!hide_set_contains(a, src->ident))
        {
            dst->next = make_hide_set(src->ident);
            dst = dst->next;
        }
    }
    return ret.next;
}

Hide_Set *hide_set_intersection(Hide_Set *a, Hide_Set *b)
{
    Hide_Set ret;
    Hide_Set *dst = &ret;
    
    for (Hide_Set *curr = a;
         curr != 0;
         curr = curr->next)
    {
        if (hide_set_contains(b, curr->ident))
        {
            dst->next = make_hide_set(curr->ident);
            dst = dst->next;
        }
    }
    
    return ret.next;
}
