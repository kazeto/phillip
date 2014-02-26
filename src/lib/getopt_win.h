/*
POSIX getopt for Windows
AT&T Public License
Code given out at the 1985 UNIFORUM conference in Dallas.  
*/

#ifdef __GNUC__
#include <getopt.h>
#else

#ifndef _WINGETOPT_H_
#define _WINGETOPT_H_

extern int opterr;
extern int optind;
extern int optopt;
extern char *optarg;
extern int getopt(int argc, char **argv, char *opts);

#endif  /* _GETOPT_H_ */

#endif  /* __GNUC__ */