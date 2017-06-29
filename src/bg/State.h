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

  struct sstream *url;
  struct sstream *path;
  struct sstream *fullUrl;
  struct sstream *guid;
  struct sstream *key;

  time_t t;

  vector(struct bgCollection *) *collections;
  void (*errorFunc)(const char *cln, int code);
  void (*successFunc)(const char *cln, int count);
};

extern struct bgState *bg;

#endif
