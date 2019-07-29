#include "resolve.h"
#include "ast.h"

Resolver make_resolver(Package p, BindConfig *conf)
{
    Resolver r = {0};

    r.package = p;

    r.allocator = gb_heap_allocator();

    r.conf = conf;
    
    r.rename_map           = hashmap_new(r.allocator);
    r.opaque_types         = hashmap_new(r.allocator);
    r.forward_declarations = hashmap_new(r.allocator);

    return r;
}

// Register all typedefs of non-pointer records types
void register_typedef_record_type(Resolver *r, Node *tpdef)
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
         && type->StructType.name)
        {
            String *renamed = gb_alloc_item(r->allocator, String);
            *renamed = rename_ident(name, RENAME_TYPE, true, r->rename_map, r->conf, r->allocator);
            hashmap_put(r->rename_map, type->StructType.name->Ident.token.str, renamed);
            break;
        }
    }
}

void register_possible_opaque_record(Resolver *r, Node *tpdef)
{
    b32 found_possible_opaque = false;
    b32 found_non_pointer = false;
    
    TypeInfo info;
    for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
    {
        Node *type = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;

        info = get_type_info(type, r->allocator);
        
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

b32 _record_has_bitfield(Resolver *r, Node *record)
{
    if (record->kind == NodeKind_EnumType || !record->StructType.fields)
        return false;

    gbArray(Node *) fields = record->StructType.fields->VarDeclList.list;
    for (int i = 0; i < gb_array_count(fields); i++)
    {
        if ((fields[i]->VarDecl.type->kind == NodeKind_StructType
          || fields[i]->VarDecl.type->kind == NodeKind_UnionType)
         && _record_has_bitfield(r, fields[i]->VarDecl.type))
            return true;
        else if (fields[i]->VarDecl.type->kind == NodeKind_BitfieldType)
            return true;
    }
    return false;
}

void remove_bit_field_type(Resolver *r, Node *record)
{
    if (_record_has_bitfield(r, record))
    {
        record->no_print = true;
        gb_printf("NOTE: Could not bind record '%.*s', because it contains a bitfield\n", LIT(record->StructType.name->Ident.token.str));
    }
}

void remove_typedef_bit_field_type(Resolver *r, Node *tpdef)
{
    for (int i = 0; i < gb_array_count(tpdef->Typedef.var_list->VarDeclList.list); i++)
    {
        Node *type = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.type;
        String name = tpdef->Typedef.var_list->VarDeclList.list[i]->VarDecl.name->Ident.token.str;

        if ((type->kind == NodeKind_StructType
          || type->kind == NodeKind_UnionType)
         && _record_has_bitfield(r, type))
        {
            tpdef->no_print = true;
            gb_printf("NOTE: Could not bind record '%.*s', because it contains a bitfield\n", LIT(name));
            break;
        }
    }
}

void register_forward_declaration(Resolver *r, Node *record)
{
    if (record->StructType.fields)
        return;

    String name = record->StructType.name->Ident.token.str;
    hashmap_put(r->forward_declarations, name, record);
}

int hashmap_add_opaque(void *a, void *b)
{
    Node *node = (Node *)b;
    node->is_opaque = true;
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
            register_typedef_record_type(r, file.tpdefs[i]);
            register_possible_opaque_record(r, file.tpdefs[i]);
            remove_typedef_bit_field_type(r, file.tpdefs[i]);
        }

        b32 redefined;
        for (int i = 0; i < gb_array_count(file.records); i++)
        {
            register_forward_declaration(r, file.records[i]);
            remove_bit_field_type(r, file.records[i]);
        }
    }

    // Resolve redundancies
    for (int fi = 0; fi < gb_array_count(r->package.files); fi++)
    {
        Ast_File file = r->package.files[fi];

        for (int i = 0; i < gb_array_count(file.tpdefs); i++)
        {
            gbArray(Node *) defs = file.tpdefs[i]->Typedef.var_list->VarDeclList.list;
            for (int j = 0; j < gb_array_count(defs); j++)
            {
                Node *type = defs[j]->VarDecl.type;
                if (type->kind == NodeKind_StructType
                 || type->kind == NodeKind_UnionType
                 || type->kind == NodeKind_EnumType)
                {
                    if (type->StructType.fields
                     && type->StructType.name && hashmap_exists(r->opaque_types, type->StructType.name->Ident.token.str))
                    {
                        hashmap_remove(r->opaque_types, type->StructType.name->Ident.token.str);
                    }

                    Node *forward_dec;
                    if (type->StructType.name
                     && hashmap_get(r->forward_declarations, type->StructType.name->Ident.token.str, (void **)&forward_dec) == MAP_OK)
                    {
                        forward_dec->no_print = true;
                    }
                }
            }
        }
        
        for (int i = 0; i < gb_array_count(file.records); i++)
        {
            Node_StructType record = file.records[i]->StructType;
            
            // If matches a registered forward declaration, and has fields, do not print forward declaration
            Node *forward_dec;
            if (record.fields
             && record.name
             && hashmap_get(r->forward_declarations, record.name->Ident.token.str, (void **)&forward_dec) == MAP_OK)
            {
                forward_dec->no_print = true;
            }

            // If matches a possible opaque type, that type is not opaque. Remove it from the map
            if (record.name && hashmap_exists(r->opaque_types, record.name->Ident.token.str))
            {
                hashmap_remove(r->opaque_types, record.name->Ident.token.str);
            }
        }
    }
    hashmap_iterate(r->opaque_types, hashmap_add_opaque, 0);
}

