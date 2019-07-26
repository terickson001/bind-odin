#ifndef C_PREPROCESSOR_DIRECTIVES_H
#define C_PREPROCESSOR_DIRECTIVES_H 1

typedef struct Define
{
    String name;
    String value;

    b32 is_function;
    gbArray(String) params;
} Define;

#endif /* ifndef C_PREPROCESSOR_DIRECTIVES_H */
