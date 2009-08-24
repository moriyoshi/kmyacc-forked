/*
 * tokentest - test driver for token.c
 *
 * Copyright (C) 1993, 2005  MORI Koichiro
 */

#include <stdio.h>

#include "token.h"

FILE *ifp;

int lineno = 0;
char *filename = "";

int main()
{
    int token;

    ifp = stdin;
    while ((token = raw_gettoken()) != EOF) {
        printf(token >= ' ' && token < 0x7f ? "read: '%c'" : "read: %d", token);
        printf(" |%s|\n", token_text);
    }
}
