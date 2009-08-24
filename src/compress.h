#ifndef _compress_h
#define _compress_h
#define YYUNEXPECTED 32767
#define YYDEFAULT -32766
extern short *yytranslate;
extern int yytranslatesize;
extern int yyncterms;
extern short *yyaction;
extern int yyactionsize;
extern short *yybase;
extern int yybasesize;
extern short *yycheck;
extern short *yydefault;
extern short *yygoto;
extern int yygotosize;
extern short *yygbase;
extern short *yygcheck;
extern short *yygdefault;
extern short *yylhs;
extern short *yylen;
extern short *ctermindex;
extern int convert_sym(int sym);
extern void makeup_table2(void);
#endif
