#ifndef BG_STATE_H
#define BG_STATE_H

#ifndef AMALGAMATION
  #include <palloc/vector.h>
#endif

struct bgCollection;
struct sstream;

struct bgState
{
  int authenticated;
  int interval;
  int intervalTimer;

  char *url;
  char *path;
  struct sstream *fullUrl;
  char *guid;
  char* key;

  time_t t;

  vector(struct bgCollection *) *collections;
  void (*errorFunc)(char *cln, int code);
  void (*successFunc)(char *cln, int count);
};

extern struct bgState *bg;

#endif
