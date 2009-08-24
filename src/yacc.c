/*
 * yacc.c - LALR parser generator
 *
 * Copyright (C) 1987,1989,1992,1993,2005  MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if HAS_STDLIB
# include <stdlib.h>
#else
char *getenv();
#endif /* HAS_STDLIB */

#include "common.h"
#include "misc.h"
#include "token.h"

#include "grammar.h"
#include "lalr.h"
#include "genparser.h"
#include "yacc.h"




global bool lflag;
global bool tflag;
global bool debug;
global bool aflag;
global bool nflag;
global bool iflag;

global FILE *ifp, *ofp, *vfp, *hfp;
global int lineno
= 1;
global char *filename;
global char *outfilename;

global int expected_srconf;

global char *pspref;



static char *progname = "kmyacc";

#ifdef MSDOS
# define MAXPATHLEN 90
#else
# define MAXPATHLEN 200
#endif /* MSDOS */


/* Print usage of this program */
void usage()
{
  fprintf(stderr, "KMyacc - parser generator ver 4.1.4\n");
  fprintf(stderr, "Copyright (C) 1987-1989,1992-1993,2005,2006  MORI Koichiro\n\n");
  fprintf(stderr, "Usage: %s [-dvltani] [-b y] [-p yy] [-m model] [-L lang] grammar.y\n", progname);
}









/* Open file fn with mode; exit if fail */
global FILE *efopen(char *fn, char *mode)
{
  FILE *fp;

  if (strcmp(fn, "-") == 0) {
    switch (*mode) {
    case 'r': return stdin;
    case 'w': return stdout;
    }
  }
  if ((fp = fopen(fn, mode)) == NULL) {
    fprintf(stderr, "%s: ", progname);
    perror(fn);
    exit(1);
  }
  return (fp);
}


/* Close file fp and exit if error */
global void efclose(FILE *fp)
{
  if (ferror(fp) || fclose(fp)) {
    fprintf(stderr, "%s: can't close\n", progname);
    exit(1);
  }
}




#ifdef MSDOS
# define OUT_SUFFIX ".out"
#else
# define OUT_SUFFIX ".output"
# define remove unlink
#endif /* MSDOS */


/* Entry point */
int main(int argc, char *argv[])
{
  char fn[MAXPATHLEN];
  char *parserfn;
  char *p;

  char *langname = NULL;
  bool vflag = NO;
  bool dflag = NO;
  char *fnpref = NULL;

  parserfn = getenv("KMYACCPAR");

#ifndef MSDOS
  progname = *argv;
#endif /* !MSDOS */

  while (++argv, --argc != 0 && argv[0][0] == '-') {
    for (p = argv[0] + 1; *p; ) {
      switch (*p++) {
      case 'd':
        dflag = YES;
        break;
      case 'x':
        debug = YES;
        /* fall thru */
      case 'v':
        vflag = YES;
        break;
      case 'l':
        lflag = YES;
        break;
      case 't':
        tflag = YES;
        break;
      case 'i':
        iflag = YES;
        /* fall thru */
      case 'n':
        nflag = YES;
        break;
      case 'L':
        if (*p != '\0') {
          langname = p;
          p = "";
        } else {
          if (--argc <= 0)
            goto boo;
          langname = *++argv;
        }
        break;
      case 'b':
        if (*p != '\0') {
          fnpref = p;
          p = "";
        } else {
          if (--argc <= 0)
            goto boo;
          fnpref = *++argv;
        }
        break;
      case 'p':
        if (*p != '\0') {
          pspref = p;
          p = "";
        } else {
          if (--argc <= 0)
            goto boo;
          pspref = *++argv;
        }
        break;
      case 'm':
        if (*p != '\0') {
          parserfn = p;
          p = "";
        } else {
          if (--argc <= 0)
            goto boo;
          parserfn = *++argv;
        }
        break;
      case 'a':
        aflag = YES;
        break;
      default:
      boo:
        usage();
        exit(1);
      }
    }
  }
  if (argc != 1) {
    usage();
    exit(1);
  }

  filename = argv[0];

  if (langname)
    parser_set_language(langname);
  else
    parser_set_language_by_yaccext(extension(filename));

  ifp = efopen(filename, "r");
  outfilename = parser_outfilename(fnpref, filename); 
  ofp = efopen(outfilename, "w");
  if (dflag) {
    char *header = parser_header_filename(fnpref, filename);
    if (header != NULL)
      hfp = efopen(header, "w");
  }
  if (parserfn == NULL)
    parserfn = parser_modelfilename(PARSERBASE);

  /* Initialize parser generator */
  parser_create(parserfn, tflag);

  /* Read declaration section */
  do_declaration();

  /* read grammar */
  do_grammar();

#if 0
  fprintf(stderr, "grammar read. ");
  show_mem_usage();
#endif

  if (worst_error == 0) {

    if (vflag)
      vfp = efopen(strcat(strcpy(fn, fnpref ? fnpref : "y"), OUT_SUFFIX), "w");

    /* compute LALR(1) states & actions */
    comp_lalr();

    /* generate parser */
    parser_generate();

#if 0
    fprintf(stderr, "gentable done. ");
    show_mem_usage();
#endif

    if (vfp) efclose(vfp);

  }

  parser_close();
  efclose(ofp);

  exit(worst_error);
}
