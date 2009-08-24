#ifndef _lalr_h
#define _lalr_h

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


extern int nstates;
extern int nnonleafstates;
extern State **statev;
extern void comp_lalr(void);
#endif
