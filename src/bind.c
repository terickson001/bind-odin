#include "bind.h"

#include "util.h"

#include "tokenizer.h"
#include "define.h"
#include "preprocess.h"
#include "parse.h"
#include "print.h"
#include "types.h"
#include "hashmap.h"

void bind_generate(Config *conf, gbArray(Bind_Task) tasks)
{
    gbAllocator a = gb_heap_allocator();
    
    gbFileContents fc = {0};

    gbArray(gbFileContents) files_to_free;
    gb_array_init(files_to_free, a);

    Package package = {0};
    gb_array_init(package.files, a);
    package.lib_name = conf->bind_conf.lib_name;
    package.name     = conf->bind_conf.package_name;;

    map_t type_table = hashmap_new(a);
    
    for (int t = 0; t < gb_array_count(tasks); t++)
    {
        Bind_Task task = tasks[t];

        char *filename = make_cstring(a, task.input_filename);
        gbFile f = {0};
        fc = gb_file_read_contents(a, true, filename);
        if (!fc.data)
        {
            if (gb_file_exists(filename))
                gb_printf_err("\e[31mERROR:\e[0m File '%.*s' is empty\n", LIT(task.input_filename));
            else
                gb_printf_err("\e[31mERROR:\e[0m Failed to open file \'%.*s\'\n", LIT(task.input_filename));
            gb_free(a, filename);
            exit(1);
        }
        gb_free(a, filename);
        
        Tokenizer tokenizer = make_tokenizer(fc, task.input_filename);
        gbArray(Token) tokens;
        gb_array_init(tokens, a);
        Token token;
        for (;;)
        {
            token = get_token(&tokenizer);
            if (token.kind != Token_Invalid)
                gb_array_append(tokens, token);
            if (token.kind == Token_EOF)
                break;
        }

        String root_dir;
        if (task.root_dir.start)
            root_dir = task.root_dir;
        else
            root_dir = dir_from_path(task.input_filename);

        Preprocessor *pp = make_preprocessor(tokens, root_dir, task.input_filename, &conf->pp_conf);
        run_pp(pp);

        gbArray(Define) defines = pp_dump_defines(pp, task.input_filename);
        
        gb_array_append(pp->output, (Token){.kind=Token_EOF});
        Parser parser = make_parser(0);
        hashmap_free(parser.type_table);
        parser.type_table = type_table;
        parser.start = parser.curr = pp->output;
        parser.end = parser.start + gb_array_count(pp->output)-1;
        parse_file(&parser);
        parser.file.raw_defines = defines;
        
        parser.file.filename = make_cstring(a, task.output_filename);
        gb_array_append(package.files, parser.file);
        
        // destroy_preprocessor(pp);
        // destroy_parser(parser);

        // gb_file_free_contents(&fc);
        gb_array_append(files_to_free, fc);
    }

    Parser parser = make_parser(0);
    hashmap_free(parser.type_table);
    parser.type_table = type_table;
    for (int i = 0; i < gb_array_count(package.files); i++)
    {
        parser.file = package.files[i];
        parse_defines(&parser, package.files[i].raw_defines);
        package.files[i].defines = parser.file.defines;
    }
    
    Resolver resolver = make_resolver(package, &conf->bind_conf);
    resolve_package(&resolver);

    Printer printer = make_printer(resolver);
    print_package(printer);
}
