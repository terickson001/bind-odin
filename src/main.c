#define GB_IMPLEMENTATION
#include "gb/gb.h"

#include "types.h"
#include "preprocess.h"
#include "define.h"

#include "tokenizer.h"
#include "print.h"
#include "parse.h"
#include "bind.h"
#include "config.h"

#include "util.h"
#include "strings.h"

const char *HELP_TEXT =
"Usage: bind-odin [options] file...\n"
"Options:\n"
"  -c, --config <file>               Load configuration from <file>\n"
"  -i, --input <path>                Input files at <path>\n"
"  -o, --output <path>               Output bindings to <path>\n"
"  -p, --prefix <prefix>             Remove <prefix> from the beginning of type and variable names\n"
"  -v, --variable-prefix <prefix>    Remove <prefix> from the beginning of variable names\n"
"  -t, --type-prefix <prefix>        Remove <prefix> from the beginning of type names\n"
"  -s, --source-order                If specified, output the bindings in the same order as the nodes appear in the source file\n"
"  -I, --include <dir>               Add <dir> to the include path\n"
"  -w, --whitelist <substring>       Create bindings for all included files whose paths contain <substring>\n"
"  -l, --link <lib>                  Link bindings to <lib>\n"
"  -P, --package <package>           Use <package> as the package name for the bindings\n";

Config *init_options(int argc, char **argv, gbArray(Bind_Task) *out_tasks);
void enable_console_colors();

int main(int argc, char **argv)
{
    enable_console_colors();
    gbAllocator a = gb_heap_allocator();
    
    gbArray(Bind_Task) tasks;
    Config *conf = init_options(argc, argv, &tasks);
    print_config(conf);
    
    bind_generate(conf, tasks);
    gb_printf("DONE!\n");
}

void enable_console_colors()
{
#ifdef GB_SYSTEM_WINDOWS
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    u32 mode;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    // WOW! It just works!!
    // Crazy
#endif
}

Config *init_options(int argc, char **argv, gbArray(Bind_Task) *out_tasks)
{
    gbAllocator a = gb_heap_allocator();
    
    Config *conf = gb_alloc_item(a, Config);
    
    char *output = 0;
    gbArray(char *) inputs;
    gb_array_init(inputs, a);
    for (int i = 1; i < argc; i++)
    {
        if ((gb_strcmp(argv[i], "-c") == 0 || gb_strcmp(argv[i], "--config") == 0) && i+1 < argc)
        {
            update_config(conf, argv[i+1], a);
            i++;
        }
        else if ((gb_strcmp(argv[i], "-o") == 0 || gb_strcmp(argv[i], "--output") == 0) && i+1 < argc)
        {
            if (conf->out_directory.start || conf->out_file.start)
            {
                String out = conf->out_directory.start ? conf->out_directory : conf->out_file;
                gb_printf_err(
                              "\x1b[31mERROR:\x1b[0m %s %s: Output has already been specified (%.*s)\n",
                              argv[i], argv[i+1], LIT(out));
                gb_exit(1);
            }
            
            if (gb_file_is_dir(argv[i+1]))
            {
                conf->out_directory = make_string(argv[i+1]);
            }
            else
                conf->out_file = make_string(argv[i+1]);
            i++;
        }
        else if ((gb_strcmp(argv[i], "-i") == 0 || gb_strcmp(argv[i], "--input") == 0) && i+1 < argc)
        {
            if (gb_file_is_dir(argv[i+1]))
            {
                if (conf->files)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s %s: Inputs must be either files or a directory\n",
                                  argv[i], argv[i+1]);
                    gb_exit(1);
                }
                if (conf->directory.start)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s %s: Input directory has already been specified (%.*s)\n",
                                  argv[i], argv[i+1], LIT(conf->directory));
                    gb_exit(1);
                }
                conf->directory = make_string(argv[i+1]);
            }
            else
            {
                if (conf->directory.start)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s %s: Inputs must be either files or a directory\n",
                                  argv[i], argv[i+1]);
                    gb_exit(1);
                }
                if (!conf->files) gb_array_init(conf->files, a);
                gb_array_append(conf->files, make_string(argv[i+1]));
            }
            i++;
        }
        else if ((gb_strcmp(argv[i], "-p") == 0 || gb_strcmp(argv[i], "--prefix") == 0) && i+1 < argc)
        {
            String prefix = make_string(argv[i+1]);
            conf->bind_conf.type_prefix  = prefix;
            conf->bind_conf.var_prefix   = prefix;
            conf->bind_conf.proc_prefix  = prefix;
            conf->bind_conf.const_prefix = prefix;
            i++;
        }
        else if ((gb_strcmp(argv[i], "-P") == 0 || gb_strcmp(argv[i], "--package") == 0) && i+1 < argc)
        {
            conf->bind_conf.package_name = make_string(argv[i+1]);
            i++;
        }
        else if ((gb_strcmp(argv[i], "-l") == 0 || gb_strcmp(argv[i], "--link") == 0) && i+1 < argc)
        {
            conf->bind_conf.lib_name = make_string(argv[i+1]);
            i++;
        }
        else if ((gb_strcmp(argv[i], "-w") == 0 || gb_strcmp(argv[i], "--whitelist") == 0) && i+1 < argc)
        {
            conf->pp_conf.whitelist = make_string(argv[i+1]);
            i++;
        }
        else if ((gb_strcmp(argv[i], "-s") == 0 || gb_strcmp(argv[i], "--source-order") == 0))
        {
            conf->bind_conf.ordering = Ordering_Source;
        }
        else if ((gb_strncmp(argv[i], "-I", 2) == 0 || gb_strcmp(argv[i], "--include") == 0))
        {
            if (!conf->pp_conf.include_dirs) gb_array_init(conf->pp_conf.include_dirs, a);
            char *curr;
            if (gb_strlen(argv[i]) > 2)
            {
                curr = argv[i]+2;
            }
            else
            {
                i++;
                if (i >= argc)
                    break;
                curr = argv[i];
            }
            
            char *start = curr;
            while (*curr)
            {
                if (*curr == ',')
                {
                    *curr = 0;
                    start = curr+1;
                }
                curr++;
            }
            gb_array_append(conf->pp_conf.include_dirs, make_string(start));
        }
        else if (gb_strcmp(argv[i], "-h") == 0 || gb_strcmp(argv[i], "--help") == 0)
        {
            gb_printf("%s", HELP_TEXT);
            gb_exit(0);
        }
        else if (argv[i][0] != '-')
        {
            if (gb_file_is_dir(argv[i]))
            {
                if (conf->files)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s: Inputs must be either files or a directory\n",
                                  argv[i]);
                    gb_exit(1);
                }
                if (conf->directory.start)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s: Input directory has already been specified (%.*s)\n",
                                  argv[i], LIT(conf->directory));
                    gb_exit(1);
                }
                conf->directory = make_string(argv[i]);
            }
            else
            {
                if (conf->directory.start)
                {
                    gb_printf_err(
                                  "\x1b[31mERROR:\x1b[0m %s: Inputs must be either files or a directory\n",
                                  argv[i]);
                    gb_exit(1);
                }
                if (!conf->files) gb_array_init(conf->files, a);
                gb_array_append(conf->files, make_string(argv[i]));
            }
        }
        else if (gb_strcmp(argv[i], "--") == 0)
        {
            i++;
            for (; i < argc; i++)
            {
                if (gb_file_is_dir(argv[i]))
                {
                    if (conf->files)
                    {
                        gb_printf_err(
                                      "\x1b[31mERROR:\x1b[0m %s: Inputs must be either files or a directory\n",
                                      argv[i]);
                        gb_exit(1);
                    }
                    if (conf->directory.start)
                    {
                        gb_printf_err(
                                      "\x1b[31mERROR:\x1b[0m %s: Input directory has already been specified (%.*s)\n",
                                      argv[i], LIT(conf->directory));
                        gb_exit(1);
                    }
                    conf->directory = make_string(argv[i]);
                }
                else
                {
                    if (conf->directory.start)
                    {
                        gb_printf_err(
                                      "\x1b[31mERROR:\x1b[0m %s: Inputs must be either files or a directory\n",
                                      argv[i]);
                        gb_exit(1);
                    }
                    if (!conf->files) gb_array_init(conf->files, a);
                    gb_array_append(conf->files, make_string(argv[i]));
                }
            }
        }
        else
        {
            gb_printf_err("Unknown option '%s'\n", argv[i]);
        }
    }
    
    
    if (!conf->files && !conf->directory.start)
    {
        gb_printf_err("No .c or .h file specified\n");
        gb_exit(1);
    }
    else if (((conf->files && gb_array_count(conf->files) > 1) || conf->directory.start) && conf->out_file.start)
    {
        gb_printf_err("Output location must be a directory when supplying multiple inputs\n");
        gb_exit(1);
    }
    
    gbArray(Bind_Task) tasks;
    gb_array_init(tasks, a);
    if (conf->directory.start)
    {
        gbArray(char *) entries;
        gb_array_init(entries, a);
        char *dir = make_cstring(a, conf->directory);
        if (!gb_dir_contents(dir, &entries, true))
        {
            gb_printf_err(
                          "\x1b[31mERROR:\x1b[0m Could not open directory '%.*s'\n",
                          LIT(conf->directory));
            gb_exit(1);
        }
        gb_free(a, dir);
        for (int i = 0; i < gb_array_count(entries); i++)
            gb_array_append(tasks, ((Bind_Task){conf->directory, make_string(entries[i]), {0}}));
        gb_array_free(entries);
    }
    else
    {
        for (int i = 0; i < gb_array_count(conf->files); i++)
            gb_array_append(tasks, ((Bind_Task){{0}, conf->files[i], {0}}));
    }
    
    for (int i = 0 ; i < gb_array_count(tasks); i++)
    {
        String base_name = str_path_base_name(tasks[i].input_filename);
        String root_dir = {0};
        String sub_dir  = {0};
        if (conf->out_directory.start && conf->directory.start)
        {
            root_dir = conf->out_directory;
            if (root_dir.start[root_dir.len-1] != GB_PATH_SEPARATOR)
                root_dir.start[root_dir.len++] = GB_PATH_SEPARATOR;
            
            if (conf->directory.start && tasks[i].input_filename.start+tasks[i].root_dir.len+1 != base_name.start)
            {
                sub_dir = string_slice(tasks[i].input_filename, tasks[i].root_dir.len+1, -1);
                sub_dir.len = base_name.start - sub_dir.start-1;
            }
        }
        else if (conf->out_file.start)
        {
            base_name = path_base_name(conf->out_file.start);
            root_dir = conf->out_file;
            root_dir.len = base_name.start - conf->out_file.start;
        }
        
        int base_length = root_dir.len + sub_dir.len + base_name.len;
        if (sub_dir.start) base_length++;
        
        tasks[i].output_filename.start = (char *)gb_alloc(a, base_length+6);
        tasks[i].output_filename.len = base_length+6;
        gb_snprintf(tasks[i].output_filename.start, base_length+5,
                    "%.*s%.*s%c%.*s.odin", LIT(root_dir), LIT(sub_dir), sub_dir.start?'-':0, LIT(base_name));
        
        
        gb_printf("TASK #%d:\n  root: %.*s\n  input: %.*s\n  output: %.*s\n\n",
                  i,
                  LIT(tasks[i].root_dir),
                  LIT(tasks[i].input_filename),
                  LIT(tasks[i].output_filename));
        
    }
    
    *out_tasks = tasks;
    return conf;
}
