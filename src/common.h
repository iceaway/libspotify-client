#ifndef COMMON_H_
#define COMMON_H_

#define DEBUG

#ifdef DEBUG
#define debug printf
#else
#define debug do { } while (0);
#endif

#endif
