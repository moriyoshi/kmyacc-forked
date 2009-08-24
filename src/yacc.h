#ifndef _yacc_h
#define _yacc_h
extern bool lflag;
extern bool tflag;
extern bool debug;
extern bool aflag;
extern bool nflag;
extern bool iflag;
extern FILE *ifp, *ofp, *vfp, *hfp;
extern int lineno;
extern char *filename;
extern char *outfilename;
extern int expected_srconf;
extern char *pspref;
extern FILE *efopen(char *fn, char *mode);
extern void efclose(FILE *fp);
#endif
