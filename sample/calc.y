/*
 * integer calculator
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
%}

%pure_parser

%token NUMBER

%left '+' '-'
%left '*' '/'

%%

start:	lines;

lines: /* empty */
	| lines line
        ;

line	: expr '\n' { printf("%d\n", $1); }
	| '\n' { printf("(empty line ignored)\n"); }
	| error '\n'
	;

expr	: expr '+' expr { $$ = $1 + $3; }
	| expr '-' expr { $$ = $1 - $3; }
	| expr '*' expr { $$ = $1 * $3; }
	| expr '/' expr { $$ = $1 / $3; }
	| '(' expr ')' { $$ = $2; }
	| NUMBER { $$ = $1; }
	;

%%



#if YYPURE
#define YYLEXARGS YYSTYPE *yylvalp
#define YYLVAL (*yylvalp)
#endif /* YYPURE */

#if !YYPURE
#define YYLEXARGS 
#define YYLVAL yylval
#endif

int yylex(YYLEXARGS)
{
    int c;
    char *p;
    char buf[100];

 again:
    c = getc(stdin);
    if (c == EOF)
        return 0;
    else if (c == ' ' || c == '\t')
        goto again;
    else if (isdigit(c)) {
        for (p = buf; isdigit(c); ) {
            *p++ = c;
            c = getc(stdin);
        }
        *p = '\0';
        ungetc(c, stdin);
        YYLVAL = atoi(buf);
        return NUMBER;
    } else
        return c;
}

int yyerror(s)
char *s;
{
    fprintf(stderr, "%s\n", s);
}

int main()
{
#ifdef YYDEBUG
    yydebug = 1;
#endif

    return yyparse();
}
