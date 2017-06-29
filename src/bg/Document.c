#ifndef AMALGAMATION
  #include "Document.h"
  #include "State.h"
  #include "parson.h"

  #include <palloc/palloc.h>
  #include <palloc/sstream.h>
#endif

void bgUpdate();

struct bgDocument *bgDocumentCreate()
{
  struct bgDocument *rtn = NULL;

  rtn = palloc(struct bgDocument);

  rtn->rootVal = json_value_init_object();
  rtn->rootObj = json_value_get_object(rtn->rootVal);
  //rtn->rootArr = json_value_get_array(rtn->rootVal);

  return rtn;
}

void bgDocumentDestroy(struct bgDocument *doc)
{
  /*unwinds json_val list structure and frees memory*/
  json_value_free(doc->rootVal);

  pfree(doc);
}

void bgDocumentAddCStr(struct bgDocument *doc, char *path, char *val)
{
  sstream* ctx = sstream_new();
  vector(sstream *) *out = vector_new(sstream *);

  sstream_push_cstr(ctx, path);
  sstream_split(ctx, '.', out);
  sstream_delete(ctx);

  if(vector_size(out) > 0)
  {
    size_t i = 0;

    for(i = 0; i < vector_size(out); i++)
    {
      if(vector_at(out, i))
      {
        sstream_delete(vector_at(out, i));
      }
    }
  }

  vector_delete(out);
  bgUpdate();
}

void bgDocumentAddInt(struct bgDocument *doc, char *path, int val)
{
  sstream* ctx = sstream_new();
  vector(sstream *) *out = vector_new(sstream *);

  sstream_push_cstr(ctx, path);
  sstream_split(ctx, '.', out);
  sstream_delete(ctx);

  if(vector_size(out) > 0)
  {
    size_t i = 0;

    for(i = 0; i < vector_size(out); i++)
    {
      if(vector_at(out, i))
      {
        sstream_delete(vector_at(out, i));
      }
    }
  }

  vector_delete(out);
  bgUpdate();
}

void bgDocumentAddDouble(struct bgDocument *doc, char *path, double val)
{
  sstream* ctx = sstream_new();
  size_t i = 0;
  vector(sstream *) *out = vector_new(sstream *);

  sstream_push_cstr(ctx, path);
  sstream_split(ctx, '.', out);
  sstream_delete(ctx);

  if(vector_size(out) > 0)
  {
    for(i = 0; i < vector_size(out); i++)
    {
      if(vector_at(out, i))
      {
        sstream_delete(vector_at(out, i));
      }
    }
  }

  vector_delete(out);
  bgUpdate();
}

void bgDocumentAddBool(struct bgDocument *doc, char *path, int val)
{
  sstream* ctx = sstream_new();
  vector(sstream *) *out = vector_new(sstream *);

  sstream_push_cstr(ctx, path);
  sstream_split(ctx, '.', out);
  sstream_delete(ctx);

  if(vector_size(out) > 0)
  {
    size_t i = 0;

    for(i = 0; i < vector_size(out); i++)
    {
      if(vector_at(out, i))
      {
        sstream_delete(vector_at(out, i));
      }
    }
  }

  vector_delete(out);
  bgUpdate();
}

