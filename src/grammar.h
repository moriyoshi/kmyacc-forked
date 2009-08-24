#ifndef _grammar_h
#define _grammar_h

#define MAXSYM (MAXNONT + MAXTERM)

#if MAXSYM < 256
typedef unsigned char Gsym;
#else
typedef short Gsym;
#endif


#define A_UNDEF	0
#define A_LEFT	1
#define A_RIGHT	2
#define A_NON	3
#define A_MASK	3

#define NILSYM MAXSYM
#define NB MAXTERM

typedef struct symbol {
  struct symbol *next;
  struct symbol *type;
  short value;
  Gsym code;
  uchar prec;
  uchar assoc;
#define F_EMPTY 0x80
#define F_CONTEXT 0x40
  char *name;
} Symbol;

typedef struct production {
  short link; /* next prod# which has same LHS */
  uchar assoc;
  uchar prec;
  int pos;
  char *action;
  Gsym body[1];
} Production;

/* Return non-zero if grammar symbol s is a terminal */
#define isterm(s) ((s) < MAXTERM)

extern Symbol *gsym[MAXSYM + 1];
extern Production *gram[MAXPROD];
extern int nprods, nterms, nnonts;
extern int worst_error;
extern Gsym error_token;
extern char *union_body;
extern int union_lineno;
extern bool pure_flag;
extern int length(Gsym *p);
extern void do_declaration(void);
extern void do_grammar(void);
#endif
