#include "wrap.h"

#include "string.h"
#include "util.h"
#include "print.h"

Wrapper make_wrapper(Resolver resolver, WrapConfig *conf)
{
     Wrapper w = {0};
     w.allocator = resolver.allocator;

     w.bind_rename_map = resolver.rename_map;
     w.rename_map = hashmap_new(w.allocator);
     init_rename_map(w.rename_map, w.allocator);
     w.conf = conf;
     w.package = resolver.package;

     return w;
}

b32 vars_contain_cstring(Node *vars)
{
     gbArray(Node *) list = vars->VarDeclList.list;
     for (int i = 0; i < gb_array_count(list); i++)
         {
         TypeInfo ti = get_type_info(list[i]->VarDecl.type);
         if (ti.stars == 0 && !ti.is_array)
             continue;

         String str = integer_type_str(list[i]->VarDecl.type);
         if (cstring_cmp(str, "u8") == 0)
             return true;
     }
     return false;
}

void wrap_record(Wrapper w, Node *node)
{
     String name = node->StructType.name->Ident.token.str;
     String renamed = rename_ident(name, RENAME_TYPE, true, w.rename_map, (BindConfig*)w.conf, w.allocator);
     String *bind_name;
     hashmap_get(w.bind_rename_map, name, (void **)&bind_name);

     if (!vars_contain_cstring(node->StructType.fields))
         gb_fprintf(w.out_file, "%.*s :: bind.%.*s;\n\n",
     LIT(renamed), LIT(*bind_name));
     else
         gb_fprintf(w.out_file, "%.*s :: bind.%.*s; // NOTE: contains cstring field\n\n",
     LIT(renamed), LIT(*bind_name));
}

void wrap_tpdef(Wrapper w, Node *tpdef)
{

}

void wrap_params(Wrapper w, Node *params)
{

}

void wrap_proc(Wrapper w, Node *node)
{
     String name = node->FunctionDecl.name->Ident.token.str;
     String renamed = rename_ident(name, RENAME_PROC, true, w.rename_map, (BindConfig*)w.conf, w.allocator);
     String *bind_name;
     hashmap_get(w.bind_rename_map, name, (void **)&bind_name);

     gb_fprintf(w.out_file, "%.*s :: proc(", LIT(renamed));
     if (node->FunctionDecl.type->FunctionType.params)
         wrap_params(w, node->FunctionDecl.type->FunctionType.params);
     gb_fprintf(w.out_file, ")");

     TypeInfo info = get_type_info(node->FunctionDecl.type->FunctionType.ret_type);
     b32 returns_void = info.stars == 0 && !info.is_array
         && info.base_type->kind == NodeKind_Ident && cstring_cmp(info.base_type->Ident.token.str, "void") == 0;
     if (!returns_void)
         {
         gb_fprintf(w.out_file, " -> ");
         print_type(*w.printer, node->FunctionDecl.type->FunctionType.ret_type, 0);
     }

     gb_fprintf(w.out_file, "\n{\n}\n");
}

void wrap_node(Wrapper w, Node *node)
{
     if (node->no_print)
         return;

     switch (node->kind)
         {
         case NodeKind_StructType:
         case NodeKind_UnionType:
         case NodeKind_EnumType:
         wrap_record(w, node);
         break;
         case NodeKind_Typedef:
         wrap_tpdef(w, node);
         break;
         case NodeKind_FunctionDecl:
         wrap_proc(w, node);
         break;
         default: break;
     }
}


void wrap_file(Wrapper w)
{
     gb_fprintf(w.out_file, "package %.*s\n\nimport bind \"%.*s\"\n\n", LIT(w.package.name), LIT(w.conf->bind_package_name));
     int record_count = gb_array_count(w.curr_file.records);
     int tpdef_count  = gb_array_count(w.curr_file.tpdefs);
     int ri = 0;
     int ti = 0;
     while (ri < record_count || ti < tpdef_count)
         {
         if (ti == tpdef_count || (ri < record_count && w.curr_file.records[ri]->index < w.curr_file.tpdefs[ti]->index))
             wrap_node(w, w.curr_file.records[ri++]);
         else if (ri == record_count || (ti < tpdef_count && w.curr_file.tpdefs[ti]->index < w.curr_file.records[ri]->index))
             wrap_node(w, w.curr_file.tpdefs[ti++]);
     }
}

void wrap_package(Wrapper w)
{
     for (int i = 0; i < gb_array_count(w.package.files); i++)
         {
         w.curr_file = w.package.files[i];
         w.out_file = gb_alloc_item(w.allocator, gbFile);

         create_path_to_file(w.curr_file.filename);
         gb_file_create(w.out_file, w.curr_file.filename);

         wrap_file(w);
     }
}
