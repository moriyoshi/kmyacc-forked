/*
 * misc.c - miscellaneous routines
 *
 * Copyright (C) 1993, 2005  MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#if HAS_STDLIB
# include <stdlib.h>
#else
char *malloc();
#endif /* HAS_STDLIB */

#include "common.h"
#include "yacc.h"

#include "misc.h"


/* alignment */
#define ALIGN 8


#define mem_roundup(x) (((x) + (ALIGN-1)) & -ALIGN)


/* Return extension of file pathname. */
global char *extension(char *path)
{
  char *p = NULL;
  while (*path) {
    if (*path == '.')
      p = path;
    else if (*path == '/')
      p = NULL;
    path++;
  }
  return p ? p : path;
}


global void error1(char *fmt, char *x)
{
  char c;

  fprintf(stderr, "%s:%d: ", filename, lineno);
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


global void error(char *fmt)
{
  error1(fmt, NULL);
}


/* Report error and exit */
global void die(char *msg)
{
  error(msg);
  exit(1);
}


/* Report error and exit */
global void die1(char *msg, char *str)
{
  error1(msg, str);
  exit(1);
}


/* List handling */

#ifndef global
typedef struct list {
  struct list *next;
  void *elem;
} List;
#endif /* global */


/* Return reversed list. */
global List *nreverse(List *p)
{
  List *q;

  q = NULL;
  while (p != NULL) {
    List *w = p->next;
    p->next = q;
    q = p;
    p = w;
  }
  return q;
}



/*
 * list merge sorting
 *	Donald E Knuth, "The Art of Computer Programming",
 *	Vol.3 / Sorting & Searching, pp. 165-166, Algorithm L.
 */
global List *sortlist(List *p, int (*compar)())
{
  List xh, yh, *xp, *yp, *x, *y;
  uint n, xn, yn;

  /* L1. Separate list p into 2 lists. */
  xp = &xh;
  yp = &yh;
  while (p != NULL) {
    xp->next = p;
    xp = p;
    p = p->next;
    if (p == NULL)
      break;
    yp->next = p;
    yp = p;
    p = p->next;
  }
  xp->next = NULL;
  yp->next = NULL;
  for (n = 1; yh.next != NULL; n *= 2) {
    /* L2. Begin new pass. */
    xp = &xh;
    yp = &yh;
    x = xp->next;
    y = yp->next;
    xn = yn = n;
    do {
      /* L3. Compare */
      if (compar(x, y) <= 0) {
        /* L4. Advance x. */
        xp->next = x;
        xp = x;
        x = x->next;
        if (--xn != 0 && x != NULL)
          continue;
        /* L5. Complete the sublist. */
        xp->next = y;
        xp = yp;
        do {
          yp = y;
          y = y->next;
        } while (--yn != 0 && y != NULL);
      } else {
        /* L6. Advance y. */
        xp->next = y;
        xp = y;
        y = y->next;
        if (--yn != 0 && y != NULL)
          continue;
        /* L7. Complete the sublist. */
        xp->next = x;
        xp = yp;
        do {
          yp = x;
          x = x->next;
        } while (--xn != 0 && x != NULL);
      }
      xn = yn = n;
      /* L8. End of pass? */
    } while (y != NULL);
    xp->next = x;
    yp->next = NULL;
  }

  return xh.next;
}



/* Flexible Array */
#ifndef global
typedef struct flexstr Flexstr;
struct flexstr {
  int alloc_size;
  int length;
  char *body;
};
#endif /* global */

void *emalloc(int size)
{
  void *p = malloc(size);
  if (p == NULL)
    die("Out of memory");
  return p;
}


/* Create flexible array object */
global Flexstr *new_flexstr(int defaultsize)
{
  Flexstr *fap = emalloc(sizeof(Flexstr));
  fap->alloc_size = defaultsize;
  assert(fap->alloc_size >= 1);
  fap->body = emalloc(defaultsize);
  fap->body[0] = '\0';
  fap->length = 0;
  return fap;
}


/* Copy string to flexible string */
global void copy_flexstr(Flexstr *fap, char *str)
{
  int n = strlen(str);
  resize_flexstr(fap, n + 1);
  strcpy(fap->body, str);
  fap->length = n;
}


/* Append string */
global void append_flexstr(Flexstr *fap, char *str)
{
  int n = strlen(str);
  int size = fap->length + n + 1;
  resize_flexstr(fap, size);
  strcpy(fap->body + fap->length, str);
  fap->length += n;
}


/* Resize flexible array object */
global void resize_flexstr(Flexstr *fap, int requiredsize)
{
  int size = fap->alloc_size;
  assert(fap->alloc_size >= 1);
  while (size < requiredsize)
    size *= 2;
  if (fap->alloc_size < size) {
    void *p = emalloc(size);
    memcpy(p, fap->body, fap->length + 1);
    free(fap->body);
    fap->body = p;
    fap->alloc_size = size;
  }
}

/* Memory allocation */



#define ALLOCTHRESH 128
#define CHUNKSIZE 4096

static char *alloc_cp;
static int alloc_left;
static long size_allocated;



/* Allocate persistent object */
global void *alloc(unsigned s)
{
  char *ap;

  s = mem_roundup(s);
  size_allocated += s;
  if (s >= ALLOCTHRESH) {
    if ((ap = malloc(s)) == NULL)
      die("Out of memory");
  } else {
    if (s > alloc_left) {
      if ((alloc_cp = malloc(CHUNKSIZE)) == NULL)
        die("Out of memory");
      alloc_left = CHUNKSIZE;
    }
    ap = alloc_cp;
    alloc_cp += s;
    alloc_left -= s;
  }
  memset(ap, 0, s);
  return ap;
}


/* Allocate temporary object (freeable by release()) */
global void *talloc(unsigned s)
{
  return alloc(s);
}


/* Release temporary object allocated by talloc() */
global void release()
{
}


global long bytes_allocated()
{
  return size_allocated;
}


global void show_mem_usage()
{
  fprintf(stderr, "(%ld)\n", size_allocated);
}
