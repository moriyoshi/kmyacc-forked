/*
 * compress - table compression
 *
 * Copyright (C) 1993, 2005  MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
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




#define new_array(a,elem)	(a = talloc(sizeof((a)[0]) * (elem)))


/*
 * Compression Scheme: row displacement scheme
 *  terminal-actions: two-pass, indexed by state
 *  nonterminal-actions: one-pass, indexed by symbol
 *
 * Generated tables:
 *
 *   yytranslate: conversion from input symbol to internal code
 *
 *   yyaction: terminal X state -> nextstate or reduce
 *   yytbase: state -> base offset in yyaction
 *   yycheck: index in yyaction -> terminal
 *   yydefault: state -> default reduce
 *
 *   yygoto: nonterminal X state -> nextstate (or reduce)
 *   yygbase: nonterminal -> base offset in yygoto
 *   yygcheck: index in yygoto -> nonterminal
 *   yygdefault: nonterminal -> default state
 *
 *   yylhs: left hand side of the production
 *   yylen: length of production
 */

global short *yytranslate;
global int yytranslatesize;

global int yyncterms;

global short *yyaction;
global int yyactionsize;
global short *yybase;
global int yybasesize;
global short *yycheck;
global short *yydefault;

global short *yygoto;
global int yygotosize;
global short *yygbase;
global short *yygcheck;
global short *yygdefault;

global short *yylhs;
global short *yylen;

#ifndef global
#define YYUNEXPECTED 32767
#define YYDEFAULT -32766
#endif /* global */


typedef struct preimage {
  int index;
  int len;
  short *classes;
} Preimage;

typedef struct aux {
  struct aux *next;
  int index;
  int gain;
  Preimage *preimage;
  short table[0];
} Aux;


global short *ctermindex;

private short *default_act;
private short *default_goto;
private short **term_action;
private short **class_action;
private short **nonterm_goto;
private short *class_of;
private int nclasses;

private short *frequency;

private Preimage **prims;
private Preimage **primof;
private int nprims;

private short *class2nd;
private int naux;


#define VACANT -32768


#define isvacant(x) ((x) == VACANT)



#if DEBUG
private void printact(int act)
{
  if (isvacant(act))
    fprintf(vfp, "  . ");
  else
    fprintf(vfp, "%4d", act);
}

private void print_table()
{
  int i, j;

  fprintf(vfp, "\nTerminal action:\n");
  fprintf(vfp, "%8.8s", "T\\S");
  for (i = 0; i < nclasses; i++)
    fprintf(vfp, "%4d", i);
  fprintf(vfp, "\n");
  for (j = 0; j < nterms; j++) {
    for (i = 0; i < nnonleafstates; i++)
      if (!isvacant(term_action[i][j]))
        break;
    if (i < nnonleafstates) {
      fprintf(vfp, "%8.8s", gsym[j]->name);
      for (i = 0; i < nclasses; i++)
        printact(class_action[i][j]);
      fprintf(vfp, "\n");
    }
  }

  fprintf(vfp, "\nNonterminal GOTO table:\n");
  fprintf(vfp, "%8.8s", "T\\S");
  for (i = 0; i < nnonleafstates; i++)
    fprintf(vfp, "%4d", i);
  fprintf(vfp, "\n");
  for (j = 0; j < nnonts; j++) {
    for (i = 0; i < nnonleafstates; i++)
      if (nonterm_goto[i][j] > 0)
        break;
    if (i < nnonleafstates) {
      fprintf(vfp, "%8.8s", gsym[NB + j]->name);
      for (i = 0; i < nnonleafstates; i++)
        printact(nonterm_goto[i][j]);
      fprintf(vfp, "\n");
    }
  }

  fprintf(vfp, "\nNonterminal GOTO table:\n");
  fprintf(vfp, "%8.8s default", "T\\S");
  for (i = 0; i < nnonleafstates; i++)
    fprintf(vfp, "%4d", i);
  fprintf(vfp, "\n");
  for (j = 0; j < nnonts; j++) {
    for (i = 0; i < nnonleafstates; i++)
      if (nonterm_goto[i][j] > 0)
        break;
    if (i < nnonleafstates) {
      fprintf(vfp, "%8.8s", gsym[NB + j]->name);
      fprintf(vfp, "%8d", default_goto[j]);
      for (i = 0; i < nnonleafstates; i++) {
        if (nonterm_goto[i][j] == default_goto[j])
          fprintf(vfp, "  = ");
        else
          printact(nonterm_goto[i][j]);
      }
      fprintf(vfp, "\n");
    }
  }

}
#endif



bool vacant_row(short *x, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    if (!isvacant(x[i]))
      return NO;
  }
  return YES;
}


bool eq_row(short *x, short *y, int n)
{
  int i;
  for (i = 0; i < n; i++) {
    if (x[i] != y[i])
      return NO;
  }
  return YES;
}



/* Best covering of classes */
int best_covering(short table[], Preimage *prim)
{
  int i, j, n, gain;

  for (i = 0; i < nstates; i++)
    frequency[i] = 0;
  gain = 0;
  for (i = 0; i < nterms; i++) {
    int max = 0;
    int maxaction = -1;
    int nvacant = 0;
    for (j = 0; j < prim->len; j++) {
      if (class2nd[prim->classes[j]] < 0) {
        int c = class_action[prim->classes[j]][i];
        if (c > 0 && ++frequency[c] > max) {
          maxaction = c;
          max = frequency[c];
        } else if (isvacant(c)) {
          nvacant++;
        }
      }
    }
    n = max - 1 - nvacant;
    if (n > 0) {
      table[i] = maxaction;
      gain += n;
    } else
      table[i] = VACANT;
  }
  return gain;
}



/* Extract common entries and shrink table size. */
void extract_common()
{
  int i, j, n;
#if DEBUG
  int f;
#endif
  Aux *alist, *p;
  Preimage *prim;

  new_array(class2nd, nclasses);
  for (i = 0; i < nclasses; i++)
    class2nd[i] = -1;

  alist = NULL;
  n = 0;
  for (i = 0; i < nprims; i++) {
    prim = prims[i];
    if (prim->len < 2)
      continue;
    p = talloc(sizeof(Aux) + (sizeof(short) * nterms));

    /* choose best covering */
    p->gain = best_covering(p->table, prim);
    if (p->gain < 1)
      continue;

    p->preimage = prim;
    p->next = alist;
    alist = p;
    n++;
  }

#if DEBUG
  if (debug) {
    fprintf(vfp, "\nCandidates of aux table:\n");
    for (p = alist; p != NULL; p = p->next) {
      fprintf(vfp, "Aux = (%d) ", p->gain);
      f = 0;
      for (j = 0; j < nterms; j++) {
        if (!isvacant(p->table[j]))
          fprintf(vfp, f++ ? ",%d" : "%d", p->table[j]);
      }
      fprintf(vfp, " * ");
      for (j = 0; j < p->preimage->len; j++)
        fprintf(vfp, j ? ",%d" : "%d", p->preimage->classes[j]);
      fprintf(vfp, "\n");
    }
  }
#endif

#if DEBUG
  if (debug) {
    fprintf(vfp, "Used aux table:\n");
  }
#endif
  naux = nclasses;
  for (;;) {
    /* Find largest gain aux. */
    int maxgain = 0;
    Aux *maxaux = NULL;
    Aux *pre = NULL;
    Aux *maxpre = NULL;
    for (p = alist; p != NULL; p = p->next) {
      if (p->gain > maxgain) {
        maxgain = p->gain;
        maxaux = p;
        maxpre = pre;
      }
      pre = p;
    }

    if (maxaux == NULL)
      break;

    if (maxpre)
      maxpre->next = maxaux->next;
    else
      alist = maxaux->next;

    maxaux->index = naux;
    for (j = 0; j < maxaux->preimage->len; j++) {
      int cl = maxaux->preimage->classes[j];
      if (eq_row(class_action[cl], maxaux->table, nterms))
        maxaux->index = cl;
    }
    if (maxaux->index >= naux)
      class_action[naux++] = maxaux->table;

    for (j = 0; j < maxaux->preimage->len; j++) {
      int cl = maxaux->preimage->classes[j];
      if (class2nd[cl] < 0)
        class2nd[cl] = maxaux->index;
    }

#if DEBUG
    if (debug) {
      fprintf(vfp, "Selected aux[%d]: (%d) ", maxaux->index, maxaux->gain);
      f = 0;
      for (j = 0; j < nterms; j++) {
        if (!isvacant(maxaux->table[j]))
          fprintf(vfp, f++ ? ",%d" : "%d", maxaux->table[j]);
      }
      fprintf(vfp, " * ");
      f = 0;
      for (j = 0; j < maxaux->preimage->len; j++) {
        int cl = maxaux->preimage->classes[j];
        if (class2nd[cl] == maxaux->index)
          fprintf(vfp, f++ ? ",%d" : "%d", cl);
      }
      fprintf(vfp, "\n");
    }
#endif

    /* recompute gain */
    for (p = alist; p != NULL; p = p->next)
      p->gain = best_covering(p->table, p->preimage);
  }

#if DEBUG
  if (debug) {
    for (i = 0; i < nnonleafstates; i++) {
      if (class2nd[class_of[i]] >= 0 && class2nd[class_of[i]] != class_of[i])
        fprintf(vfp, "state %d (class %d): aux[%d]\n", i, class_of[i], class2nd[class_of[i]]);
      else
        fprintf(vfp, "state %d (class %d)\n", i, class_of[i]);
    }
  }
#endif 
}





struct trow {
  short index;
  short mini;
  short maxi;
  short nent;
};
#define span(x) ((x)->maxi - (x)->mini)
#define nhole(x) (span(x) - (x)->nent)

int qcmp_order(const void *x0, const void *y0)
{
  const struct trow *x = x0, *y = y0;
  if (x->nent != y->nent)
    return y->nent - x->nent;
  if (span(y) != span(x))
    return (span(y) - span(x));
  return x->mini - y->mini;
}


void pack_table(short **transit, int nrows, int ncols, bool dontcare,
                bool checkrow,
                short **out_table, int *out_tablesize, short **out_check,
                short **out_base)
{
  int actpoolmax;
  short *actpool;
  short *check;
  short *base;
  int i, j, ii, poolsize;
  struct trow *trow;

  new_array(trow, nrows);
  for (i = 0; i < nrows; i++) {
    struct trow *p = &trow[i];
    p->index = i;
    p->nent = 0;
    p->mini = -1;
    p->maxi = 0;
    for (j = 0; j < ncols; j++) {
      if (!isvacant(transit[i][j])) {
        if (p->mini < 0) p->mini = j;
        p->maxi = j + 1;
        p->nent++;
      }
    }
    if (p->mini < 0) p->mini = 0;
  }

  /* allocation order */
  qsort(trow, nrows, sizeof(trow[0]), qcmp_order);

#if DEBUG
  if (debug) {
    fprintf(vfp, "Order:\n");
    for (i = 0; i < nrows; i++)
      fprintf(vfp, "%d,", trow[i].index);
    fprintf(vfp, "\n");
  }
#endif

  /* pack */
  poolsize = nrows * ncols;
  new_array(actpool, poolsize);
  new_array(check, poolsize);
  new_array(base, nrows);
  actpoolmax = 0;

  for (i = 0; i < poolsize; i++) {
    actpool[i] = 0;
    check[i] = -1;
  }
  for (ii = 0; ii < nrows; ii++) {
    int k, jj, h;
    i = trow[ii].index;
    /* reserve 0 for empty row. */
    if (vacant_row(transit[i], ncols)) {
      base[i] = 0;
      goto ok;
    }
    for (h = 0; h < ii; h++) {
      if (eq_row(transit[trow[h].index], transit[i], ncols)) {
        base[i] = base[trow[h].index];
        goto ok;
      }
    }
    for (j = 0; j < poolsize; j++) {
      jj = j;
      base[i] = j - trow[ii].mini;
      if (!dontcare) {
        if (base[i] == 0)
          continue;
        for (h = 0; h < ii; h++)
          if (base[trow[h].index] == base[i])
            goto next;
      }

      for (k = trow[ii].mini; k < trow[ii].maxi; k++) {
        if (!isvacant(transit[i][k])) {
          if (jj >= poolsize)
            die("can't happen");
          if (check[jj] >= 0 && !(dontcare && actpool[jj] == transit[i][k]))
            goto next;
        }
        jj++;
      }
      break;
    next:;
    }
    jj = j;
    for (k = trow[ii].mini; k < trow[ii].maxi; k++) {
      if (!isvacant(transit[i][k])) {
        actpool[jj] = transit[i][k];
        check[jj] = checkrow ? i : k;
      }
      jj++;
    }
    if (jj >= actpoolmax)
      actpoolmax = jj;
  ok:;
  }

#if 0
  /* verify */
  for (i = 0; i < nrows; i++) {
    for (j = 0; j < ncols; j++) {
      if (dontcare && isvacant(transit[i][j]))
        continue;
      if ((unsigned)(base[i] + j) < actpoolmax
          && check[base[i] + j] == (checkrow ? i : j))
        assert(actpool[base[i] + j] == transit[i][j]);
      else
        assert(isvacant(transit[i][j]));
    }
  }
#endif

  *out_table = actpool;
  *out_tablesize = actpoolmax;
  *out_check = check;
  *out_base = base;
}



void encode_shift_reduce(short *t, int n)
{
  int i;

  for (i = 0; i < n; i++) {
    if (t[i] >= nnonleafstates)
      t[i] = nnonleafstates + default_act[t[i]];
  }
}



void authodox_table()
{
  int i, j, ncterms;
  short **cterm_action;
  short *otermindex;
  short **nonterm_transposed;
  short *base;

  new_array(ctermindex, nterms);
  new_array(otermindex, nterms);

  ncterms = 0;
  for (j = 0; j < nterms; j++) {
    ctermindex[j] = -1;
    if (j == error_token) {
      ctermindex[j] = ncterms;
      otermindex[ncterms++] = j;
      continue;
    }
    for (i = 0; i < nnonleafstates; i++) {
      if (term_action[i][j] != VACANT) {
        ctermindex[j] = ncterms;
        otermindex[ncterms++] = j;
        break;
      }
    }
  }

  new_array(cterm_action, naux);
  for (i = 0; i < nclasses; i++) {
    new_array(cterm_action[i], ncterms);
    for (j = 0; j < ncterms; j++)
      cterm_action[i][j] = class_action[i][otermindex[j]];
  }

  /* remove common entries */
  for (i = 0; i < nclasses; i++) {
    if (class2nd[i] >= 0 && class2nd[i] != i) {
      short *table = class_action[class2nd[i]];
      for (j = 0; j < ncterms; j++) {
        if (!isvacant(table[otermindex[j]])) {
          if (cterm_action[i][j] == table[otermindex[j]])
            cterm_action[i][j] = VACANT;
          else if (cterm_action[i][j] == VACANT)
            cterm_action[i][j] = YYDEFAULT;
        }
      }
    }
  }
  for (i = nclasses; i < naux; i++) {
    new_array(cterm_action[i], ncterms);
    for (j = 0; j < ncterms; j++)
      cterm_action[i][j] = class_action[i][otermindex[j]];
  }
  pack_table(cterm_action, naux, ncterms, NO, NO,
             &yyaction, &yyactionsize, &yycheck, &base);
  yydefault = default_act;

  new_array(yybase, nnonleafstates * 2);
  yybasesize = nnonleafstates;
  for (i = 0; i < nnonleafstates; i++) {
    int cl = class_of[i];
    yybase[i] = base[cl];
    if (class2nd[cl] >= 0 && class2nd[cl] != cl) {
      yybase[i + nnonleafstates] = base[class2nd[cl]];
      if (i + nnonleafstates + 1 > yybasesize)
        yybasesize = i + nnonleafstates + 1;
    }
  }

#if 0
  /* check two pass table */
  for (i = 0; i < nclasses; i++) {
    for (j = 0; j < nterms; j++) {
      int jj = ctermindex[j];
      int x, k;
      if ((unsigned)(k = base[i] + jj) < yyactionsize && yycheck[k] == jj)
        x = yyaction[k];
      else if (class2nd[i] >= 0 && (unsigned)(k = base[class2nd[i]] + jj) < yyactionsize && yycheck[k] == jj)
        x = yyaction[k];
      else
        x = VACANT;
      if (x == YYDEFAULT) x = VACANT;
      assert(class_action[i][j] == x);
#if 0
      if (class_action[i][j] != x)
        fprintf(vfp, "class:%d, symbol:%d(%s): %d != %d\n",
                i, j, gsym[j]->name, class_action[i][j], x);
#endif
    }
  }
#endif


  /* generate goto table */
  new_array(nonterm_transposed, nnonts);
  for (j = 0; j < nnonts; j++) {
    new_array(nonterm_transposed[j], nnonleafstates);
    for (i = 0; i < nnonleafstates; i++) {
      nonterm_transposed[j][i] = nonterm_goto[i][j];
      if (nonterm_goto[i][j] == default_goto[j])
        nonterm_transposed[j][i] = VACANT;
    }
  }
  pack_table(nonterm_transposed, nnonts, nnonleafstates, NO, YES,
             &yygoto, &yygotosize, &yygcheck, &yygbase);

  yygdefault = default_goto;

  /* Set yylhs, yylen */
  new_array(yylhs, nprods);
  new_array(yylen, nprods);
  for (i = 0; i < nprods; i++) {
    yylhs[i] = gram[i]->body[1] - NB;
    yylen[i] = length(gram[i]->body + 1);
  }

  /* Set yytranslate */
  yytranslatesize = 0;
  for (i = 0; i < nterms; i++) {
    if (gsym[i]->value + 1 > yytranslatesize)
      yytranslatesize = gsym[i]->value + 1;
  }
  yyncterms = ncterms;
  new_array(yytranslate, yytranslatesize);
  for (i = 0; i < yytranslatesize; i++)
    yytranslate[i] = ncterms;
  for (i = 0; i < nterms; i++) {
    if (ctermindex[i] >= 0)
      yytranslate[gsym[i]->value] = ctermindex[i];
#if 0
    else
      gsym[i]->value = -1;
#endif
  }

  encode_shift_reduce(yyaction, yyactionsize);
  encode_shift_reduce(yygoto, yygotosize);
  encode_shift_reduce(yygdefault, nnonts);
}


private int cmp_preimage(const Preimage *x, const Preimage *y)
{
  int i;
  if (x->len != y->len)
    return x->len - y->len;
  for (i = 0; i < x->len; i++) {
    if (x->classes[i] != y->classes[i])
      return x->classes[i] - y->classes[i];
  }
  return 0;
}


private int qcmp_preimage(const void *x, const void *y)
{
  return cmp_preimage(*(Preimage *const *)x, *(Preimage *const *)y);
}

/* Compute preimages for each state */
void comp_preimages()
{
  int i, j;
  Preimage **primv;

  new_array(primv, nstates);
  for (i = 0; i < nstates; i++) {
    primv[i] = talloc(sizeof(Preimage));
    primv[i]->index = i;
    primv[i]->len = 0;
  }

  for (i = 0; i < nclasses; i++) {
    for (j = 0; j < nterms; j++) {
      int s = class_action[i][j];
      if (s > 0)
        primv[s]->len++;
    }
  }
  for (i = 0; i < nstates; i++) {
    new_array(primv[i]->classes, primv[i]->len);
    primv[i]->len = 0;
  }
  for (i = 0; i < nclasses; i++) {
    for (j = 0; j < nterms; j++) {
      int s = class_action[i][j];
      if (s > 0)
        primv[s]->classes[primv[s]->len++] = i;
    }
  }

  qsort(primv, nstates, sizeof(primv[0]), qcmp_preimage);

  new_array(primof, nstates);
  new_array(prims, nstates);
  nprims = 0;
  for (i = 0; i < nstates; ) {
    struct preimage *p = primv[i];
    prims[nprims] = p;
    for (; i < nstates && cmp_preimage(p, primv[i]) == 0; i++)
      primof[primv[i]->index] = p;
    p->index = nprims++;
  }
}




private int cmp_states(int x, int y)
{
  int i;
  for (i = 0; i < nterms; i++) {
    if (term_action[x][i] != term_action[y][i])
      return term_action[x][i] - term_action[y][i];
  }
  return 0;
}

private int qcmp_states(const void *x, const void *y)
{
  return cmp_states(*(const short *)x, *(const short *)y);
}


private int encode_rederr(int code)
{
  return code < 0 ? YYUNEXPECTED : code;
}


/* Convert symbol to compressed code. */
global int convert_sym(int sym)
{
  return isterm(sym) ? ctermindex[sym] : sym;
}


global void makeup_table2()
{
  int i, j, k;
  short *state_imagesorted;
  Reduce *p;

  new_array(term_action, nnonleafstates);
  new_array(class_action, nnonleafstates * 2);
  new_array(nonterm_goto, nnonleafstates);
  new_array(default_act, nstates);
  new_array(default_goto, nnonts);

  new_array(frequency, nstates);
  new_array(state_imagesorted, nnonleafstates);
  new_array(class_of, nstates);

  for (i = 0; i < nnonleafstates; i++) {
    State **t;

    new_array(term_action[i], nterms);
    new_array(nonterm_goto[i], nnonts);
    for (j = 0; j < nterms; j++)
      term_action[i][j] = VACANT;
    for (j = 0; j < nnonts; j++)
      nonterm_goto[i][j] = VACANT;

    for (t = statev[i]->shifts; *t; t++) {
      if (isterm((*t)->thru))
        term_action[i][(*t)->thru] = (*t)->number;
      else
        nonterm_goto[i][(*t)->thru - NB] = (*t)->number;
    }
    for (p = statev[i]->reduce; p->sym != NILSYM; p++)
      term_action[i][p->sym] = -encode_rederr(p->num);

    state_imagesorted[i] = i;
  }

  for (i = 0; i < nstates; i++) {
    for (p = statev[i]->reduce; p->sym != NILSYM; p++)
      ;
    default_act[i] = encode_rederr(p->num);
  }

  for (j = 0; j < nnonts; j++) {
    int max = 0;
    int maxst = VACANT;
    for (i = 0; i < nstates; i++)
      frequency[i] = 0;
    for (i = 0; i < nnonleafstates; i++) {
      int st = nonterm_goto[i][j];
      if (st > 0) {
        frequency[st]++;
        if (frequency[st] > max) {
          max = frequency[st];
          maxst = st;
        }
      }
    }
    default_goto[j] = maxst;
  }

  qsort(state_imagesorted, nnonleafstates, sizeof(state_imagesorted[0]), qcmp_states);
  j = 0;
  for (i = 0; i < nnonleafstates; ) {
    k = state_imagesorted[i];
    class_action[j] = term_action[k];
    for (; i < nnonleafstates && cmp_states(state_imagesorted[i], k) == 0; i++)
      class_of[state_imagesorted[i]] = j;
    j++;
  }
  nclasses = j;

#if DEBUG
  if (debug) {
    fprintf(vfp, "\nState=>class:\n");
    for (i = 0; i < nnonleafstates; i++) {
      if (i % 10 == 0)
        fprintf(vfp, "\n");
      fprintf(vfp, "%3d=>%-3d ", i, class_of[i]);
    }
    fprintf(vfp, "\n");
  }
#endif

  comp_preimages();

#if DEBUG
  if (debug) print_table();
#endif
  extract_common();

  authodox_table();

}
