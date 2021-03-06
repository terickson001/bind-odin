#ifndef _BIND_CONFIG_H_
#define _BIND_CONFIG_H_

#include "gb/gb.h"
#include "hashmap.h"
#include "strings.h"
#include "types.h"

typedef struct PreprocessorConfig
{
     gbArray(String) include_dirs;

     // {String:String}
     map_t custom_symbols;

     String whitelist;

     // {String:[String]}
     map_t pre_includes;

     b32 shallow_include;
} PreprocessorConfig;

typedef enum BindOrdering
{
     Ordering_Sorted = 1,
     Ordering_Source,
} BindOrdering;

typedef struct BindConfig
{
     String package_name;
     gbArray(String) libraries;

     String type_prefix;
     String var_prefix;
     String proc_prefix;
     String const_prefix;

     Case type_case;
     Case var_case;
     Case proc_case;
     Case const_case;

     b32 use_cstring;
     b32 shallow_bind;
     String whitelist;

     BindOrdering ordering;
     gbArray(String) custom_ordering;

     // {String:String}
     map_t custom_types;
} BindConfig;

typedef struct WrapConfig
{
     String package_name;
     String bind_package_name;

     String type_prefix;
     String var_prefix;
     String proc_prefix;
     String const_prefix;

     Case type_case;
     Case var_case;
     Case proc_case;
     Case const_case;
} WrapConfig;

typedef struct Config
{
     gbArray(String) files;
     String out_file;

     String directory;
     String out_directory;

     PreprocessorConfig pp_conf;
     BindConfig bind_conf;

     b32 do_wrap;
     WrapConfig wrap_conf;
} Config;

Config *load_config(char *file, gbAllocator a);
void update_config(Config *conf, char *file, gbAllocator a);

void print_config(Config *conf);

#endif
