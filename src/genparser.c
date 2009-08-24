/*
 * Generate parser code from model file
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
#include <assert.h>

#include "common.h"
#include "token.h"
#include "yacc.h"
#include "misc.h"
#include "grammar.h"
#include "lalr.h"
#include "compress.h"

#include "genparser.h"

#ifndef global

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
#define LANG_CSHAPR	9
#define LANG_AS		10
#define LANG_HSP	11

#endif /* global */


/* Language name and extensions table. */
typedef struct langmap {
  int id;
  char *name;
  char *ext;
  char *yext;
} LANGMAP;


/* Meta character */
private char metachar = '$';

private char *parserfn;
private FILE *pfp;
private int plineno;

private bool copy_header = NO;


/** Host programming language. **/
private LANGMAP *language;


#define iswhite(c)	((c) == ' ' || (c) == '\t')



void proto_error(char *fmt, char *x)
{
  char c;

  fprintf(stderr, "%s:%d: ", parserfn, plineno);
  while ((c = *fmt++) != '\0') {
    if (c == '%' && *fmt != '\0') {
      switch (c = *fmt++) {
      case 's':
        fputs(x, stderr);
        break;
      default:
        putc(c, stderr);
        break;
      }
    } else {
      putc(c, stderr);
    }
  }
  putc('\n', stderr);
}



/* Language name and extensions table. */
private LANGMAP langmap[] = {
  { LANG_C, "c", ".c", ".y" },
  { LANG_CPP, "cpp", ".cpp", ".cpy" },
  { LANG_JAVA, "java", ".java", ".jy" },
  { LANG_PERL, "perl", ".pl", ".ply" },
  { LANG_RUBY, "ruby", ".rb", ".rby" },
  { LANG_JS, "javascript", ".js", ".jsy" },
  { LANG_ML, "ml", ".ml", ".mly" },
  { LANG_PYTHON, "py", ".py", ".pyy" },
  { LANG_PHP, "php", ".php", ".phpy" },
  { LANG_CSHARP, "csharp", ".cs", ".csy" },
  { LANG_AS, "as", ".as", ".asy" },
  { LANG_HSP, "hsp", ".hsp", ".hspy" },
  { -1, NULL, NULL, NULL }
};


/* Find language by name. */
LANGMAP *lang_byname(char *lang)
{
  struct langmap *p;
  for (p = langmap; p->id >= 0; p++) {
    if (strcmp(p->name, lang) == 0)
      return p;
  }
  return NULL;
}


/* Deduce language from yacc filename. */
LANGMAP *lang_byyaccext(char *yext)
{
  struct langmap *p;
  for (p = langmap; p->id >= 0; p++) {
    if (strcmp(p->yext, yext) == 0)
      return p;
  }
  return NULL;
}


/* Return language name string. */
global char *get_lang_name()
{
  return language->name;
}

/* Return language id. */
global int get_lang_id()
{
  return language->id;
}


/* Insert #line into the parser code */
void print_line(int ln, char *fn)
{
  switch (language->id) {
  case LANG_C: case LANG_CPP:
    if (!lflag)
      fprintf(ofp, "#line %d \"%s\"\n", ln, fn);
  }
}



bool strmatch(char *text, char *pattern)
{
  return strncmp(text, pattern, strlen(pattern)) == 0;
}

bool metamatch(char *text, char *keyword)
{
  return *text == metachar
    && strncmp(text + 1, keyword, strlen(keyword)) == 0;
}

void unsupported()
{
  die1("%s: unsupported language", language->name);
}


/* Language-dependent initialization. */
void init_lang()
{
  switch (language->id) {
  case LANG_C:
    break;
  case LANG_PERL:
    /* In Perl, $$, $1, $2... are inhibited */
    iflag = YES;
    /* fall thru */
  default:
    /* Enables semantic value reference by name except C language. */
    nflag = YES;
    break;
  }
}


/* Set host language by name. */
global void parser_set_language(char *langname)
{
  language = lang_byname(langname);
  if (language == NULL)
    die1("%s: unsupported host language", langname);
  init_lang();
}


/* Set host language infered from yacc filename. */
global void parser_set_language_by_yaccext(char *ext)
{
  language = lang_byyaccext(ext);
  if (language == NULL)
    die1("%s: unknown yacc extension.", ext);
  init_lang();
}

#define MAXPATHLEN 256

/* Return output parser file name. */
global char *parser_outfilename(char *pref, char *yacc_filename)
{
  static char fn[MAXPATHLEN];

  switch (get_lang_id()) {
  case LANG_C: case LANG_CPP:
#ifdef MSDOS
    strcat(strcpy(fn, pref ? pref : "ytab"), ".c");
#else
    strcat(strcpy(fn, pref ? pref : "y"), ".tab.c");
#endif /* MSDOS */
    return fn;

  default:
    /* Foo.y -> Foo.type if pref == NULL
     * Foo.y -> Bar.type if pref == "Bar"
     */
    if (pref == NULL) {
      int n = extension(yacc_filename) - yacc_filename;
      strncpy(fn, yacc_filename, n);
      fn[n] = '\0';
    } else {
      strcpy(fn, pref);
    }
    strcat(fn, language->ext);
    return fn;
  }
}


/* Return parser model file name. */
global char *parser_modelfilename(char *parser_base)
{
  static char fn[MAXPATHLEN];

  strcpy(fn, parser_base);
  strcat(fn, language->ext);
#ifndef MSDOS
  strcat(fn, ".parser");
#endif /* !MSDOS */
  return fn;
}


/* Return output parser header file name. */
global char *parser_header_filename(char *pref, char *yacc_filename)
{
  static char fn[MAXPATHLEN];

  switch (get_lang_id()) {
  case LANG_C: case LANG_CPP:
#ifdef MSDOS
    strcat(strcpy(fn, pref ? pref : "ytab"), ".h");
#else
    strcat(strcpy(fn, pref ? pref : "y"), ".tab.h");
#endif /* MSDOS */
    return fn;

  default:
    return NULL;
  }
}


void def_semval_macro(char *);
void set_metachar(char *);

/* Initialize parser generator. */
global void parser_create(char *fn, bool tflag)
{
  char line[256];
  char spaces[256];

  parserfn = fn;
  pfp = efopen(parserfn, "r");

  /* Read parameters. */

  /* Copy parser - 1/3 */
  plineno = 1;
  print_line(plineno, parserfn);

  while (fgets(line, sizeof(line), pfp) != NULL) {
    char *p, *q;
    for (p = line, q = spaces; iswhite(*p); )
      *q++ = *p++;
    *q = '\0';
    plineno++;
    if (metamatch(p, "include"))
      break;
    if (metamatch(p, "meta"))
      set_metachar(p + 5);
    else if (metamatch(p, "semval"))
      def_semval_macro(p + 7);
    else
      fputs(line, ofp);
  }
}


  

global void parser_begin_copying()
{
  print_line(lineno, filename);
}


global void parser_copy_token(char *str)
{
  fprintf(ofp, "%s", str);
}


global void parser_end_copying()
{
}



char *enough_type(short *p, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    if (p[i] > 0x7f)
      return "short";
  }
  switch (get_lang_id()) {
  case LANG_C: case LANG_CPP:
    return "char";
  case LANG_JAVA:
    return "byte";
  case LANG_CSHARP:
    return "sbyte";
  case LANG_AS:
  	return "int";
  default:
    unsupported();
    return 0; // Remove compiler warning
  }
}


void print_array(short *p, int n, char *indent)
{
  int i, col;
  col = 0;
  for (i = 0; i < n; i++) {
    if (col == 0)
      fprintf(ofp, "%s", indent);
    fprintf(ofp, i + 1 == n ? "%5d" : "%5d,", p[i]);
    if (++col == 10) {
      if(get_lang_id() == LANG_PYTHON || get_lang_id() == LANG_HSP)
        fprintf(ofp, " \\");
      fprintf(ofp, "\n");
      col = 0;
    }
  }
  if (col != 0) {
    if(get_lang_id() == LANG_PYTHON)
      fprintf(ofp, " \\");
    fprintf(ofp, "\n");
  }
}




private char *skipsp(char *s)
{
  while (iswhite(*s))
    s++;
  return s;
}

private char *skipnosp(char *s)
{
  while (*s && !isspace(*s))
    s++;
  return s;
} 

private char *quote(char *s)
{
  static char buf[128];
  char *p;

  p = buf;
  while (*s) {
    if (*s == '\\' || *s == '\"')
      *p++ = '\\';
    *p++ = *s++;
  }
  *p = '\0';
  return buf;
}



#define isidentch(c) (isalpha(c) || isdigit(c) || (c) == '_')



private char *semval_lhs_untyped;
private char *semval_lhs_typed;
private char *semval_rhs_untyped;
private char *semval_rhs_typed;

private char *copy_rest_of_line(char *p)
{
  char *s;

  for (s = p; *s && *s != '\n'; s++)
    ;
  *s = '\0';
  s = talloc(strlen(p) + 1);
  strcpy(s, p);
  return s;
}


/*
 * Define how semantic values ($$/$n) in action are expanded.
 *
 *  $semval($,%t) yyval.%t
 *  $semval($) yyval
 *  $semval(%n) yyastk[yysp-(%l-%n)]
 *  $semval(%n,%t) (%t)yyastk[yysp-(%l-%n)]
 */
void def_semval_macro(char *p)
{
  if (strmatch(p, "($)"))
    semval_lhs_untyped = copy_rest_of_line(skipsp(p + 3));
  else if (strmatch(p, "($,%t)"))
    semval_lhs_typed = copy_rest_of_line(skipsp(p + 6));
  else if (strmatch(p, "(%n)"))
    semval_rhs_untyped = copy_rest_of_line(skipsp(p + 4));
  else if (strmatch(p, "(%n,%t)"))
    semval_rhs_typed = copy_rest_of_line(skipsp(p + 7));
  else {
    proto_error("$semval: bad format: %s", p);
    exit(1);
  }
}



void set_metachar(char *p)
{
  p = skipsp(p);
  if (*p == '\0' || isspace(*p)) {
    proto_error("$meta: missing character", NULL);
    exit(1);
  }
  metachar = *p;
}




global char *parser_dollar(int dollartype, int nth, int len, char *typename)
{
  static char buf[128];
  char *mp, *dp;

  if (dollartype == '$') {
    if (typename)
      mp = semval_lhs_typed;
    else
      mp = semval_lhs_untyped;
  } else {
    if (typename)
      mp = semval_rhs_typed;
    else
      mp = semval_rhs_untyped;
  }

  if (mp == NULL) {
    proto_error("Semantic value macro undefined", NULL);
    exit(1);
  }

  /* expand macro */
  for (dp = buf; *mp; ) {
    if (*mp == '%') {
      mp++;
      switch (*mp) {
      case 'n':
        sprintf(dp, "%d", nth);
        break;
      case 'l':
        sprintf(dp, "%d", len);
        break;
      case 't':
        strcpy(dp, typename);
        break;
      default:
        *dp++ = *mp;
        *dp = '\0';
        break;
      }
      dp += strlen(dp);
      mp++;
    } else
      *dp++ = *mp++;
  }
  *dp = '\0';

  return buf;
}

        

/* Make class name from filename */
void class_name(char *buf, char *filename)
{
  char *p, *s;
  for (s = p = extension(filename); --s >= filename && isidentch(*s); )
    ;
  s++;
  strncpy(buf, s, p - s);
  buf[p - s] = '\0';
}


/** Print comma-separated list of variable **/
void gen_list_var(char *indent, char *var)
{
  if (strcmp(var, "yytranslate") == 0)
    print_array(yytranslate, yytranslatesize, indent);
  else if (strcmp(var, "yyaction") == 0)
    print_array(yyaction, yyactionsize, indent);
  else if (strcmp(var, "yycheck") == 0)
    print_array(yycheck, yyactionsize, indent);
  else if (strcmp(var, "yybase") == 0)
    print_array(yybase, yybasesize, indent);
  else if (strcmp(var, "yydefault") == 0)
    print_array(yydefault, nnonleafstates, indent);
  else if (strcmp(var, "yygoto") == 0)
    print_array(yygoto, yygotosize, indent);
  else if (strcmp(var, "yygcheck") == 0)
    print_array(yygcheck, yygotosize, indent);
  else if (strcmp(var, "yygbase") == 0)
    print_array(yygbase, nnonts, indent);
  else if (strcmp(var, "yygdefault") == 0)
    print_array(yygdefault, nnonts, indent);
  else if (strcmp(var, "yylhs") == 0)
    print_array(yylhs, nprods, indent);
  else if (strcmp(var, "yylen") == 0)
    print_array(yylen, nprods, indent);
  else if (strcmp(var, "terminals") == 0) {
    int i;
    int nl = 0;
    for (i = 0; i < nterms; i++) {
      if (ctermindex[i] >= 0) {
        if (nl++) {
          if ( get_lang_id() == LANG_HSP )
            fprintf(ofp, ",\\\n" );
          else
            fprintf(ofp, ",\n");
        }
        fprintf(ofp, "%s\"%s\"", indent, quote(gsym[i]->name));
      }
    }
    if ( get_lang_id() == LANG_HSP )
        fprintf(ofp, "\\" );
    fprintf(ofp, "\n");
  }
  else if (strcmp(var, "nonterminals") == 0) {
    int i;
    int nl = 0;
    for (i = 0; i < nnonts; i++) {
      if (nl++) {
        if ( get_lang_id() == LANG_HSP )
          fprintf(ofp, ",\\\n" );
        else
          fprintf(ofp, ",\n");
      }
      fprintf(ofp, "%s\"%s\"", indent, quote(gsym[NB + i]->name));
    }
    if ( get_lang_id() == LANG_HSP )
        fprintf(ofp, "\\" );
    fprintf(ofp, "\n");
  }
  else {
    proto_error("$listvar: unknown variable: %s", var);
    exit(1);
  }
}


/** Generate enough type for variable. **/
void gen_enoughtypeof(char *buf, char *var)
{
  short *p;
  int n;
  
  if (strcmp(var, "yytranslate") == 0)
    p = yytranslate, n = yytranslatesize;
  else if (strcmp(var, "yycheck") == 0)
    p = yycheck, n = yyactionsize;
  else if (strcmp(var, "yygcheck") == 0)
    p = yygcheck, n = yygotosize;
  else if (strcmp(var, "yylhs") == 0)
    p = yylhs, n = nprods;
  else if (strcmp(var, "yylen") == 0)
    p = yylen, n = nprods;
  else {
    proto_error("$TYPEOF: unknown variable: %s", var);
    exit(1);
  }
  strcpy(buf, enough_type(p, n));
}


/* Generate valueof variable */
void gen_valueof(char *buf, char *var)
{
  if (strcmp(var, "YYSTATES") == 0)
    sprintf(buf, "%d", nstates);
  else if (strcmp(var, "YYNLSTATES") == 0)
    sprintf(buf, "%d", nnonleafstates);
  else if (strcmp(var, "YYINTERRTOK") == 0)
    sprintf(buf, "%d", yytranslate[gsym[error_token]->value]);
  else if (strcmp(var, "YYUNEXPECTED") == 0)
    sprintf(buf, "%d", YYUNEXPECTED);
  else if (strcmp(var, "YYDEFAULT") == 0)
    sprintf(buf, "%d", YYDEFAULT);
  else if (strcmp(var, "YYTERMS") == 0)
    sprintf(buf, "%d", yyncterms);
  else if (strcmp(var, "YYNONTERMS") == 0)
    sprintf(buf, "%d", nnonts);
  else if (strcmp(var, "YYBADCH") == 0)
    sprintf(buf, "%d", yyncterms);
  else if (strcmp(var, "YYMAXLEX") == 0)
    sprintf(buf, "%d", yytranslatesize);
  else if (strcmp(var, "YYLAST") == 0)
    sprintf(buf, "%d", yyactionsize);
  else if (strcmp(var, "YYGLAST") == 0)
    sprintf(buf, "%d", yygotosize);
  else if (strcmp(var, "YY2TBLSTATE") == 0)
    sprintf(buf, "%d", yybasesize - nnonleafstates);
  else if (strcmp(var, "CLASSNAME") == 0)
    class_name(buf, outfilename);
  else if (strcmp(var, "-p") == 0)
    strcpy(buf, pspref ? pspref : "yy");
  else {
    proto_error("unknown variable: $(%s)", var);
    exit(1);
  }
}


void expand_mac(char *def, int num, char *str)
{
  char buf[512];
  char *s = buf;
  char *p;
  for (p = def; *p; p++) {
    if (*p == '%') {
      p++;
      switch (*p) {
      case 'n':
        sprintf(s, "%d", num);
        s += strlen(s);
        break;
      case 's':
        sprintf(s, "%s", str);
        s += strlen(s);
        break;
      case 'b':
        *s = '\0';
        print_line(gram[num]->pos, filename);
        fputs(buf, ofp);
        fputs(gram[num]->action, ofp);
        s = buf;
        break;
      default:
        *s++ = *p;
        break;
      }
    } else
      *s++ = *p;
  }
  *s = '\0';
  fputs(buf, ofp);
  if (copy_header)
    fputs(buf, hfp);
}



/** Generate token values **/
void gen_tokenval()
{
  char line[256];
  char *mac[100];
  int i, j, n;

  /* read macro body */
  n = 0;
  for (; fgets(line, sizeof(line), pfp); plineno++) {
    if (metamatch(line, "endtokenval")) {
      plineno++;
      break;
    }
    if (n >= height(mac))
      proto_error("$tokenval: macro too large", NULL);
    mac[n] = talloc(strlen(line) + 1);
    strcpy(mac[n], line);
    n++;
  }

  /* expand */
  for (i = 1; i < nterms; i++) {
    if (gsym[i]->name[0] != '\'') {
      char *str = gsym[i]->name;
      if (i == 1)
        str = "YYERRTOK";
      for (j = 0; j < n; j++)
        expand_mac(mac[j], gsym[i]->value, str);
    }
  }
}


/** Generate reduce actions **/
void gen_reduce()
{
  char line[256];
  char *mac[100];
  int i, j, n, m;

  /* read macro body */
  n = 0;
  m = -1;
  for (; fgets(line, sizeof(line), pfp); plineno++) {
    if (metamatch(line, "endreduce")) {
      plineno++;
      break;
    }
    if (metamatch(line, "noact")) {
      m = n;
      continue;
    }
    if (n >= height(mac))
      proto_error("$reduce: macro too large", NULL);
    mac[n] = talloc(strlen(line) + 1);
    strcpy(mac[n], line);
    n++;
  }
  if (m < 0)
    m = n;

  /* expand */
  for (i = 0; i < nprods; i++) {
    if (gram[i]->action) {
      for (j = 0; j < m; j++)
        expand_mac(mac[j], i, NULL);
    } else {
      for (j = m; j < n; j++)
        expand_mac(mac[j], i, NULL);
    }
  }
}


/** Generate parser code. **/
global void parser_generate()
{
  char line[256];
  char buf[512];
  char *spaces;
  bool skipmode = NO;
  bool line_changed = NO;
  makeup_table2();

  print_line(plineno, parserfn);

  for (; fgets(line, sizeof(line), pfp); plineno++) {
    char *p, *q;

    if (skipmode) {
      if (metamatch(skipsp(line), "endif"))
        skipmode = NO;
      continue;
    }

    for (p = line, q = buf; *p; ) {
      if (*p != metachar)
        *q++ = *p++;
      else if (p[1] == metachar) {
        p++;
        p++;
        *q++ = metachar;
      }
      else if (metamatch(p, "(")) {
        char *var = (p += 2);
        while (*p && *p != ')') p++;
        if (*p != ')') {
          proto_error("$(: missing ')'", NULL);
          exit(1);
        }
        *p++ = '\0';
        gen_valueof(q, var);
        q += strlen(q);
      }
      else if (metamatch(p, "TYPEOF(")) {
        char *var = (p += 8);
        while (*p && *p != ')') p++;
        if (*p != ')') {
          proto_error("$TYPEOF: missing ')'", NULL);
          exit(1);
        }
        *p++ = '\0';
        gen_enoughtypeof(q, var);
        q += strlen(q);
      }
      else
        break;
    }
    *q = '\0';

    if (*p == metachar) {
      /* Line-oriented $-keywords */
      for (q = buf; *q; q++) {
        if (!iswhite(*q)) {
          proto_error("non-blank character before $-keyword.\n", NULL);
          exit(1);
        }
      }
      spaces = buf;
      if (metamatch(p, "header")) {
        if (hfp) copy_header = YES;
      }
      else if (metamatch(p, "endheader")) {
        copy_header = NO;
      }
      else if (metamatch(p, "tailcode")) {
        /* Copy rest of source */
        while (raw_gettoken() != NEWLINE)
          ;
        print_line(lineno, filename);
        while (fgets(line, sizeof(line), ifp)) {
          fputs(spaces, ofp);
          fputs(line, ofp);
        }
      }
      else if (metamatch(p, "verification-table")) {
        int i, j;
        fprintf(ofp, "static short yyvalidnonterms[] = {\n");
        for (i = 0; i < nnonleafstates; i++) {
          fprintf(ofp, "  /* state %d */\n", i);
          for (j = 0; statev[i]->shifts[j]; j++) {
            if (!isterm(statev[i]->shifts[j]->thru))
              fprintf(ofp, "  %d,\n", statev[i]->shifts[j]->thru - NB);
          }
          fprintf(ofp, "  -1,\n");
        }
        fprintf(ofp, "  -2\n};\n");
      }
      else if (metamatch(p, "union")) {
        fputs(union_body, ofp);
        fputs("\n", ofp);
        if (copy_header) {
          fputs(union_body, hfp);
          fputs("\n", hfp);
        }
      }
      else if (metamatch(p, "tokenval")) {
        gen_tokenval();
      }
      else if (metamatch(p, "reduce")) {
        gen_reduce();
      }
      else if (metamatch(p, "switch-for-token-name")) {
        int i;
        for (i = 0; i < nterms; i++) {
          if (ctermindex[i] >= 0) {
          	if(get_lang_id() != LANG_PYTHON) {
              fprintf(ofp, "%scase %d: return \"%s\";\n",
                      spaces,
                      gsym[i]->value,
                      quote(gsym[i]->name));
            } else {
              fprintf(ofp, "%sif n == %d:\n%s    return \"%s\"\n",
                      spaces,
                      gsym[i]->value,
                      spaces,
                      quote(gsym[i]->name));
            }
          }
        }
      }
      else if (metamatch(p, "switch-for-nonterminal-name")) {
        int i;
        for (i = 0; i < nnonts; i++) {
          fprintf(ofp, "%scase %d: return \"%s\";\n",
                  spaces, i, quote(gsym[NB + i]->name));
        }
      }
      else if (metamatch(p, "production-strings")) {
        int i;
        Gsym *s;
        for (i = 0; i < nprods; i++) {
          s = gram[i]->body + 1;
          fprintf(ofp, "%s\"%s :", spaces, gsym[*s++]->name);
          if (*s == 0)
            fprintf(ofp, " /* empty */");
          for (; *s != 0; s++)
            fprintf(ofp, " %s", quote(gsym[*s]->name));
          if ( get_lang_id() == LANG_HSP )
            fputs(i + 1 == nprods ? "\"\n" : "\",\\\n", ofp);
          else
            fputs(i + 1 == nprods ? "\"\n" : "\",\n", ofp);
        }
      }
      else if (metamatch(p, "listvar")) {
        char *var = skipsp(skipnosp(p));
        *skipnosp(var) = '\0';
        gen_list_var(spaces, var);
      }
      else if (metamatch(p, "ifnot")) {
        p = skipsp(skipnosp(p));
        *skipnosp(p) = '\0';
        if (strcmp(p, "-a") == 0) {
          if (aflag) skipmode = YES;
        }
        else if (strcmp(p, "-t") == 0) {
          if (tflag) skipmode = YES;
        }
        else if (strcmp(p, "-p") == 0) {
          if (pspref) skipmode = YES;
        }
        else if (strcmp(p, "%union") == 0) {
          if (union_body) skipmode = YES;
        }
        else if (strcmp(p, "%pure_parser") == 0) {
          if (pure_flag) skipmode = YES;
        }
        else {
          proto_error("$ifnot: unknown switch: %s", p);
          exit(1);
        }
      }
      else if (metamatch(p, "if")) {
        p = skipsp(skipnosp(p));
        *skipnosp(p) = '\0';
        if (strcmp(p, "-a") == 0) {
          if (!aflag) skipmode = YES;
        }
        else if (strcmp(p, "-t") == 0) {
          if (!tflag) skipmode = YES;
        }
        else if (strcmp(p, "-p") == 0) {
          if (!pspref) skipmode = YES;
        }
        else if (strcmp(p, "%union") == 0) {
          if (!union_body) skipmode = YES;
        }
        else if (strcmp(p, "%pure_parser") == 0) {
          if (!pure_flag) skipmode = YES;
        }
        else {
          proto_error("$if: unknown switch: %s", p);
          exit(1);
        }
      }
      else if (metamatch(p, "endif")) {
        skipmode = NO;
      }
      else {
        proto_error("Unknown $: %s", line);
        exit(1);
      }
      line_changed = YES;
    }
    else {
      if (line_changed) {
        print_line(plineno, parserfn);
        line_changed = NO;
      }
      fputs(buf, ofp);
      if (copy_header)
        fputs(buf, hfp);
    }
  }

  if (hfp) efclose(hfp);
}


global void parser_close()
{
}
