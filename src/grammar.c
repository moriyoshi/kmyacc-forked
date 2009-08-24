/*
 * grammar - Read YACC Grammar
 *
 * Copyright (C) 1993, 2005  MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if HAS_STDLIB
# include <stdlib.h>
#endif /* HAS_STDLIB */

#include "common.h"
#include "yacc.h"
#include "misc.h"
#include "token.h"
#include "genparser.h"
#include "grammar.h"

#ifndef global

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

#endif /* global */

global Symbol *gsym[MAXSYM + 1];

global Production *gram[MAXPROD];
global int nprods, nterms, nnonts;


global int worst_error;

global Gsym error_token;

global char *union_body;
global int union_lineno;

global bool pure_flag;

#define HASHSIZE 256
static Symbol *hashtbl[HASHSIZE];

static int cur_prec;
static Gsym start_prime, start_sym;

static bool unioned;

static char M_SYNTAX[] = "Syntax error near '%s'";





#define hash(s) (((unsigned)s) >> 3)

/* Return a symbol which corresponds to string s */
Symbol *intern(char *s)
{
  Symbol *p, **root;
    
  root = hashtbl + (hash(s) % HASHSIZE);
  for (p = *root; p != NULL; p = p->next) {
    if (p->name == s)
      return (p);
  }
  p = alloc(sizeof(Symbol) + strlen(s));
  p->next = *root;
  *root = p;
  p->name = s;
  p->code = NILSYM;
  return (p);
}


/* Return non-zero if t is a terminal or a nonterminal symbol */
#define isgsym(t) ((t) == NAME || (t) == '\'')


/* Return non-zero if c is a octal digit */
#define isoctal(c) ('0' <= (c) && (c) <= '7')


/* Return value of C character sequence s */
int charval(char *s)
{
  int val;
  int c;
  int i;
    
  if ((c = *s++) != '\\')
    return c;
  c = *s++;
  if (isoctal(c)) {
    val = 0;
    for (i = 0; isoctal(*s) && i < 3; i++)
      val = val * 8 + *s++ - '0';
    return val;
  }
  switch(c) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'b': return '\b';
  case 'r': return '\r';
  case 'f': return '\f';
  case 'v': return '\013';
  case 'a': return '\007';
  default:
    error("Undefined escape sequence");
    worst_error = 1;
    /* fall thru */
  case '\'':
  case '\"':
  case '\\':
  case '\?':
    return c;
  }
}


/* Return grammar id for string s, register if new */
Gsym intern_gsym(char *s, bool term)
{
  Symbol *p;
    
  p = intern(s);
  if (p->code != NILSYM)
    return (p->code);
  if (term || p->name[0] == '\'') {
    if (nterms >= MAXTERM)
      die("Too many terminals");
    gsym[nterms] = p;
    p->code = nterms++;
    p->assoc = A_UNDEF;
    p->prec = 0;
    p->type = NULL;
    if (p->name[0] == '\'') {
      p->value = charval(p->name + 1);
    } else {
      p->value = -1;
    }
  } else {
    if (nnonts >= MAXNONT)
      die("Too many nonterminals");
    gsym[NB + nnonts] = p;
    p->code = NB + nnonts++;
    p->assoc = A_UNDEF;
    p->prec = 0;
    p->type = NULL;
    p->value = -1;
  }
  return (p->code);
}


/* Return unique nonterminal id */
Gsym gen_nont()
{
  static int n = 1;
  char buf[6];
    
  sprintf(buf, "@%d", n++);
  return (intern_gsym(intern_token(buf), NO));
}


/* Read <typename> in %type/%token, etc.
 * return interned string between balanced <>.
 */
Symbol *gettype()
{
  char buf[256], *p, *ps;
  int ct, left, len;
  int t;

  if (gettoken() != '<') {
    ungettok();
    return (NULL);
  }
  ct = 1;
  p = ps = buf;
  left = sizeof(buf) - 1;
  t = gettoken();
  for (;;) {
    switch (t) {
    case '\n':
    case EOF:
      die("missing closing >");
      break;
    case '<':
      ct++;
      break;
    case '>':
      ct--;
      break;
    }
    if (ct == 0)
      break;
    len = strlen(token_text);
    if (len > left)
      die("type too long");
    strcpy(p, token_text);
    p += len;
    left -= len;
    if (t != SPACE)
      ps = p;

    t = raw_gettoken();
  }
  *ps = '\0';
  unioned = YES;
  return (intern(intern_token(buf)));
}


/* Return length of production p */
global int length(Gsym *p)
{
  int n;
    
  for (n = 0; *++p; n++)
    ;
  return (n);
}



/* Process %token, %left, %right, %nonassoc */
void do_token(int tag)
{
  Symbol *type, *p;
  int precincr = 0;
  char t;
    
  type = gettype();
  t = gettoken();
  while (isgsym(t)) {
    p = gsym[intern_gsym(token_text, YES)];
    if (type != NULL)
      p->type = type;
    switch (tag) {
    case LEFT:
      p->assoc |= A_LEFT;
      goto setprec;
      break;
    case RIGHT:
      p->assoc |= A_RIGHT;
      goto setprec;
    case NONASSOC:
      p->assoc |= A_NON;
    setprec:
      p->prec = cur_prec;
      precincr = 1;
      break;
    }
    if ((t = gettoken()) == NUMBER) {
      if (p->value == -1)
        p->value = atoi(token_text);
      else
        error1("token %s already has a value", p->name);
      t = gettoken();
    }
    if (t == ',')
      t = gettoken();
  }
  ungettok();
  cur_prec += precincr;
}


/* Process %type statement */
void do_type()
{
  Symbol *type, *p;
  char t;
    
  type = gettype();
  for (;;) {
    if ((t = gettoken()) == ',')
      continue;
    if (!isgsym(t))
      break;
    p = gsym[intern_gsym(token_text, NO)];
    if (type != NULL)
      p->type = type;
  }
  ungettok();
}


/* Get block text surrounded by { ... } */
Flexstr *get_block_body()
{
  Flexstr *flap = new_flexstr(128);
  int ct;

  if (gettoken() != '{')
    die("{ expected");
  copy_flexstr(flap, token_text);
  ct = 1;
  while (ct != 0) {
    switch (raw_gettoken()) {
    case EOF:
      die("Unexpected EOF");
      break;
    case '{':
      ct++;
      break;
    case '}':
      ct--;
      break;
    }
    append_flexstr(flap, token_text);
  }
  return flap;
}


/* Process %union statement */
void do_union()
{
  Flexstr *flap;

  union_lineno = lineno;
  flap = get_block_body();
  union_body = flap->body;
}




/* Copy source text surrounded by %{ ... %} */
void do_copy()
{
  parser_begin_copying();
  while (raw_gettoken() != ENDINC)
    parser_copy_token(token_text);
  parser_end_copying();
}



/* Process declaration part */
global void do_declaration()
{
  int t;
  int i;
  int val;

  nterms = nnonts = 0;
  intern_gsym(get_lang_id() == LANG_PHP ? "EOF" : "$EOF", YES);
  gsym[0]->value = 0;
  error_token = intern_gsym(intern_token("error"), YES);
  start_prime = intern_gsym(get_lang_id() == LANG_PHP ? "start" : "$start", NO);
  start_sym = 0;
  cur_prec = 0;
  unioned = NO;
  while ((t = gettoken()) != MARK) {
    switch (t) {
    case TOKEN:
    case LEFT:
    case RIGHT:
    case NONASSOC:
      do_token(t);
      break;
    case BEGININC:
      do_copy();
      break;
    case UNION:
      do_union();
      unioned = YES;
      break;
    case TYPE:
      do_type();
      break;
    case EXPECT:
      if (gettoken() == NUMBER) {
        expected_srconf = atoi(token_text);
      } else {
        die("Missing number");
      }
      break;
    case START:
      gettoken();
      start_sym = intern_gsym(token_text, NO);
      break;
    case PURE_PARSER:
      pure_flag = YES;
      break;
    case EOF:
      die("No grammar given");
      break;
    default:
      die1(M_SYNTAX, token_text);
      break;
    }
  }

  /* Assign values to tokens */
  val = gsym[1]->value;
  if (val > 0)
    val++;
  else
    val = 256;
  for (i = 1; i < nterms; i++) {
    if (gsym[i]->value < 0)
      gsym[i]->value = val++;
  }
}


/* Read action */
char *copyact(Gsym *g, int n, int delm, char *attrname[])
{
  Flexstr *flap = new_flexstr(128);
  int tok;
  int ct, v, i;
  Symbol *type;

  ct = 0;
  while ((tok = raw_gettoken()) != delm || ct > 0) {
    switch (tok) {
    case EOF:
      die("Unexpected EOF");
      break;
    case '{':
      ct++;
      break;
    case '}':
      ct--;
      break;
    case NAME:
      if (!nflag)
        break;
      type = NULL;
      v = -1;
      for (i = 0; i <= n; i++) {
        if (gsym[g[i]]->name == token_text) {
          if (v < 0)
            v = i;
          else {
            error1("ambiguous semantic value reference: '%s'", token_text);
            break;
          }
        }
      }
      if (v < 0) {
        for (i = 0; i <= n; i++) {
          if (attrname[i] == token_text) {
            v = i;
            break;
          }
        }
        if (token_text == attrname[n + 1])
          v = 0;
      }
      if (v >= 0) {
        tok = (v == 0 ? '$' : 0);
        goto semval;
      }
      break;
    case '$':
      if (iflag)
        break;
      type = NULL;
      tok = raw_gettoken();
      if (tok == '<') {
        if (raw_gettoken() != NAME)
          error("type expected");
        type = intern(token_text);
        if (raw_gettoken() != '>')
          error("missing >");
        tok = raw_gettoken();
      }
      v = 1;
      if (tok == '$')
        v = 0;
      else if (tok == '-') {
        tok = raw_gettoken();
        if (tok != NUMBER)
          error("number expected");
        else
          v = -atoi(token_text);
      }
      else {
        if (tok != NUMBER)
          error("number expected");
        else {
          v = atoi(token_text);
          if (v > n) {
            error("too big $N");
            v = 1;
          }
        }
      }
    semval:
      if (type == NULL)
        type = gsym[g[v]]->type;
      if (type == NULL && unioned) {
        error1("type not defined for `%s'", gsym[g[v]]->name);
        worst_error = 1;
      }
      append_flexstr
        (flap, parser_dollar(tok, v, n, type ? type->name : NULL));
      continue;
    }
    append_flexstr(flap, token_text);
  }
  return flap->body;
}


/* Read grammar */
global void do_grammar()
{
  Gsym gbuf[100], lastterm, w;
  char *attrname[100];
  Production *r;
  char *action;
  int pos;
  int i;
  int t;
    
  r = alloc(sizeof(Production) + sizeof(Gsym) * 3);
  r->body[1] = start_prime;
  r->link = -1;
  gram[0] = r;
  nprods = 1;
  gbuf[0] = 0;
  t = gettoken();
  while (t != MARK && t != EOF) {
    if (t == NAME) {
      if (peektoken() == '@') {
        attrname[0] = token_text;
        gettoken();
        t = gettoken();
      } else {
        attrname[0] = NULL;
      }
      gbuf[0] = intern_gsym(token_text, NO);
      attrname[1] = NULL;
      if (isterm(gbuf[0]))
        die1("Nonterminal symbol expected: '%s'", token_text);
      if (gettoken() != ':')
        die("':' expected");
      if (start_sym == 0)
        start_sym = gbuf[0];
    } else if (t == '|') {
      if (gbuf[0] == 0)
        die1(M_SYNTAX, token_text);
      attrname[1] = NULL;
    } else if (t == BEGININC) {
      do_copy();
      t = gettoken();
      continue;
    } else {
      die1(M_SYNTAX, token_text);
    }
    lastterm = 0;
    action = NULL;
    pos = 0;
    i = 1;
    for (;;) {
      t = gettoken();
      if (t == '=') {
        if ((t = gettoken()) == '{') {
          pos = lineno;
          action = copyact(gbuf, i - 1, '}', attrname);
        } else {
          ungettok();
          pos = lineno;
          action = copyact(gbuf, i - 1, ';', attrname);
        }
      }
      else if (t == '{') {
        pos = lineno;
        action = copyact(gbuf, i - 1, '}', attrname);
      }
      else if (t == PRECTOK) {
        gettoken();
        lastterm = intern_gsym(token_text, NO);
      }
      else if (t == NAME && peektoken() == ':')
        break;
      else if (t == NAME && peektoken() == '@') {
        attrname[i] = token_text;
        gettoken();
      }
      else if (isgsym(t)) {
        if (action) {
          r = alloc(sizeof(Production) + sizeof(Gsym) * 2);
          r->action = action;
          r->pos = pos;
          gbuf[i++] = r->body[1] = gen_nont();
          attrname[i] = NULL;
          r->link = gsym[r->body[1]]->value;
          gsym[r->body[1]]->value = nprods;
          gram[nprods++] = r;
        }
        gbuf[i++] = w = intern_gsym(token_text, NO);
        attrname[i] = NULL;
        if (isterm(w))
          lastterm = w;
        action = NULL;
      }
      else
        break;
    }
    gbuf[i++] = 0;
    if (!action) {
      if (i > 2 && gsym[gbuf[0]]->type != NULL
          && gsym[gbuf[0]]->type != gsym[gbuf[1]]->type) {
        error("Stack types are different");
        worst_error = 1;
      }
    }
    r = alloc(sizeof(Production) + sizeof(Gsym) * i);
    r->action = action;
    r->pos = pos;
    memcpy(r->body + 1, gbuf, sizeof(Gsym) * i);
    r->prec = gsym[lastterm]->prec;
    r->assoc = gsym[lastterm]->assoc & A_MASK;
    r->link = gsym[r->body[1]]->value;
    gsym[r->body[1]]->value = nprods;
    gram[nprods++] = r;
    if (t == ';')
      t = gettoken();
  }

  gram[0]->body[2] = start_sym;
  gsym[start_prime]->value = 0;

  for (i = NB + 1; i < NB + nnonts; i++) {
    /* reverse same-LHS link */
    int j, k, w;

    if ((j = gsym[i]->value) == -1) {
      error1("Nonterminal '%s' used but not defined", gsym[i]->name);
      worst_error = 1;
    }
    k = -1;
    while (j != -1) {
      w = gram[j]->link;
      gram[j]->link = k;
      k = j;
      j = w;
    }
    gsym[i]->value = k;
  }

#if 0
  if (debug) {
    Gsym *p;

    fprintf(vfp, "grammar read:\n");
    for (i = 0; i < nprods; i++) {
      fprintf(vfp, "(%d)\t%s\t:", i, gsym[gram[i]->body[1]]->name);
      for (p = gram[i]->body + 2; *p != 0; p++)
        fprintf(vfp, " %s", gsym[*p]->name);
      putc('\n', vfp);
    }
  }
#endif
}
