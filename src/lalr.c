/*
 * LALR(1) parser generation
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
#include "yacc.h"
#include "misc.h"
#include "lalr.h"



#ifndef global

#include "grammar.h"

typedef Gsym *Item;

typedef struct {
  Gsym sym;
  short num;
} Reduce;

typedef struct lr1 LR1;

typedef struct conflict {
  struct conflict *next;
  bool shift_reduce;
  Gsym symbol;
  union {
    struct {
      struct state *shift;
      int reduce;
    } sr;
    struct {
      int reduce1;
      int reduce2;
    } rr;
  } u;
} Conflict;


typedef struct state {
  struct state *next;
  int number;
  LR1 *items;
  struct state **shifts;
  Reduce *reduce;
  Conflict *conf;
  Gsym thru;
} State;


#endif /* global */

struct lr1 {
  struct lr1 *next;
  Gsym left;
  Item item;
  uchar *look;
};


global int nstates;
global int nnonleafstates;

global State **statev;

private State *states;


private int nlooks;
private int nacts, nacts2;
private int nitems;

private int nsrerr = 0;
private int nrrerr = 0;

#if MAXSYM > MAXPROD
private bool visited[MAXSYM];
#else
private bool visited[MAXPROD];
#endif


/* List of states indexed by its label (thru) */
private List *states_thru[MAXSYM + 1];


/* Set-operation functions */
#define NBITS 8 /* number of bits in a byte */

#define tbit(a, i) (((a)[(i) / NBITS] >> ((i) % NBITS)) & 1)
#define sbit(a, i) ((a)[(i) / NBITS] |= 1 << ((i) % NBITS))
#define cbit(a, i) ((a)[(i) / NBITS] &= ~(1 << ((i) % NBITS)))

private bool *nullable;
private uchar (*first)[(MAXTERM + NBITS - 1) / NBITS];
private uchar (*follow)[(MAXTERM + NBITS - 1) / NBITS];



/* Let set d = set d OR set s */
bool orbits(uchar *d, uchar *s)
{
  bool changed;
  int i;
    
  changed = NO;
  for (i = 0; i < nterms; i += NBITS) {
    if (*s & ~*d) {
      changed = YES;
      *d |= *s;
    }
    s++;
    d++;
  }
  return (changed);
}


#define for_each_member(v, set) \
    for (v = 0; (v = next_elem(v, set)) < nterms; v++)

/* Return an element next to e in set p */
Gsym next_elem(Gsym e, uchar *p)
{
  for (; e < nterms; e++) {
    if (tbit(p, e))
      break;
  }
  return (e);
}


/* Dump set */
void dump_set(uchar *set)
{
  Gsym x;
    
  for (x = 0; x < nterms; x++) {
    if (tbit(set, x))
      fprintf(vfp, "%s ", gsym[x]->name);
  }
}


/* Return production number of Item p */
int item2pnum(Gsym *p)
{
  int i;
    
  while (*--p != 0)
    ;
  for (i = 0; i < nprods; i++) {
    if (gram[i]->body == p)
      return (i);
  }
  die("item2pnum(): can't happen");
}


/* Print Item s */
void write_item(Item s)
{
  Item p;
    
  p = s;
  while (*--s != 0)
    ;
  s++;
  fprintf(vfp, "\t(%d) %s :", item2pnum(p), gsym[*s]->name);
  for (s++; ;s++) {
    if (s == p)
      fputs(" .", vfp);
    if (*s == '\0')
      break;
    fprintf(vfp, " %s", gsym[*s]->name);
  }
  fprintf(vfp, "\n");
}




/* Precompute FIRST & NULLABLE for each nonterminals */

    
void first_nullable_precomp()
{
  int i;
  Gsym g, h, *s;
  bool changed;

  nullable = talloc(sizeof(bool) * nnonts);
  first = talloc(sizeof(first[0]) * nnonts);
  do {
    changed = NO;
    for (i = 0; i < nprods; i++) {
      h = gram[i]->body[1];
      for (s = gram[i]->body + 2; (g = *s++) != 0; ) {
        if (isterm(g)) {
          if (!tbit(first[h - NB], g))
            changed = YES;
          sbit(first[h - NB], g);
          break;
        }
        changed |= orbits(first[h - NB], first[g - NB]);
        if (!nullable[g - NB])
          break;
      }
      if (g == 0 && !nullable[h - NB]) {
        nullable[h - NB] = YES;
        changed = YES;
      }
    }
  } while (changed);

  if (debug) {
    fprintf(vfp, "First:\n");
    for (i = 0; i < nnonts; i++) {
      fprintf(vfp, "%s\t[ ", gsym[NB + i]->name);
      dump_set(first[i]);
      if (nullable[i])
        fprintf(vfp, "@ ");
      fprintf(vfp, "]\n");
    }
  }
}



/* Precompute nonterminals that reaches A -> empty */

void comp_empty()
{
  int i;
  bool changed;

  do {
    changed = 0;
    for (i = 0; i < nprods; i++) {
      Gsym lhs = gram[i]->body[1];
      Gsym rhs = gram[i]->body[2];
      if ((rhs == 0 || gsym[rhs]->assoc & F_EMPTY)
          && !(gsym[lhs]->assoc & F_EMPTY)) {
        gsym[lhs]->assoc |= F_EMPTY;
        changed = 1;
      }
    }
  } while (changed);

  if (debug) {
    fprintf(vfp, "EMPTY nonterminals: ");
    for (i = 0; i < nnonts; i++) {
      if (gsym[NB+i]->assoc & F_EMPTY)
        fprintf(vfp, " %s", gsym[NB+i]->name);
    }
    fprintf(vfp, "\n");
  }
}



/*  compute LALR(1) sets of items  */


private int bytes_LA;

#define headitem(item) ((item)[-2] == 0)
#define tailitem(item) ((item)[0] == 0)

/* Return non-zero if item set p and t are same */
bool sameset(LR1 *p, LR1 *t)
{
  while (t != NULL) {
    if (p == NULL || p->item != t->item)
      return NO;
    p = p->next;
    t = t->next;
  }
  return p == NULL || headitem(p->item);
}


private LR1 *freelist = NULL;

/* Free unnecessary item objects. */
void free_items(LR1 *list)
{
  LR1 *p;

  if (list == NULL)
    return;

  for (p = list; p->next != NULL; p = p->next)
    ;
  p->next = freelist;
  freelist = list;
}


/* Return new LR(1) item object. */
LR1 *alloc_item()
{
  LR1 *p = freelist;
  if (p != NULL) {
    freelist = p->next;
#if 0
    p->next = NULL;
    p->item = NULL;
    p->left = 0;
    p->look = NULL;
#endif
    return p;
  }
  nitems++;
  return talloc(sizeof(LR1));
}




/* find and apppend empty-productions derived from nonterminal x */
LR1 *find_empty(LR1 *tail, Gsym x)
{
  LR1 *p;
  int i;

  if (!visited[x] && gsym[x]->assoc & F_EMPTY) {
    visited[x] = 1;
    for (i = gsym[x]->value; i >= 0; i = gram[i]->link) {
      if (gram[i]->body[2] == 0) {
        p = alloc_item();
        tail->next = p;
        tail = p;
        p->next = NULL;
        p->left = 0;
        p->item = gram[i]->body + 2;
        p->look = talloc(bytes_LA);
        nlooks++;
      } else if (!isterm(gram[i]->body[2]))
        tail = find_empty(tail, gram[i]->body[2]);
    }
  }
  return tail;
}


/* Return LR(1) set of items */
LR1 *make_state(LR1 *items)
{
  LR1 *p, *q, *tail;
  Gsym g;

  /* Add lookahead set buffer to LR(0) items. */
  tail = NULL;
  for (p = items; p != NULL; p = p->next) {
    p->look = NULL;
    if (p->left != 0) {
      for (q = items; q != p; q = q->next) {
        if (q->left == p->left) {
          p->look = q->look; /* same left, share lookahead */
          break;
        }
      }
    }
    if (p->look == NULL) {
      p->look = talloc(bytes_LA);
      nlooks++;
    }
    tail = p;
  }

  /* find empty productions */
  memset(visited, 0, sizeof(bool) * MAXSYM);
  for (p = items; p != NULL; p = p->next) {
    g = *p->item;
    if (!isterm(g))
      tail = find_empty(tail, g);
  }

  return items;
}



int cmp_LR1(LR1 *x, LR1 *y)
{
  int w;
    
  if ((w = (int)x->item[-1] - (int)y->item[-1]) != 0)
    return (w);
  return (x->item - y->item);
}





/* Compare states */
int cmp_state(State *p, State *q)
{
  State *x;
  int pn, pt, qn, qt, i;

  pn = pt = 0;
  for (pn = 0; (x = p->shifts[pn]) != NULL; pn++) {
    if (isterm(x->thru))
      pt++;
  }
  for (i = 0; p->reduce[i].sym != NILSYM; i++) {
    pt++;
    pn++;
  }
  qn = qt = 0;
  for (qn = 0; (x = q->shifts[qn]) != NULL; qn++) {
    if (isterm(x->thru))
      qt++;
  }
  for (i = 0; q->reduce[i].sym != NILSYM; i++) {
    qt++;
    qn++;
  }
  if (pt != qt)
    return qt - pt;
  return qn - pn;
}



void link_state(State *s, Gsym g)
{
  List *chain = alloc(sizeof(List));
  chain->next = states_thru[g];
  chain->elem = s;
  states_thru[g] = chain;
}




/* Make LR(0) items (kernel only) */
void comp_kernels()
{
  LR1 *x;
  Gsym g;
  int k, ni;
  LR1 *tmplist, *tmptail, *tp;

  State *nextst[MAXTERM + MAXNONT];
  State *tail, *p, *q;

  bytes_LA = (nterms + NBITS - 1) / NBITS;

  tmplist = alloc_item();
  tmplist->next = NULL;
  tmplist->left = 0;
  tmplist->item = gram[0]->body + 2;

  memset(states_thru, 0, sizeof(states_thru));

  states = alloc(sizeof(State));
  states->next = NULL;
  states->items = make_state(tmplist);
  states->thru = NILSYM;
  link_state(states, NILSYM);
  tail = states;
  nstates = 1;

  for (p = states; p != NULL; p = p->next) {
    /* collect direct GOTO's (come from kernel items) */
    tmplist = tmptail = NULL;
    for (x = p->items; x != NULL; x = x->next) {
      if (!tailitem(x->item)) {
        LR1 *wp = alloc_item();
        wp->left = 0;
        wp->item = x->item + 1;
        wp->next = NULL;
        if (tmptail)
          tmptail->next = wp;
        else
          tmplist = wp;
        tmptail = wp;
      }
    }

    /* collect indirect GOTO's (come from nonkernel items) */
    memset(visited, 0, sizeof(bool) * MAXSYM);
    for (tp = tmplist; tp != NULL; tp = tp->next) {
      g = tp->item[-1];
      if (!isterm(g) && !visited[g]) {
        visited[g] = YES;
        for (k = gsym[g]->value; k >= 0; k = gram[k]->link) {
          if (gram[k]->body[2] != 0) {
            LR1 *wp = alloc_item();
            wp->left = g;
            wp->item = gram[k]->body + 3;
            wp->next = NULL;
            tmptail->next = wp;
            tmptail = wp;
          }
        }
      }
    }

    tmplist = (LR1 *)sortlist((List *)tmplist, cmp_LR1);

    /* compute next states & fill nextst[] */
    ni = 0;
    for (tp = tmplist; tp != NULL; ) {
      LR1 *sp = NULL, *sublist;
      List *lp;

      g = tp->item[-1];
      /* separate sublist */
      sublist = tp;
      while (tp != NULL && tp->item[-1] == g) {
        sp = tp;
        tp = tp->next;
      }
      sp->next = NULL;

      for (lp = states_thru[g]; lp != NULL; lp = lp->next) {
        if (sameset(((State *)lp->elem)->items, sublist))
          break;
      }
      if (lp != NULL) {
        free_items(sublist);
        q = lp->elem;
      } else {
        /* brand new state */
        q = alloc(sizeof(State));
        tail->next = q;
        tail = q;
        q->next = NULL;
        q->items = make_state(sublist);
        q->thru = g;
        link_state(q, g);
        nstates++;
      }
      if (ni >= height(nextst))
        die("Too many actions in one state");
      nextst[ni++] = q;
    }
    p->shifts = alloc(sizeof(State *) * (ni + 1));
    memcpy(p->shifts, nextst, sizeof(State *) * ni);
    p->shifts[ni] = NULL;

    nacts += ni;
  }
}




/* Compute FIRST set of sequence s into p */
void comp_first(uchar *p, Gsym *s)
{
  Gsym g;
    
  while ((g = *s++) != 0) {
    if (isterm(g)) {
      sbit(p, g);
      return;
    }
    orbits(p, first[g - NB]);
    if (!nullable[g - NB])
      return;
  }
}


/* Return non-zero if grammar sequence s could be empty */
bool seq_nullable(Gsym *s)
{
  Gsym g;
    
  while ((g = *s++) != 0) {
    if (isterm(g) || !nullable[g - NB])
      return (NO);
  }
  return (YES);
}




/* Compute FOLLOW set into follow */
void comp_follow(State *st)
{
  uchar *p;
  State **t;
  LR1 *x;
  bool changed;
    
  for (t = st->shifts; *t; t++) {
    if (!isterm((*t)->thru)) {
      memset(p = follow[(*t)->thru - NB], 0, bytes_LA);
      for (x = (*t)->items; x && !headitem(x->item); x = x->next)
        comp_first(p, x->item);
    }
  }
  for (x = st->items; x; x = x->next) {
    if (!isterm(*x->item) && seq_nullable(x->item + 1))
      orbits(follow[*x->item - NB], x->look);
  }
  do {
    changed = NO;
    for (t = st->shifts; *t; t++) {
      if (!isterm((*t)->thru)) {
        p = follow[(*t)->thru - NB];
        for (x = (*t)->items; x != NULL && !headitem(x->item); x = x->next) {
          if (seq_nullable(x->item) && x->left != 0)
            changed |= orbits(p, follow[x->left - NB]);
        }
      }
    }
  } while (changed);
}



/* Compute lookahead for each state */
void comp_lookaheads()
{
  LR1 *x, *y;
  Gsym g, *s;
  bool changed;
  State *p, *t;
  int j;

  follow = talloc(sizeof(follow[0]) * nnonts);
  sbit(states->items[0].look, 0);
  do {
    changed = NO;
    for (p = states; p != NULL; p = p->next) {
      comp_follow(p);
      for (x = p->items; x != NULL; x = x->next) {
        if ((g = *(s = x->item)) != 0) {
          s++;
          for (j  = 0; (t = p->shifts[j]) != NULL && t->thru != g; j++)
            ;
          assert(t->thru == g);
          for (y = t->items; y != NULL && y->item != s; y = y->next)
            ;
          assert(y->item == s);
          changed |= orbits(y->look, x->look);
        }
      }
      for (j  = 0; (t = p->shifts[j]) != NULL; j++) {
        for (x = t->items; x != NULL; x = x->next) {
          if (x->left != 0)
            changed |= orbits(x->look, follow[x->left - NB]);
        }
      }
      for (x = p->items; x != NULL; x = x->next) {
        if (tailitem(x->item) && headitem(x->item))
          orbits(x->look, follow[x->item[-1] - NB]);
      }
    }
  } while (changed);
    
  if (debug) {
    for (p = states; p != NULL; p = p->next) {
      fprintf(vfp, "state unknown:\n");
      for (x = p->items; x != NULL; x = x->next) {
        write_item(x->item);
        fprintf(vfp, "\t\t[ ");
        dump_set(x->look);
        fprintf(vfp, "]\n");
      }
    }
  }
}



/* Compare precedence of production (pnum) and grammar symbol x */
#define NON_ASSOC -32768
int cmpprec(int pnum, Gsym x)
{
  int v;
    
  if (gram[pnum]->assoc == A_UNDEF || (gsym[x]->assoc & A_MASK) == A_UNDEF)
    return (0);
  if ((v = (int)gsym[x]->prec - (int)gram[pnum]->prec) != 0)
    return (v);
  switch (gram[pnum]->assoc) {
  case A_LEFT:	/* Left associative, reduce */
    return (-1);
  case A_RIGHT:	/* Right associative, shift */
    return (1);
  case A_NON:		/* Non-associative, error */
    return (NON_ASSOC);
  }
}


/* Return true if st is reduce-only state. */
bool isreduceonly(State *p)
{
  return (p->shifts[0] == NULL && p->reduce[0].sym == NILSYM);
}


/* Print transition table for state (st) */
void print_state(int st)
{
  LR1 *x;
  char *str;
  State **s;
  Reduce *r;
  Conflict *conf;

  if (!vfp)
    return;
  fprintf(vfp, "state %d\n", st);
  for (conf = statev[st]->conf; conf != NULL; conf = conf->next) {
    if (conf->shift_reduce) {
      fprintf(vfp,
              "%d: shift/reduce conflict (shift %d, reduce %d) on %s\n",
              st, conf->u.sr.shift->number, conf->u.sr.reduce,
              gsym[conf->symbol]->name);
    } else {
      fprintf(vfp,
              "%d: reduce/reduce conflict (reduce %d, reduce %d) on %s\n",
              st, conf->u.rr.reduce1, conf->u.rr.reduce2,
              gsym[conf->symbol]->name);
    }
  }
            
  for (x = statev[st]->items; x != NULL; x = x->next)
    write_item(x->item);
  putc('\n', vfp);
  for (s = statev[st]->shifts, r = statev[st]->reduce; ; ) {
    if ((*s) != NULL && (*s)->thru < r->sym) {
      str = gsym[(*s)->thru]->name;
      fprintf(vfp, strlen(str) < 8 ? "\t%s\t\t" : "\t%s\t", str);
      fprintf(vfp, "%s %d", isterm((*s)->thru) ? "shift" : "goto", (*s)->number);
      if (isreduceonly(*s))
        fprintf(vfp, " and reduce (%d)", (*s)->reduce[0].num);
      fprintf(vfp, "\n");
      s++;
    } else {
      str = r->sym == NILSYM ? "." : gsym[r->sym]->name;
      fprintf(vfp, strlen(str) < 8 ? "\t%s\t\t" : "\t%s\t", str);
      if (r->num == 0)
        fprintf(vfp, "accept\n");
      else if (r->num < 0)
        fprintf(vfp, "error\n");
      else
        fprintf(vfp, "reduce (%d)\n", r->num);
      if (*s == 0 && r->sym == NILSYM)
        break;
      r++;
    }
  }
  putc('\n', vfp);
}



int cmp_by_num(const void *x0, const void *y0)
{
  const Reduce *x = x0, *y = y0;
  if (x->num != y->num)
    return (y->num - x->num);
  return (x->sym - y->sym);
}


int cmp_by_sym(const void *x0, const void *y0)
{
  const Reduce *x = x0, *y = y0;
  if (x->sym != y->sym)
    return (x->sym - y->sym);
  return (x->num - y->num);
}


private char rubout[] = "hoge";

#define RUBOUT ((State *)rubout)

/* Fill reduce entries for each state */
void fill_reduce()
{
  int i, j, k, m;
  LR1 *x;
  Reduce tmpr[MAXTERM + MAXNONT];
  int nr;
  int tdefact, pnum;
  int rel;
  int maxn;
  Gsym e;
  uchar *alook = talloc(bytes_LA);
  State *p, *t;
  State **r, **q;
  Conflict *conf;

  memset(visited, 0, sizeof(bool) * MAXPROD);
  for (p = states; p != NULL; p = p->next) {
    tdefact = 0;
    for (j = 0; (t = p->shifts[j]) != NULL; j++) {
      if (t->thru == 1) /* shifting error */
        tdefact = -1;
    }

    /* Pick up reduce entries */
    nr = 0;
    for (x = p->items; x != NULL; x = x->next) if (tailitem(x->item)) {
      memcpy(alook, x->look, bytes_LA);

      pnum = item2pnum(x->item);
      /* find shift/reduce conflict */
      for (m = 0; (t = p->shifts[m]) != NULL; m++) {
        if (t == RUBOUT) continue;
        e = t->thru;
        if (!isterm(e)) break;
        if (tbit(alook, e)) {
          rel = cmpprec(pnum, e);
          if (rel == NON_ASSOC) {
            cbit(alook, e);
            p->shifts[m] = RUBOUT;
            if (nr >= height(tmpr))
              die("Too many actions in one state");
            tmpr[nr].sym = e;
            tmpr[nr++].num = -1;
#if 0
            if (vfp) fprintf(vfp, "%d: error overrides on %s\n", i, gsym[e]->name);
#endif
          } else if (rel < 0) /* reduce */ {
            p->shifts[m] = RUBOUT;
#if 0
            if (vfp) fprintf(vfp, "%d: reduce overrides on %s\n", i, gsym[e]->name);
#endif
          }
          else if (rel > 0) /* shift */ {
            cbit(alook, e);
          } else if (rel == 0) /* conflict */ {
            cbit(alook, e);
            nsrerr++;
            conf = alloc(sizeof(Conflict));
            conf->next = p->conf;
            conf->shift_reduce = YES;
            conf->symbol = e;
            conf->u.sr.shift = t;
            conf->u.sr.reduce = pnum;
            p->conf = conf;
#if 0
            if (vfp) {
              fprintf(vfp,
                      "%d: shift/reduce conflict (shift %d, reduce %d) on %s\n",
                      i, *t, pnum, gsym[e]->name);
            }
#endif
          }
        }
      }
      for (j = 0; j < nr; j++) {
        if (tbit(alook, tmpr[j].sym)) {
          /* reduce/reduce conflict */
          nrrerr++;
          conf = alloc(sizeof(Conflict));
          conf->next = p->conf;
          conf->shift_reduce = NO;
          conf->symbol = tmpr[j].sym;
          conf->u.rr.reduce1 = tmpr[j].num;
          conf->u.rr.reduce2 = pnum;
          p->conf = conf;
#if 0
          if (vfp) {
            fprintf(vfp,
                    "%d: reduce/reduce conflict (reduce %d, reduce %d) on %s\n",
                    i, tmpr[j].num, pnum, gsym[tmpr[j].sym]->name);
          }
#endif
          if (pnum < tmpr[j].num)
            tmpr[j].num = pnum;
          cbit(alook, tmpr[j].sym);
        }
      }

      for_each_member (e, alook) {
        if (nr >= height(tmpr))
          die("Too many actions in one state");
        tmpr[nr].sym = e;
        tmpr[nr].num = pnum;
        nr++;
      }
    }
	
    /* Decide default action */
    if (!tdefact) {
      tdefact = -1;
      qsort(tmpr, nr, sizeof(tmpr[0]), cmp_by_num);
      maxn = 0;
      for (j = 0; j < nr; ) {
        for (k = j; j < nr && tmpr[j].num == tmpr[k].num; j++)
          ;
        if (j - k > maxn && tmpr[k].num > 0) {
          maxn = j - k;
          tdefact = tmpr[k].num;
        }
      }
    }
    /* Squeeze tmpr */
    for (j = k = 0; j < nr; j++) {
      if (tmpr[j].num != tdefact) {
        if (k != j)
          tmpr[k] = tmpr[j];
        k++;
      }
    }
    nr = k;
    qsort(tmpr, nr, sizeof(tmpr[0]), cmp_by_sym);
    tmpr[nr].sym = NILSYM;
    tmpr[nr++].num = tdefact;

    /* Squeeze shift actions */
    for (r = q = p->shifts; *q; q++) {
      if (*q != RUBOUT)
        *r++ = *q;
    }
    *r = NULL;

    for (j = 0; j < nr; j++) {
      if (tmpr[j].num >= 0)
        visited[tmpr[j].num] = 1;
    }
    /* Register tmpr */
    p->reduce = alloc(sizeof(Reduce) * nr);
    nacts2 += nr;
    memcpy(p->reduce, tmpr, sizeof(tmpr[0]) * nr);

#if 0	
    /* print states */
    if (vfp)
      print_state(i);
#endif
  }

  k = 0;
  for (j = 0; j < nprods; j++) {
    if (!visited[j]) {
      k++;
      if (vfp) {
        fprintf(vfp, "Never reduced: ");
        write_item(gram[j]->body + 1);
      }
    }
  }
  if (k) fprintf(stderr, "%d rule(s) never reduced\n", k);

  /* Sort states in decreasing order of entries. */
  /* do not move initial state. */
  states->next = (State *)sortlist((List *)states->next, cmp_state);

  /* Numbering states. */
  statev = talloc(sizeof(statev[0]) * nstates);

  i = 0;
  for (p = states; p != NULL; p = p->next) {
    p->number = i;
    statev[i++] = p;
    if (p->shifts[0] != NULL || p->reduce[0].sym != NILSYM)
      nnonleafstates = i;
  }

  if (vfp) {
    for (i = 0; i < nstates; i++)
      print_state(i);
  }
}




global void comp_lalr()
{
  /* precompute symbols that reach empty production */
  comp_empty();

  /* precompute first set & nullable set */
  first_nullable_precomp();

  /* compute kernels of LR(0) items & fill shift entries */
  comp_kernels();

  /* compute lookaheads */
  comp_lookaheads();
#if 0
  fprintf(stderr, "lookahead done. ");
  show_mem_usage();
#endif

  /* fill reduce entries & check conflictions */
  fill_reduce();
#if 0
  fprintf(stderr, "fill_reduce done. ");
  show_mem_usage();
#endif

  /* print diagnostic messages */
  if (nsrerr != expected_srconf || nrrerr != 0) {
    char *conj = "";
    fprintf(stderr, "%s: there are", filename);
    if (nsrerr != expected_srconf) {
      fprintf(stderr, " %d shift/reduce", nsrerr);
      conj = " and";
    }
    if (nrrerr != 0)
      fprintf(stderr, "%s %d reduce/reduce", conj, nrrerr);
    fprintf(stderr, " conflicts\n");
  }

  if (vfp) {
    fprintf(vfp, "\nStatistics for %s:\n", filename);
    fprintf(vfp, "\t%d terminal symbols\n", nterms);
    fprintf(vfp, "\t%d nonterminal symbols\n", nnonts);
    fprintf(vfp, "\t%d productions\n", nprods);
    fprintf(vfp, "\t%d states\n", nstates);
    fprintf(vfp, "\t%d shift/reduce, %d reduce/reduce conflicts\n",
            nsrerr, nrrerr);
    fprintf(vfp, "\t%d items\n", nitems);
    fprintf(vfp, "\t%d lookahead sets used\n", nlooks);
    fprintf(vfp, "\t%d+%d=%d action entries\n", nacts, nacts2, nacts + nacts2);
    fprintf(vfp, "\t%ld bytes used\n", bytes_allocated());
  }

  release();
}
