#ifndef _C_BIND_RESOLVE_H_
#define _C_BIND_RESOLVE_H_

#include "gb/gb.h"

#include "hashmap.h"
#include "types.h"
#include "util.h"
#include "config.h"

typedef struct Resolver
{
    Package package;
    map_t opaque_types;
    map_t forward_declarations;
    
    map_t rename_map;
    BindConfig *conf;
    gbAllocator allocator;
} Resolver;

Resolver make_resolver(Package p, BindConfig *conf);
void resolve_package(Resolver *r);

#endif
