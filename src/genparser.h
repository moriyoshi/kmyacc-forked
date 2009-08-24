#ifndef _genparser_h
#define _genparser_h

/* Language ID */
#define LANG_C		0
#define LANG_CPP	1
#define LANG_JAVA	2
#define LANG_PERL	3
#define LANG_RUBY	4
#define LANG_JS		5
#define LANG_ML		6
#define LANG_PYTHON	7
#define LANG_PHP	8
#define LANG_CSHARP 9
#define LANG_AS		10
#define LANG_HSP	11

extern char *get_lang_name(void);
extern int get_lang_id(void);
extern void parser_set_language(char *langname);
extern void parser_set_language_by_yaccext(char *ext);
extern char *parser_outfilename(char *pref, char *yacc_filename);
extern char *parser_modelfilename(char *parser_base);
extern char *parser_header_filename(char *pref, char *yacc_filename);
extern void parser_create(char *fn, bool tflag);
extern void parser_begin_copying(void);
extern void parser_copy_token(char *str);
extern void parser_end_copying(void);
extern char *parser_dollar(int dollartype, int nth, int len, char *typename);
extern void parser_generate(void);
extern void parser_close(void);
#endif
