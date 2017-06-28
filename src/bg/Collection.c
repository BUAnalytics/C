#ifndef AMALGAMATION
  #include "Collection.h"
  #include "Document.h"
  #include "State.h"
  #include "http/http.h"

  #include "palloc/palloc.h"
#endif

#include <stdio.h>
#include <string.h>

void bgUpdate();

void bgCollectionCreate(char *cln)
{
  struct bgCollection* newCln;
  newCln = palloc(struct bgCollection);
 
  newCln->name = sstream_new();
  sstream_push_cstr(newCln->name, cln);

  newCln->documents = vector_new(struct bgDocument*);
  vector_push_back(bg->collections, newCln);
  newCln->lastDocumentCount = 0;

  newCln->http = HttpCreate();
  HttpAddCustomHeader(newCln->http, "AuthAccessKey", bg->guid);
  HttpAddCustomHeader(newCln->http, "AuthAccessSecret", bg->key);
  HttpAddCustomHeader(newCln->http, "Content-Type", "application/json;charset=utf-8");

  bgUpdate();
}

void bgCollectionAdd(char *cln, struct bgDocument *doc)
{
  vector_push_back(bgCollectionGet(cln)->documents, doc);

  /* Again, could be more complex */
  doc = NULL;

  bgUpdate();
}

void bgCollectionUpload(char *cln)
{
  /* For Serializing data */
  sstream *ser = sstream_new();
  sstream *url = sstream_new();
  struct bgCollection* c = bgCollectionGet(cln);
  JSON_Value* v = NULL;
  size_t i = 0;
  int responseCode = 0;

  sstream_push_cstr(ser, "{\"documents\":[");

  /* Concatenating c_str onto ser - dangerous? */
  for(i = 0; i < vector_size(c->documents); i++)
  {
    if(vector_at(c->documents, i))
    {
      v = vector_at(c->documents,i)->rootVal;
      sstream_push_cstr(ser, json_serialize_to_string(v));
    }
  } 

  sstream_push_cstr(ser, "]}");

  /* Sending request to server */
  sstream_push_cstr(url, sstream_cstr(bg->fullUrl));
  sstream_push_cstr(url, sstream_cstr(c->name));
  sstream_push_cstr(url, "/documents");
  HttpRequest(c->http, sstream_cstr(url), sstream_cstr(ser));
  /* blocking while request pushes through */
  while(!HttpRequestComplete(c->http))
  {
    /* blocking stuff */
  }
  /* Handle response */
  responseCode = HttpResponseStatus(c->http);
  if(responseCode != 200)
  {
    if(bg->errorFunc != NULL)
      bg->errorFunc(sstream_cstr(c->name), responseCode);
  }
  else
  {
    if(bg->successFunc != NULL)
      bg->successFunc(sstream_cstr(c->name), vector_size(c->documents));

  }
  
  /* Cleanup */
  sstream_delete(ser);
  sstream_delete(url);

  for(i = 0; i < vector_size(c->documents); i++)
  {
    bgDocumentDestroy(vector_at(c->documents, i));
  }

  /* Clearing vector for later use */
  vector_clear(c->documents);
}

/* Destroys collection and containing documents w/o upload */
void bgCollectionDestroy(struct bgCollection *cln)
{
  /* Document destruction is Possibly complex */
  size_t i = 0;
  if(cln->documents != NULL)
  {
    for(i = 0; i < vector_size(cln->documents); i++)
    {
      bgDocumentDestroy(vector_at(cln->documents, i));
    }
    vector_delete(cln->documents);
  }

  sstream_delete(cln->name);
  HttpDestroy(cln->http);

  pfree(cln);

  cln = NULL;
}

/*  Helper function to get the collection from the state by name 
 *  returns NULL if no collection by cln exists
*/
struct bgCollection *bgCollectionGet(char *cln)
{
  /* 
   * TODO - Change to comparing char* directly
   *  then fall back onto strcmp if failure
   *  
   *  Although, would this still work as name
   *  is a sstream?
  */
  size_t i = 0;

  for(i = 0; i < vector_size(bg->collections); i++)
  {
    if(strcmp(cln, sstream_cstr(vector_at(bg->collections, i)->name)) == 0)
    {
      return vector_at(bg->collections, i);
    }
  }
  
  return NULL;
}
