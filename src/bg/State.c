#ifndef AMALGAMATION
#include "config.h"
#include "State.h"
#include "Collection.h"
#include "Document.h"
#include "parson.h"
#include "http/http.h"

#include "palloc/sstream.h"
#include <palloc/palloc.h>
#endif

#include <time.h>
#include <string.h>

struct bgState *bg;

/* Find out where error/success callbacks should be called
in this particular function
*/
void bgUpdate()
{
  /* For updating interval */
  size_t i = 0;
  time_t tNow = time(NULL);

  /* Updating Interval */
  bg->intervalTimer -= (tNow - bg->t)*1000;
  bg->t = tNow;

  /* Polling collections http connections to push through data */
  for(i = 0; i < vector_size(bg->collections); i++)
  {
    if(vector_at(bg->collections, i))
    {
      HttpRequestComplete(vector_at(bg->collections, i)->http);
    }
  }

  /* Pushing data if interval is done */
  if(bg->intervalTimer <= 0)
  {
    int successes = 0;
    sstream *ser = sstream_new();
    sstream *url = sstream_new();
    /* Upload collections */
    for(i = 0; i < vector_size(bg->collections); i++)
    {
      JSON_Value* v = NULL;
      size_t j = 0;
      struct bgCollection* c = vector_at(bg->collections, i);

      //TODO continue if no data to send

      if(HttpRequestComplete(c->http))
      {
        if(HttpResponseStatus(c->http) == 200)
        {
          bg->successFunc(sstream_cstr(c->name), c->lastDocumentCount);
        }

        /* Serializing documents of collection */
        for(j = 0; j < vector_size(c->documents); j++)
        {
          v = vector_at(c->documents, j)->rootVal;
          sstream_push_cstr(ser, json_serialize_to_string(v));
        }

        sstream_clear(url);
        sstream_push_cstr(url, sstream_cstr(bg->fullUrl));
        sstream_push_cstr(url, sstream_cstr(c->name));
        sstream_push_cstr(url, "/documents");
        HttpRequest(c->http, sstream_cstr(url), sstream_cstr(ser));
        sstream_clear(ser);

        c->lastDocumentCount = vector_size(c->documents);
        for(j = 0; j < vector_size(c->documents); j++)
        {
          bgDocumentDestroy(vector_at(c->documents, j));
        }
        vector_clear(c->documents);
      }
    }

    /* Cleaning up sstream */
    sstream_delete(ser);

    /* Resetting intervalTimer */
    bg->intervalTimer = bg->interval;
  }
}

void bgAuth(char *guid, char *key)
{
  bg = palloc(struct bgState);
  bg->collections = vector_new(struct bgCollection *);
  bg->interval = 2000;

  bg->url = BG_URL;
  bg->path = BG_PATH;

  bg->fullUrl = sstream_new();
  sstream_push_cstr(bg->fullUrl, bg->url);
  sstream_push_cstr(bg->fullUrl, bg->path);
  sstream_push_cstr(bg->fullUrl, "/projects/collections/");

  //TODO move to sstream and delete guid+key
  bg->guid = guid;
  bg->key = key;
}

void bgCleanup()
{
  /*
  Looping throough and calling 'destructor'
  and then deleting remnants with vector_delete
  */
  size_t i = 0;
  for(i = 0; i < vector_size(bg->collections); i ++)
  {
    /*NULLS pointer in function*/
    bgCollectionDestroy(vector_at(bg->collections, i));
  }
  vector_delete(bg->collections);

  pfree(bg->url);
  pfree(bg->path);
  sstream_delete(bg->fullUrl);
  pfree(bg->guid);
  pfree(bg->key);

  pfree(bg);
}

void bgInterval(int milli)
{
  bg->interval = milli;
}

void bgErrorFunc(void (*errorFunc)(char *cln, int code))
{
  bg->errorFunc = errorFunc;
}

void bgSuccessFunc(void (*successFunc)(char *cln, int count))
{
  bg->successFunc = successFunc;
}

