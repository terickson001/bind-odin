#ifndef _C_BIND_H_
#define _C_BIND_H_

#include "gb/gb.h"
#include "string.h"
#include "config.h"

typedef struct Bind_Task
{
    String root_dir;
    String input_filename;
    String output_filename;
} Bind_Task;

void bind_generate(Config *conf, gbArray(Bind_Task) tasks);

#endif
