#include "resolve.h"
#include "ast.h"

Resolver make_resolver(Package p, BindConfig *conf)
{
   Resolver r = {0};

   r.package = p;

   r.allocator = gb_heap_allocator();

   r.conf = conf;

   r.rename_map           = hashmap_new(r.allocator);
   // r.opaque_types         = hashmap_new(r.allocator);
   r.forward_declarations = hashmap_new(r.allocator);
   r.duplicate_procs      = hashmap_new(r.allocator);
   r.duplicate_typedefs   = hashmap_new(r.allocator);

   gb_array_init(r.rename_queue, r.allocator);
   gb_array_init(r.needs_opaque_def, r.allocator);

   return r;
}

// Register all typedefs of non-pointer record types
void register_typedef_forward_declaration(Resolver *r, Node *tpdef)
{
    for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
    {
        Node *type = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;
        String name = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.name->Ident.token.str;

        // typedef struct Foo Foo;
        // typedef struct Foo Bar;
        if ((type->kind == NodeKind_StructType
            || type->kind == NodeKind_UnionType
            || type->kind == NodeKind_EnumType)
            && type->StructType.name
            && !type->StructType.fields)
        {
            String record_name = type->StructType.name->Ident.token.str;
            gbArray(Node *) forward_decs;
            if (hashmap_get(r->forward_declarations, record_name, (void **)&forward_decs) != MAP_OK)
                gb_array_init(forward_decs, r->allocator);
            else
                tpdef->Typedef.var_list->VarDeclList.list[i]->no_print = true;

            gb_array_append(forward_decs, tpdef->Typedef.var_list->VarDeclList.list[i]);
            hashmap_put(r->forward_declarations, record_name, forward_decs);
        }
        Node *recv_typedef;
        if (hashmap_get(r->duplicate_typedefs, name, (void **)&recv_typedef) != MAP_OK)
            hashmap_put(r->duplicate_typedefs, name, tpdef->Typedef.var_list->VarDeclList.list[i]);
        else
        {
            switch (recv_typedef->VarDecl.type->kind)
            {
                case NodeKind_StructType:
                case NodeKind_UnionType:
                case NodeKind_EnumType:
                    if (!recv_typedef->VarDecl.type->StructType.fields && tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type->StructType.fields)
                    {
                        recv_typedef->no_print = true;
                        hashmap_put(r->duplicate_typedefs, name, tpdef->Typedef.var_list->VarDeclList.list[i]);
                        break;
                    }
                default:
                    tpdef->Typedef.var_list->VarDeclList.list[i]->no_print = true;
                    break;
            }
        }
    }
}

void replace_base_type(Node **type, Node *new_base)
{
    for (;;)
    {
        switch ((*type)->kind)
        {
        case NodeKind_PointerType:
            type = &(*type)->PointerType.type;
            continue;
        case NodeKind_ArrayType:
            type = &(*type)->ArrayType.type;
            continue;
        case NodeKind_ConstType:
            type = &(*type)->ConstType.type;
            continue;
        case NodeKind_BitfieldType:
            type = &(*type)->BitfieldType.type;
            continue;
        default: break;
        }
        break;
    }
    *type = new_base;
}

void simplify_plural_record_typedef(Resolver *r, Node *tpdef)
{
    Node *base_type = 0;
    Node *replace_type = 0;
    for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
    {
        Node **type = &tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;
        // String name = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.name->Ident.token.str;
        if (((*type)->kind == NodeKind_StructType || (*type)->kind == NodeKind_UnionType || (*type)->kind == NodeKind_EnumType)
            && (*type)->StructType.fields)
        {
            base_type = (*type);
            replace_type =  tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.name;
        }
        else if (base_type && (*type) != base_type)
        {
            TypeInfo ti = get_type_info((*type));
            if (ti.base_type == base_type)
                replace_base_type(type, replace_type);
        }
    }
}

void register_possible_opaque_record(Resolver *r, Node *tpdef)
{
   b32 found_possible_opaque = false;
   b32 found_non_pointer = false;

   TypeInfo info = {0};
   for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
   {
       Node *type = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;

       info = get_type_info(type);

         // typedef struct Foo *pFoo;
       if ((info.base_type->kind == NodeKind_StructType
          || info.base_type->kind == NodeKind_UnionType
          || info.base_type->kind == NodeKind_EnumType)
           && info.base_type->StructType.name
           && !info.base_type->StructType.fields)
       {
             // Map struct name to the typedef that requries its definition
           if (info.stars > 0 || info.is_array)
               found_possible_opaque = true;
           else
               found_non_pointer = true;
       }
   }
   if (found_possible_opaque && !found_non_pointer)
       hashmap_put(r->opaque_types, info.base_type->StructType.name->Ident.token.str, tpdef);
}

void register_proc(Resolver *r, Node *proc)
{
    String name = proc->FunctionDecl.name->Ident.token.str;

    Node *recv_proc;
    if (hashmap_get(r->duplicate_procs, name, (void **)&recv_proc) != MAP_OK)
        hashmap_put(r->duplicate_procs, name, proc);
    else
        proc->no_print = true;
}

void register_forward_declaration(Resolver *r, Node *record)
{
    if (record->StructType.fields)
       return;

   String name = record->StructType.name->Ident.token.str;
   gbArray(Node *) forward_decs;
   if (hashmap_get(r->forward_declarations, name, (void **)&forward_decs) != MAP_OK)
       gb_array_init(forward_decs, r->allocator);
   else
       record->no_print = true;

   gb_array_append(forward_decs, record);
   hashmap_put(r->forward_declarations, name, forward_decs);
}


b32 register_bitfield(Resolver *r, Node *record)
{
    if (record->kind == NodeKind_EnumType || !record->StructType.fields)
        return false;

    gbArray(Node *) fields = record->StructType.fields->VarDeclList.list;
    b32 only_bitfield = true;
    for (int i = 0; i < gb_array_count(fields); i++)
    {
        if ((fields[i]->VarDecl.type->kind == NodeKind_StructType
            || fields[i]->VarDecl.type->kind == NodeKind_UnionType))
        {
            register_bitfield(r, fields[i]->VarDecl.type);
            only_bitfield = false;
        }
        else if (fields[i]->VarDecl.type->kind == NodeKind_BitfieldType)
        {
            record->StructType.has_bitfield = true;
        }
        else
        {
            only_bitfield = false;
        }
    }
    record->StructType.only_bitfield = only_bitfield;
    return false;
}

void remove_compound_constant(Resolver *r, Node *define)
{
   Node *expr = define->Define.value;
   if (expr->kind == NodeKind_CompoundLit)
   {
       define->no_print = true;
       gb_printf("\x1b[35mNOTE:\x1b[0m Could not bind define '%.*s', because compound constants are disallowed\n", LIT(define->Define.name));
   }
}

void register_typedef_bitfield(Resolver *r, Node *tpdef)
{
   for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
   {
       Node *type = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;
       String name = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.name->Ident.token.str;

       if ((type->kind == NodeKind_StructType
           || type->kind == NodeKind_UnionType)
           && register_bitfield(r, type))
       {
           break;
       }
   }
}

int hashmap_add_opaque(void *a, void *b)
{
//     gb_printf("ADDING OPAQUE\n");
   Node *node = (Node *)b;
   Resolver *r = (Resolver *)a;
   // node->is_opaque = true;
   gb_array_append(r->needs_opaque_def, node);
   return 0;
}

void resolve_package(Resolver *r)
{
     // Register necessary types
   for (int fi = 0; fi < gb_array_count(r->package.files); fi++)
   {
       Ast_File file = r->package.files[fi];
       for (int i = 0; i < gb_array_count(file.tpdefs); i++)
       {
           simplify_plural_record_typedef(r, file.tpdefs[i]);
           register_typedef_forward_declaration(r, file.tpdefs[i]);
           //register_typedef_record_type(r, file.tpdefs[i]);
           //register_possible_opaque_record(r, file.tpdefs[i]);
       }

       b32 redefined;
       for (int i = 0; i < gb_array_count(file.records); i++)
       {
           register_forward_declaration(r, file.records[i]);
       }

       for (int i = 0; i < gb_array_count(file.functions); i++)
       {
           register_proc(r, file.functions[i]);
       }
   }

     // Resolve redundancies
   for (int fi = 0; fi < gb_array_count(r->package.files); fi++)
   {
       Ast_File file = r->package.files[fi];

       for (int i = 0; i < gb_array_count(file.tpdefs); i++)
       {
           register_typedef_bitfield(r, file.tpdefs[i]);

           gbArray(Node *) defs = file.tpdefs[i]->Typedef.var_list->VarDeclList.list;
           for (int j = 0; j < gb_array_count(defs); j++)
           {
               if (defs[j]->no_print) continue;

               Node *type = defs[j]->VarDecl.type;
               if (type->kind == NodeKind_StructType
                   || type->kind == NodeKind_UnionType
                   || type->kind == NodeKind_EnumType)
               {
                   if (/*type->StructType.fields
                       && */type->StructType.name && hashmap_exists(r->opaque_types, type->StructType.name->Ident.token.str))
                   {
                       hashmap_remove(r->opaque_types, type->StructType.name->Ident.token.str);
                   }

                   gbArray(Node *) forward_decs;
                   if (type->StructType.name && type->StructType.fields
                       && hashmap_get(r->forward_declarations, type->StructType.name->Ident.token.str, (void **)&forward_decs) == MAP_OK)
                   {
                       for (int k = 0; k < gb_array_count(forward_decs); k++)
                       {
                           if (forward_decs[k]->no_print) break;
                           if (!(forward_decs[k]->kind == NodeKind_VarDecl && string_cmp(forward_decs[k]->VarDecl.name->Ident.token.str, defs[j]->VarDecl.name->Ident.token.str) != 0))
                           {
                               Node *record = 0;
                               switch (forward_decs[k]->kind)
                               {
                                   case NodeKind_VarDecl: record = forward_decs[k]->VarDecl.type; break;
                                   case NodeKind_StructType:
                                   case NodeKind_UnionType:
                                   case NodeKind_EnumType:
                                     record = forward_decs[k];
                                     break;
                               }

                               if (!hashmap_exists(r->rename_map, record->StructType.name->Ident.token.str))
                               {
                                   String *renamed = gb_alloc_item(r->allocator, String);
                                   *renamed = rename_ident(defs[j]->VarDecl.name->Ident.token.str, RENAME_TYPE, true, r->rename_map, r->conf, r->allocator);
                                   hashmap_put(r->rename_map, record->StructType.name->Ident.token.str, renamed);
                               }
                               forward_decs[k]->no_print = true;
                           }
                       }
                   }
                   if (type->StructType.name)
                   {
                       if (!hashmap_exists(r->rename_map, type->StructType.name->Ident.token.str))
                       {
                           if (!type->StructType.fields)
                           {
                               Pair p = {type->StructType.name, defs[j]->VarDecl.name};
                               gb_array_append(r->rename_queue, p);
                           }
                           else
                           {
                               String *renamed = gb_alloc_item(r->allocator, String);
                               *renamed = rename_ident(defs[j]->VarDecl.name->Ident.token.str, RENAME_TYPE, true, r->rename_map, r->conf, r->allocator);
                               hashmap_put(r->rename_map, type->StructType.name->Ident.token.str, renamed);
                           }
                       }
                   }
               }
           }
       }

       for (int i = 0; i < gb_array_count(file.records); i++)
       {
           Node_StructType record = file.records[i]->StructType;

           register_bitfield(r, file.records[i]);
             // If matches a registered forward declaration, and has fields, do not print forward declaration
           gbArray(Node *) forward_decs;
           if (record.fields
               && record.name
               && hashmap_get(r->forward_declarations, record.name->Ident.token.str, (void **)&forward_decs) == MAP_OK)
           {
               for (int j = 0; j < gb_array_count(forward_decs); j++)
               {
                   forward_decs[j]->no_print = true;
                   if (forward_decs[j]->kind != NodeKind_VarDecl) continue;
                   if (!hashmap_exists(r->rename_map, record.name->Ident.token.str))
                   {
                       String *renamed = gb_alloc_item(r->allocator, String);
                       *renamed = rename_ident(forward_decs[j]->VarDecl.name->Ident.token.str, RENAME_TYPE, true, r->rename_map, r->conf, r->allocator);
                       hashmap_put(r->rename_map, record.name->Ident.token.str, renamed);
                   }

               }
           }

             // If matches a possible opaque type, that type is not opaque. Remove it from the map
           if (record.name && hashmap_exists(r->opaque_types, record.name->Ident.token.str))
           {
               hashmap_remove(r->opaque_types, record.name->Ident.token.str);
           }
       }

       for (int i = 0; i < gb_array_count(file.defines); i++)
       {
           remove_compound_constant(r, file.defines[i]);
       }
   }

   for (int i = 0; i < gb_array_count(r->rename_queue); i++)
   {
       Pair p = r->rename_queue[i];
       if (!hashmap_exists(r->rename_map, p.a->Ident.token.str))
       {
           String *renamed = gb_alloc_item(r->allocator, String);
           *renamed = rename_ident(p.b->Ident.token.str, RENAME_TYPE, true, r->rename_map, r->conf, r->allocator);
           hashmap_put(r->rename_map, p.a->Ident.token.str, renamed);
       }
   }
   hashmap_iterate(r->opaque_types, hashmap_add_opaque, r);
}
