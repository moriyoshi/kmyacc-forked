#ifndef _misc_h
#define _misc_h
typedef struct list {
  struct list *next;
  void *elem;
} List;
typedef struct flexstr Flexstr;
struct flexstr {
  int alloc_size;
  int length;
  char *body;
};
extern char *extension(char *path);
extern void error1(char *fmt, char *x);
extern void error(char *fmt);
extern void die(char *msg);
extern void die1(char *msg, char *str);
extern List *nreverse(List *p);
extern List *sortlist(List *p, int (*compar)());
extern Flexstr *new_flexstr(int defaultsize);
extern void copy_flexstr(Flexstr *fap, char *str);
extern void append_flexstr(Flexstr *fap, char *str);
extern void resize_flexstr(Flexstr *fap, int requiredsize);
extern void *alloc(unsigned s);
extern void *talloc(unsigned s);
extern void release(void);
extern long bytes_allocated(void);
extern void show_mem_usage(void);
#endif
