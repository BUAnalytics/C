#include "bg_analytics.h"
#ifndef AMALGAMATION
  #include "palloc.h"
#endif

#include <stdio.h>
#include <string.h>

static int registered;

struct PoolEntry
{
  void *ptr;
  size_t size;
  char *type;
  int used;
  struct PoolEntry *next;
};

struct PoolEntry *poolHead;

void pool_cleanup()
{
  struct PoolEntry *curr = poolHead;

  printf("[palloc]Evaluating memory...\n");

  while(curr)
  {
    struct PoolEntry *tmp = curr;

    if(curr->used)
    {
      printf("[palloc]Leak\n");
      printf("  Type: %s\n", curr->type);
      printf("  Size: %i\n", (int)curr->size);
      printf("  Used: %i\n", curr->used);
    }
    else
    {
      size_t i = 0;
      char *ptr = (char*)curr->ptr;
      int dirty = 0;

      for(i = 0; i < curr->size; i++)
      {
        if(*ptr != PALLOC_SENTINEL)
        {
          dirty = 1;
        }

        ptr++;
      }

      if(dirty == 1)
      {
        printf("[palloc]Use after free\n");
        printf("  Type: %s\n", curr->type);
        printf("  Size: %i\n", (int)curr->size);
      }
    }

    curr = curr->next;

    free(tmp->ptr);
    free(tmp->type);
    free(tmp);
  }

  poolHead = NULL;
}

#ifdef PALLOC_ACTIVE
void pfree(void *ptr)
{
  struct PoolEntry *entry = poolHead;

  while(entry)
  {
    if(entry->ptr == ptr)
    {
      if(!entry->used)
      {
        printf("Error: Memory already freed\n");
      }

#ifdef PALLOC_DEBUG
      printf("Freeing: %s\n", entry->type);
#endif
      memset(entry->ptr, PALLOC_SENTINEL, entry->size);
      entry->used = 0;
      return;
    }

    entry = entry->next;
  }

  printf("Error: Memory not managed by pool\n");
}
#else
void pfree(void *ptr)
{
  free(ptr);
}
#endif

#ifdef PALLOC_ACTIVE
void *_palloc(size_t size, char *type)
{
  struct PoolEntry *entry = poolHead;

  if(!registered)
  {
    printf("[palloc]Registering hook...\n");
    registered = 1;
    atexit(pool_cleanup);
  }

  while(entry)
  {
    if(!entry->used && strcmp(type, entry->type) == 0)
    {
      char *ptr = (char *)entry->ptr;
      size_t i = 0;
      int dirty = 0;

#ifdef PALLOC_DEBUG
      printf("Reusing: %s\n", type);
#endif

      for(i = 0; i < entry->size; i++)
      {
        if(*ptr != PALLOC_SENTINEL)
        {
          dirty = 1;
          break;
        }

        ptr++;
      }

      if(dirty)
      {
        printf("[palloc]Use after free\n");
        printf("  Type: %s\n", entry->type);
        printf("  Size: %i\n", (int)entry->size);
      }

      entry->used = 1;

      if(PALLOC_SENTINEL != 0)
      {
        memset(entry->ptr, 0, entry->size);
      }

      return entry->ptr;
    }

    entry = entry->next;
  }

#ifdef PALLOC_DEBUG
  printf("Allocating: %s\n", type);
#endif
  entry = calloc(1, sizeof(*entry));
  if(!entry) return NULL;

  entry->ptr = calloc(1, size);

  if(!entry->ptr)
  {
    free(entry);
    return NULL;
  }

  entry->size = size;
  entry->type = calloc(strlen(type) + 1, sizeof(char));
  strcpy(entry->type, type);
  entry->used = 1;

  entry->next = poolHead;
  poolHead = entry;

  return entry->ptr;
}
#else
void *_palloc(size_t size, char *type)
{
  void *rtn = NULL;

  rtn = calloc(1, size);

  return rtn;
}
#endif

#ifndef AMALGAMATION
  #include "vector.h"
  #include "palloc.h"
#endif

#include <string.h>

void *_VectorNew(size_t size, char *type)
{
  struct _Vector *rtn = NULL;
  char typeStr[256] = {0};

  strcpy(typeStr, "vector(");
  strcat(typeStr, type);
  strcat(typeStr, ")");
  rtn = _palloc(sizeof(*rtn), typeStr);

  strcpy(typeStr, "vector_header(");
  strcat(typeStr, type);
  strcat(typeStr, ")");
  rtn->vh = _palloc(sizeof(*rtn->vh), typeStr);
  rtn->vh->entrySize = size;

  return rtn;
}

int _VectorOobAssert(void *_vh, size_t idx)
{
  struct _VectorHeader *vh = _vh;

  if(vh->size <= idx)
  {
    printf("Error: Index out of bounds\n");
    return 1;
  }

  return 0;
}

void _VectorErase(void *_vh, void *_v, size_t idx)
{
  struct _VectorHeader *vh = _vh;
  struct _Vector *v = _v;
  size_t restSize = (vh->size - (idx + 1)) * vh->entrySize;

  _VectorOobAssert(_vh, idx);

  if(restSize > 0)
  {
    char *element = (char *)v->data + idx * vh->entrySize;
    char *rest = element + vh->entrySize;
    memmove(element, rest, restSize);
  }

  vh->size --;
}

void _VectorResize(void *_vh, void *_v, size_t size)
{
  struct _Vector *v = _v;
  struct _VectorHeader *vh = _vh;

  if(vh->size == size)
  {
    return;
  }
  else if(size == 0)
  {
    if(v->data)
    {
      free(v->data);
      v->data = NULL;
    }
  }
  else if(vh->size > size)
  {
    v->data = realloc(v->data, size * vh->entrySize);

    if(!v->data)
    {
      /* TODO: Should cache and revert */
      printf("Error: Failed to reallocate\n");
    }
  }
  else if(vh->size < size)
  {
    char *cur = NULL;
    char *last = NULL;

    v->data = realloc(v->data, size * vh->entrySize);

    if(!v->data)
    {
      /* TODO: Should cache and revert */
      printf("Error: Failed to reallocate\n");
    }

    cur = v->data;
    cur += vh->size * vh->entrySize;
    last = v->data;
    last += size * vh->entrySize;

    while(cur != last)
    {
      *cur = 0;
      cur++;
    }
  }

  vh->size = size;
}

size_t _VectorSize(void *_vh)
{
  struct _VectorHeader *vh = _vh;

  return vh->size;
}

void _VectorDelete(void *_vh, void *_v)
{
  struct _Vector *v = _v;
  struct _VectorHeader *vh = _vh;

  if(v->vh != vh)
  {
    printf("Error: Invalid vector\n");
  }

  if(v->data)
  {
    free(v->data);
  }

  pfree(vh);
  pfree(v);
}


#ifndef AMALGAMATION
  #include "sstream.h"
  #include "palloc.h"
#endif

#include <string.h>
#include <stdio.h>

struct sstream *sstream_new()
{
  struct sstream *rtn = NULL;

  rtn = palloc(struct sstream);

  return rtn;
}

void sstream_clear(struct sstream *ctx)
{
  struct Chunk *curr = ctx->first;

  while(curr)
  {
    struct Chunk *tmp = curr;

    curr = curr->next;
    if(tmp->s) free(tmp->s);
    pfree(tmp);
  }

  ctx->first = NULL;
}

void sstream_delete(struct sstream *ctx)
{
  sstream_clear(ctx);
  pfree(ctx);
}

void sstream_push_int(struct sstream *ctx, int val)
{
  char buffer[128] = {0};

  sprintf(buffer, "%i", val);
  sstream_push_cstr(ctx, buffer);
}

void sstream_push_float(struct sstream *ctx, float val)
{
  char buffer[128] = {0};

  sprintf(buffer, "%f", val);
  sstream_push_cstr(ctx, buffer);
}

void sstream_push_double(struct sstream *ctx, double val)
{
  char buffer[128] = {0};

  sprintf(buffer, "%f", val);
  sstream_push_cstr(ctx, buffer);
}

void sstream_push_char(struct sstream *ctx, char val)
{
/*
  char buffer[2] = {0};

  buffer[0] = val;
  sstream_push_cstr(ctx, buffer);
*/

  struct Chunk *nc = NULL;
  struct Chunk *curr = NULL;

  nc = palloc(struct Chunk);
  nc->c = val;
  nc->len = 1;

  if(!ctx->first)
  {
    ctx->first = nc;
    return;
  }

  curr = ctx->first;

  while(curr->next)
  {
    curr = curr->next;
  }

  curr->next = nc;
}

void sstream_push_cstr(struct sstream *ctx, char *s)
{
  struct Chunk *nc = NULL;
  struct Chunk *curr = NULL;
  size_t len = strlen(s);

  if(len < 1) return;

  nc = palloc(struct Chunk);
  nc->s = malloc(sizeof(char) * (len + 1));
  strcpy(nc->s, s);
  nc->len = len;

  if(!ctx->first)
  {
    ctx->first = nc;
    return;
  }

  curr = ctx->first;

  while(curr->next)
  {
    curr = curr->next;
  }

  curr->next = nc;
}

void sstream_collate(struct sstream *ctx)
{
  size_t allocSize = 0;
  struct Chunk *curr = ctx->first;
  char *ns = NULL;

  if(!curr) return;

  if(curr->s)
  {
    if(!curr->next) return;
  }

  while(curr)
  {
    allocSize += curr->len;
    curr = curr->next;
  }

  allocSize++;
  ns = malloc(allocSize * sizeof(char));
  ns[0] = '\0';
  curr = ctx->first;

  while(curr)
  {
    struct Chunk *tmp = curr;

    if(!curr->s)
    {
      char cs[2] = {0};

      cs[0] = curr->c;
      strcat(ns, cs);
    }
    else
    {
      strcat(ns, curr->s);
    }

    curr = curr->next;
    if(tmp->s) free(tmp->s);
    pfree(tmp);
  }

  ctx->first = palloc(struct Chunk);
  ctx->first->s = ns;
  ctx->first->len = allocSize - 1;
}

char *sstream_cstr(struct sstream *ctx)
{
  sstream_collate(ctx);

  if(!ctx->first) return "";

  return ctx->first->s;
}

size_t sstream_length(struct sstream *ctx)
{
  sstream_collate(ctx);

  if(!ctx->first) return 0;

  return ctx->first->len;
}

int sstream_int(struct sstream *ctx)
{
  sstream_collate(ctx);

  if(!ctx->first) return 0;

  return atoi(ctx->first->s);
}

char sstream_at(struct sstream *ctx, size_t i)
{
  sstream_collate(ctx);

  if(!ctx->first)
  {
    printf("Error: Stream is empty\n");
    abort();
  }

  if(i >= ctx->first->len)
  {
    printf("Error: Index out of bounds\n");
    abort();
  }

  return ctx->first->s[i];
}

void sstream_push_chars(struct sstream *ctx, char *values, size_t count)
{
  char *tmp = NULL;

  tmp = malloc(sizeof(char) * (count + 1));
  tmp[count] = 0;
  memcpy(tmp, values, sizeof(char) * count);
  sstream_push_cstr(ctx, tmp);

  free(tmp);
}

void sstream_split(struct sstream *ctx, char token,
  vector(struct sstream*) *out)
{
  size_t i = 0;
  struct sstream *curr = NULL;

  curr = sstream_new();

  for(i = 0; i < sstream_length(ctx); i++)
  {
    if(sstream_at(ctx, i) == token)
    {
      vector_push_back(out, curr);
      curr = sstream_new();
    }
    else
    {
      sstream_push_char(curr, sstream_at(ctx, i));
    }
  }

  if(sstream_length(curr) > 0)
  {
    vector_push_back(out, curr);
  }
  else
  {
    sstream_delete(curr);
  }
}

#ifndef AMALGAMATION
  #include "http.h"

  #include <palloc/palloc.h>
  #include <palloc/vector.h>
  #include <palloc/sstream.h>
#endif

#ifdef _WIN32
  #define USE_WINSOCK
  #define USE_WINAPI
#else
  #define USE_POSIX
#endif

#ifdef USE_WINSOCK
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif

#ifdef USE_POSIX
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

#include <stdio.h>
#include <string.h>

#define HTTP_CONNECTING 1
#define HTTP_RECEIVING 2
#define HTTP_COMPLETE 3

#define BUFFER_SIZE 1024

#ifdef USE_POSIX
  #define NULL_SOCKET -1
#endif
#ifdef USE_WINSOCK
  #define NULL_SOCKET INVALID_SOCKET
#endif

static int winsockInitialized;

struct CustomHeader
{
  sstream *variable;
  sstream *value;
};

struct Http
{
  sstream *host;
  sstream *path;
  sstream *query;
  sstream *post;
  vector(char) *raw;
  vector(int) *socks;
  sstream *rawHeaders;
  sstream *rawContent;
  vector(struct CustomHeader) *customHeaders;
  int sock;
  int status;
};

void HttpAddCustomHeader(struct Http *ctx, char *variable, char *value)
{
  struct CustomHeader ch = {0};

  ch.variable = sstream_new();
  sstream_push_cstr(ch.variable, variable);
  ch.value = sstream_new();
  sstream_push_cstr(ch.value, value);
  vector_push_back(ctx->customHeaders, ch);
}

void _HttpClearSocks(struct Http *ctx)
{
  size_t i = 0;

  for(i = 0; i < vector_size(ctx->socks); i++)
  {
#ifdef USE_POSIX
    close(vector_at(ctx->socks, i));
#endif
#ifdef USE_WINSOCK
    shutdown(vector_at(ctx->socks, i), SD_BOTH);
    closesocket(vector_at(ctx->socks, i));
#endif
  }

  vector_clear(ctx->socks);
}

int HttpState(struct Http *ctx)
{
  if(vector_size(ctx->socks) > 0)
  {
    return HTTP_CONNECTING;
  }
  else if(ctx->sock != NULL_SOCKET)
  {
    return HTTP_RECEIVING;
  }

  return HTTP_COMPLETE;
}

#ifdef USE_WINSOCK
void _HttpShutdownWinsock()
{
  WSACleanup();
}
#endif

struct Http *HttpCreate()
{
  struct Http *rtn = NULL;
#ifdef USE_WINSOCK
  if(!winsockInitialized)
  {
    WSADATA wsaData = {0};
    int result = 0;

    result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if(result != 0)
    {
      printf("WSAStartup failed with error: %d\n", result);
      return NULL;
    }

    winsockInitialized = 1;
    atexit(_HttpShutdownWinsock);
  }
#endif

  rtn = palloc(struct Http);
  rtn->raw = vector_new(char);
  rtn->rawContent = sstream_new();
  rtn->rawHeaders = sstream_new();
  rtn->socks = vector_new(int);
  rtn->customHeaders = vector_new(struct CustomHeader);
  rtn->sock = NULL_SOCKET;
  rtn->host = sstream_new();
  rtn->path = sstream_new();
  rtn->query = sstream_new();
  rtn->post = sstream_new();

  return rtn;
}

void HttpDestroy(struct Http *ctx)
{
  size_t i = 0;

  _HttpClearSocks(ctx);
  vector_delete(ctx->socks);
  sstream_delete(ctx->rawHeaders);
  sstream_delete(ctx->rawContent);
  vector_delete(ctx->raw);
  sstream_delete(ctx->host);
  sstream_delete(ctx->path);
  sstream_delete(ctx->query);
  sstream_delete(ctx->post);

  for(i = 0; i < vector_size(ctx->customHeaders); i++)
  {
    sstream_delete(vector_at(ctx->customHeaders, i).variable);
    sstream_delete(vector_at(ctx->customHeaders, i).value);
  }

  vector_delete(ctx->customHeaders);

  pfree(ctx);
}

void _HttpPollConnect(struct Http *ctx)
{
  size_t i = 0;
  sstream *content = NULL;

  //printf("polling connect\n");

  for(i = 0; i < vector_size(ctx->socks); i++)
  {
    fd_set write_fds = {0};
    struct timeval tv = {0};
    int err = 0;
    int sock = vector_at(ctx->socks, i);

    FD_SET(sock, &write_fds);
    err = select(sock + 1, NULL, &write_fds, NULL, &tv);

    if(err == 0)
    {
      continue;
    }
    else if(err == -1)
    {
#ifdef USE_POSIX
      close(sock);
#endif
#ifdef USE_WINSOCK
      shutdown(sock, SD_BOTH);
      closesocket(sock);
#endif
      vector_erase(ctx->socks, i);
      i--;
      continue;
    }
    else
    {
      int optval = 0;
      int optlen = sizeof(optval);
#ifdef USE_POSIX
      if(getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1)
#endif
#ifdef USE_WINSOCK
      if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == -1)
#endif
      {
#ifdef USE_POSIX
        close(sock);
#endif
#ifdef USE_WINSOCK
        shutdown(sock, SD_BOTH);
        closesocket(sock);
#endif
        vector_erase(ctx->socks, i);
        i--;
        continue;
      }

      if(optval != 0)
      {
#ifdef USE_POSIX
        close(sock);
#endif
#ifdef USE_WINSOCK
        shutdown(sock, SD_BOTH);
        closesocket(sock);
#endif
        vector_erase(ctx->socks, i);
        i--;
        continue;
      }
    }

    ctx->sock = sock;
    vector_erase(ctx->socks, i);
    _HttpClearSocks(ctx);
    break;
  }

  if(ctx->sock == NULL_SOCKET && vector_size(ctx->socks) < 1)
  {
    ctx->status = -1;
    return;
  }

  if(ctx->sock == NULL_SOCKET)
  {
    return;
  }

  content = sstream_new();

  if(sstream_length(ctx->post) > 0)
  {
    sstream_push_cstr(content, "POST ");
  }
  else
  {
    sstream_push_cstr(content, "GET ");
  }

  sstream_push_cstr(content, sstream_cstr(ctx->path));
  /*TODO*/
  sstream_push_char(content, '?');
  sstream_push_cstr(content, sstream_cstr(ctx->query));
  sstream_push_cstr(content, " HTTP/1.0\r\n");
  sstream_push_cstr(content, "Host: ");
  sstream_push_cstr(content, sstream_cstr(ctx->host));
  sstream_push_cstr(content, "\r\n");

  for(i = 0; i < vector_size(ctx->customHeaders); i++)
  {
    sstream_push_cstr(content,
      sstream_cstr(vector_at(ctx->customHeaders, i).variable));

    sstream_push_cstr(content, ": ");

    sstream_push_cstr(content,
      sstream_cstr(vector_at(ctx->customHeaders, i).value));

    sstream_push_cstr(content, "\r\n");
  }

  if(sstream_length(ctx->post) > 0)
  {
    sstream_push_cstr(content, "Content-Length: ");
    sstream_push_int(content, sstream_length(ctx->post));
    sstream_push_cstr(content, "\r\n");
  }

  sstream_push_cstr(content, "\r\n");

  if(sstream_length(ctx->post) > 0)
  {
    sstream_push_cstr(content, sstream_cstr(ctx->post));
  }

#ifdef USE_POSIX
  send(ctx->sock, sstream_cstr(content), sstream_length(content), MSG_NOSIGNAL);
#endif
#ifdef USE_WINSOCK
  send(ctx->sock, sstream_cstr(content), sstream_length(content), 0);
#endif

  sstream_delete(content);
}

void _HttpProcessHeaders(struct Http *ctx)
{
  vector(sstream *) *lines = NULL;
  vector(sstream *) *line = NULL;
  size_t i = 0;
  size_t j = 0;

  lines = vector_new(sstream *);
  line = vector_new(sstream *);
  sstream_split(ctx->rawHeaders, '\n', lines);

  for(i = 0; i < vector_size(lines); i++)
  {
    sstream_split(vector_at(lines, i), ' ', line);

    if(vector_size(line) >= 2)
    {
      if(strcmp(sstream_cstr(vector_at(line, 0)), "HTTP/1.1") == 0 ||
        strcmp(sstream_cstr(vector_at(line, 0)), "HTTP/1.0") == 0)
      {
        //printf("Status: %s\n", sstream_cstr(vector_at(line, 1)));
        ctx->status = atoi(sstream_cstr(vector_at(line, 1)));
      }
    }

    for(j = 0; j < vector_size(line); j++)
    {
      sstream_delete(vector_at(line, j));
    }

    vector_clear(line);
  }

  //printf("%i %s\n", (int)vector_size(lines), sstream_cstr(ctx->rawHeaders));

  for(i = 0; i < vector_size(lines); i++)
  {
    sstream_delete(vector_at(lines, i));
  }

  vector_delete(lines);
  vector_delete(line);
}

void _HttpProcessRaw(struct Http *ctx)
{
  if(sstream_length(ctx->rawHeaders) == 0)
  {
    size_t i = 0;
    char last[5] = {0};

    for(i = 0; i < vector_size(ctx->raw); i++)
    {
      last[0] = last[1];
      last[1] = last[2];
      last[2] = last[3];
      last[3] = vector_at(ctx->raw, i);

      if(strcmp(last, "\r\n\r\n") == 0)
      {
        size_t c = 0;

        for(c = 0; c < i-3; c++)
        {
          sstream_push_char(ctx->rawHeaders, vector_at(ctx->raw, c));
        }

        _HttpProcessHeaders(ctx);
        break;
      }
    }
  }

  if(sstream_length(ctx->rawHeaders) == 0) return;

  if(ctx->sock == NULL_SOCKET)
  {
    size_t i = 0;

    for(i = sstream_length(ctx->rawHeaders) + 4; i < vector_size(ctx->raw); i++)
    {
      sstream_push_char(ctx->rawContent, vector_at(ctx->raw, i));
    }

    //printf("Content: %s\n", sstream_cstr(ctx->rawContent));
  }
}

void _HttpPollReceive(struct Http *ctx)
{
  fd_set read_fds = {0};
  struct timeval tv = {0};
  int err = 0;

  //printf("polling receive\n");

  FD_SET(ctx->sock, &read_fds);
  err = select(ctx->sock + 1, &read_fds, NULL, NULL, &tv);

  if(err == 0)
  {
    return;
  }
  else if(err == -1)
  {
    ctx->status = -1;
#ifdef USE_POSIX
    close(ctx->sock);
#endif
#ifdef USE_WINSOCK
    shutdown(ctx->sock, SD_BOTH);
    closesocket(ctx->sock);
#endif
    ctx->sock = NULL_SOCKET;
    return;
  }
  else
  {
    char buff[BUFFER_SIZE] = {0};
    size_t i = 0;
#ifdef USE_POSIX
    ssize_t n = 0;

    n = read(ctx->sock, buff, BUFFER_SIZE - 1);

    if(n > 0)
#endif
#ifdef USE_WINSOCK
    int n = 0;
    n = recv(ctx->sock, buff, BUFFER_SIZE - 1, 0);

    if(n != SOCKET_ERROR && n != 0)
#endif
    {
      for(i = 0; i < n; i++)
      {
        vector_push_back(ctx->raw, buff[i]);
      }

      //printf("Data waiting: %s\n", buff);
    }
    else
    {
#ifdef USE_POSIX
      close(ctx->sock);
#endif
#ifdef USE_WINSOCK
      shutdown(ctx->sock, SD_BOTH);
      closesocket(ctx->sock);
#endif
      ctx->sock = NULL_SOCKET;
    }

    _HttpProcessRaw(ctx);
  }
}

void _HttpPoll(struct Http *ctx)
{
  if(HttpState(ctx) == HTTP_CONNECTING)
  {
    _HttpPollConnect(ctx);
  }
  else if(HttpState(ctx) == HTTP_RECEIVING)
  {
    _HttpPollReceive(ctx);
  }
}

void _HttpParseRequest(struct Http *ctx, char *url)
{
  size_t i = 0;
  size_t len = 0;

  sstream_clear(ctx->host);
  sstream_clear(ctx->path);
  sstream_clear(ctx->query);

  len = strlen(url);

  for(; i < len - 1; i++)
  {
    if(url[i] == '/' && url[i+1] == '/')
    {
      i+=2;
      break;
    }
  }

  for(; i < len; i++)
  {
    if(url[i] == '/')
    {
      break;
    }

    sstream_push_char(ctx->host, url[i]);
  }

  for(; i < len; i++)
  {
    if(url[i] == '?')
    {
      i++;
      break;
    }

    sstream_push_char(ctx->path, url[i]);
  }

  for(; i < len; i++)
  {
    sstream_push_char(ctx->query, url[i]);
  }

  //printf("Host: %s\n", sstream_cstr(ctx->host));
  //printf("Path: %s\n", sstream_cstr(ctx->path));
  //printf("Query: %s\n", sstream_cstr(ctx->query));
}

void HttpRequest(struct Http *ctx, char *url, char *post)
{
  struct addrinfo hints = {0};
  struct addrinfo *res = NULL;
  struct addrinfo *ent = NULL;
  int err = 0;
  int flags = 0;

  if(HttpState(ctx) != HTTP_COMPLETE) return;

  sstream_clear(ctx->post);

  if(post)
  {
    sstream_push_cstr(ctx->post, post);
  }

  _HttpParseRequest(ctx, url);

  sstream_clear(ctx->rawHeaders);
  sstream_clear(ctx->rawContent);
  vector_clear(ctx->raw);
  ctx->status = 0;

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  err = getaddrinfo(sstream_cstr(ctx->host), "80", &hints, &res);

  if(err)
  {
    ctx->status = -1;
    return;
  }

  for(ent = res; ent != NULL; ent = ent->ai_next)
  {
    int sock = NULL_SOCKET;

    if(ent->ai_family != AF_INET && ent->ai_family != AF_INET6)
    {
      continue;
    }

    sock = socket(ent->ai_family, ent->ai_socktype, 0);

    if(sock == NULL_SOCKET)
    {
      continue;
    }

#ifdef USE_POSIX
    flags = fcntl(sock, F_GETFL);

    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {

      close(sock);
      continue;
    }
#endif
#ifdef USE_WINSOCK
    {
    unsigned nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
    }
#endif

    err = connect(sock, (struct sockaddr *)ent->ai_addr, ent->ai_addrlen);

#ifdef USE_POSIX
    if(err == -1)
#endif
#ifdef USE_WINSOCK
    if(err == SOCKET_ERROR)
#endif
    {
#ifdef USE_POSIX
      if(errno == EINPROGRESS)
#endif
#ifdef USE_WINSOCK
      if(WSAGetLastError() == WSAEWOULDBLOCK)
#endif
      {
        vector_push_back(ctx->socks, sock);
      }
      else
      {
#ifdef USE_POSIX
        close(sock);
#endif
#ifdef USE_WINSOCK
        shutdown(sock, SD_BOTH);
        closesocket(sock);
#endif
      }

      continue;
    }

    ctx->sock = sock;
    _HttpClearSocks(ctx);
  }

  if(ctx->sock == NULL_SOCKET && vector_size(ctx->socks) < 1)
  {
    ctx->status = -1;
  }

  freeaddrinfo(res);
}

int HttpRequestComplete(struct Http *ctx)
{
  _HttpPoll(ctx);

  if(HttpState(ctx) == HTTP_COMPLETE)
  {
    return 1;
  }

  return 0;
}

int HttpResponseStatus(struct Http *ctx)
{
  return ctx->status;
}

char *HttpResponseContent(struct Http *ctx)
{
  return sstream_cstr(ctx->rawContent);
}

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
    v = vector_at(c->documents,i)->rootVal;
    sstream_push_cstr(ser, json_serialize_to_string(v));
    if(i < vector_size(c->documents)-1)
    {
      sstream_push_char(ser, ',');
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

  // LEAK:
  pfree(doc);
}

void bgDocumentAddCStr(struct bgDocument *doc, char *path, char *val)
{
  sstream* ctx = sstream_new();
  size_t i = 0;
  vector(sstream*) *out = vector_new(sstream*); 

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

void bgDocumentAddInt(struct bgDocument *doc, char *path, int val)
{
  sstream* ctx = sstream_new();
  size_t i = 0;
  vector(sstream*) *out = vector_new(sstream*); 

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

void bgDocumentAddDouble(struct bgDocument *doc, char *path, double val)
{
  sstream* ctx = sstream_new();
  size_t i = 0;
  vector(sstream*) *out = vector_new(sstream*); 
  
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
  size_t i = 0;
  vector(sstream*) *out = vector_new(sstream*); 

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

/*
 Parson ( http://kgabis.github.com/parson/ )
 Copyright (c) 2012 - 2017 Krzysztof Gabis

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif /* _CRT_SECURE_NO_WARNINGS */
#endif /* _MSC_VER */

#ifndef AMALGAMATION
  #include "parson.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

/* Apparently sscanf is not implemented in some "standard" libraries, so don't use it, if you
 * don't have to. */
#define sscanf THINK_TWICE_ABOUT_USING_SSCANF

#define STARTING_CAPACITY         15
#define ARRAY_MAX_CAPACITY    122880 /* 15*(2^13) */
#define OBJECT_MAX_CAPACITY      960 /* 15*(2^6)  */
#define MAX_NESTING               19
#define DOUBLE_SERIALIZATION_FORMAT "%f"

#define SIZEOF_TOKEN(a)       (sizeof(a) - 1)
#define SKIP_CHAR(str)        ((*str)++)
#define SKIP_WHITESPACES(str) while (isspace(**str)) { SKIP_CHAR(str); }
#define MAX(a, b)             ((a) > (b) ? (a) : (b))

#undef malloc
#undef free

static JSON_Malloc_Function parson_malloc = malloc;
static JSON_Free_Function parson_free = free;

#define IS_CONT(b) (((unsigned char)(b) & 0xC0) == 0x80) /* is utf-8 continuation byte */

/* Type definitions */
typedef union json_value_value {
    char        *string;
    double       number;
    JSON_Object *object;
    JSON_Array  *array;
    int          boolean;
    int          null;
} JSON_Value_Value;

struct json_value_t {
    JSON_Value      *parent;
    JSON_Value_Type  type;
    JSON_Value_Value value;
};

struct json_object_t {
    JSON_Value  *wrapping_value;
    char       **names;
    JSON_Value **values;
    size_t       count;
    size_t       capacity;
};

struct json_array_t {
    JSON_Value  *wrapping_value;
    JSON_Value **items;
    size_t       count;
    size_t       capacity;
};

/* Various */
static char * read_file(const char *filename);
static void   remove_comments(char *string, const char *start_token, const char *end_token);
static char * parson_strndup(const char *string, size_t n);
static char * parson_strdup(const char *string);
static int    hex_char_to_int(char c);
static int    parse_utf16_hex(const char *string, unsigned int *result);
static int    num_bytes_in_utf8_sequence(unsigned char c);
static int    verify_utf8_sequence(const unsigned char *string, int *len);
static int    is_valid_utf8(const char *string, size_t string_len);
static int    is_decimal(const char *string, size_t length);

/* JSON Object */
static JSON_Object * json_object_init(JSON_Value *wrapping_value);
static JSON_Status   json_object_add(JSON_Object *object, const char *name, JSON_Value *value);
static JSON_Status   json_object_resize(JSON_Object *object, size_t new_capacity);
static JSON_Value  * json_object_nget_value(const JSON_Object *object, const char *name, size_t n);
static void          json_object_free(JSON_Object *object);

/* JSON Array */
static JSON_Array * json_array_init(JSON_Value *wrapping_value);
static JSON_Status  json_array_add(JSON_Array *array, JSON_Value *value);
static JSON_Status  json_array_resize(JSON_Array *array, size_t new_capacity);
static void         json_array_free(JSON_Array *array);

/* JSON Value */
static JSON_Value * json_value_init_string_no_copy(char *string);

/* Parser */
static JSON_Status  skip_quotes(const char **string);
static int          parse_utf16(const char **unprocessed, char **processed);
static char *       process_string(const char *input, size_t len);
static char *       get_quoted_string(const char **string);
static JSON_Value * parse_object_value(const char **string, size_t nesting);
static JSON_Value * parse_array_value(const char **string, size_t nesting);
static JSON_Value * parse_string_value(const char **string);
static JSON_Value * parse_boolean_value(const char **string);
static JSON_Value * parse_number_value(const char **string);
static JSON_Value * parse_null_value(const char **string);
static JSON_Value * parse_value(const char **string, size_t nesting);

/* Serialization */
static int    json_serialize_to_buffer_r(const JSON_Value *value, char *buf, int level, int is_pretty, char *num_buf);
static int    json_serialize_string(const char *string, char *buf);
static int    append_indent(char *buf, int level);
static int    append_string(char *buf, const char *string);

/* Various */
static char * parson_strndup(const char *string, size_t n) {
    char *output_string = (char*)parson_malloc(n + 1);
    if (!output_string) {
        return NULL;
    }
    output_string[n] = '\0';
    strncpy(output_string, string, n);
    return output_string;
}

static char * parson_strdup(const char *string) {
    return parson_strndup(string, strlen(string));
}

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_utf16_hex(const char *s, unsigned int *result) {
    int x1, x2, x3, x4;
    if (s[0] == '\0' || s[1] == '\0' || s[2] == '\0' || s[3] == '\0') {
        return 0;
    }
    x1 = hex_char_to_int(s[0]);
    x2 = hex_char_to_int(s[1]);
    x3 = hex_char_to_int(s[2]);
    x4 = hex_char_to_int(s[3]);
    if (x1 == -1 || x2 == -1 || x3 == -1 || x4 == -1) {
        return 0;
    }
    *result = (unsigned int)((x1 << 12) | (x2 << 8) | (x3 << 4) | x4);
    return 1;
}

static int num_bytes_in_utf8_sequence(unsigned char c) {
    if (c == 0xC0 || c == 0xC1 || c > 0xF4 || IS_CONT(c)) {
        return 0;
    } else if ((c & 0x80) == 0) {    /* 0xxxxxxx */
        return 1;
    } else if ((c & 0xE0) == 0xC0) { /* 110xxxxx */
        return 2;
    } else if ((c & 0xF0) == 0xE0) { /* 1110xxxx */
        return 3;
    } else if ((c & 0xF8) == 0xF0) { /* 11110xxx */
        return 4;
    }
    return 0; /* won't happen */
}

static int verify_utf8_sequence(const unsigned char *string, int *len) {
    unsigned int cp = 0;
    *len = num_bytes_in_utf8_sequence(string[0]);

    if (*len == 1) {
        cp = string[0];
    } else if (*len == 2 && IS_CONT(string[1])) {
        cp = string[0] & 0x1F;
        cp = (cp << 6) | (string[1] & 0x3F);
    } else if (*len == 3 && IS_CONT(string[1]) && IS_CONT(string[2])) {
        cp = ((unsigned char)string[0]) & 0xF;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
    } else if (*len == 4 && IS_CONT(string[1]) && IS_CONT(string[2]) && IS_CONT(string[3])) {
        cp = string[0] & 0x7;
        cp = (cp << 6) | (string[1] & 0x3F);
        cp = (cp << 6) | (string[2] & 0x3F);
        cp = (cp << 6) | (string[3] & 0x3F);
    } else {
        return 0;
    }

    /* overlong encodings */
    if ((cp < 0x80    && *len > 1) ||
        (cp < 0x800   && *len > 2) ||
        (cp < 0x10000 && *len > 3)) {
        return 0;
    }

    /* invalid unicode */
    if (cp > 0x10FFFF) {
        return 0;
    }

    /* surrogate halves */
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        return 0;
    }

    return 1;
}

static int is_valid_utf8(const char *string, size_t string_len) {
    int len = 0;
    const char *string_end =  string + string_len;
    while (string < string_end) {
        if (!verify_utf8_sequence((const unsigned char*)string, &len)) {
            return 0;
        }
        string += len;
    }
    return 1;
}

static int is_decimal(const char *string, size_t length) {
    if (length > 1 && string[0] == '0' && string[1] != '.') {
        return 0;
    }
    if (length > 2 && !strncmp(string, "-0", 2) && string[2] != '.') {
        return 0;
    }
    while (length--) {
        if (strchr("xX", string[length])) {
            return 0;
        }
    }
    return 1;
}

static char * read_file(const char * filename) {
    FILE *fp = fopen(filename, "r");
    size_t file_size;
    long pos;
    char *file_contents;
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    pos = ftell(fp);
    if (pos < 0) {
        fclose(fp);
        return NULL;
    }
    file_size = pos;
    rewind(fp);
    file_contents = (char*)parson_malloc(sizeof(char) * (file_size + 1));
    if (!file_contents) {
        fclose(fp);
        return NULL;
    }
    if (fread(file_contents, file_size, 1, fp) < 1) {
        if (ferror(fp)) {
            fclose(fp);
            parson_free(file_contents);
            return NULL;
        }
    }
    fclose(fp);
    file_contents[file_size] = '\0';
    return file_contents;
}

static void remove_comments(char *string, const char *start_token, const char *end_token) {
    int in_string = 0, escaped = 0;
    size_t i;
    char *ptr = NULL, current_char;
    size_t start_token_len = strlen(start_token);
    size_t end_token_len = strlen(end_token);
    if (start_token_len == 0 || end_token_len == 0) {
        return;
    }
    while ((current_char = *string) != '\0') {
        if (current_char == '\\' && !escaped) {
            escaped = 1;
            string++;
            continue;
        } else if (current_char == '\"' && !escaped) {
            in_string = !in_string;
        } else if (!in_string && strncmp(string, start_token, start_token_len) == 0) {
            for(i = 0; i < start_token_len; i++) {
                string[i] = ' ';
            }
            string = string + start_token_len;
            ptr = strstr(string, end_token);
            if (!ptr) {
                return;
            }
            for (i = 0; i < (ptr - string) + end_token_len; i++) {
                string[i] = ' ';
            }
            string = ptr + end_token_len - 1;
        }
        escaped = 0;
        string++;
    }
}

/* JSON Object */
static JSON_Object * json_object_init(JSON_Value *wrapping_value) {
    JSON_Object *new_obj = (JSON_Object*)parson_malloc(sizeof(JSON_Object));
    if (new_obj == NULL) {
        return NULL;
    }
    new_obj->wrapping_value = wrapping_value;
    new_obj->names = (char**)NULL;
    new_obj->values = (JSON_Value**)NULL;
    new_obj->capacity = 0;
    new_obj->count = 0;
    return new_obj;
}

static JSON_Status json_object_add(JSON_Object *object, const char *name, JSON_Value *value) {
    size_t index = 0;
    if (object == NULL || name == NULL || value == NULL) {
        return JSONFailure;
    }
    if (json_object_get_value(object, name) != NULL) {
        return JSONFailure;
    }
    if (object->count >= object->capacity) {
        size_t new_capacity = MAX(object->capacity * 2, STARTING_CAPACITY);
        if (new_capacity > OBJECT_MAX_CAPACITY) {
            return JSONFailure;
        }
        if (json_object_resize(object, new_capacity) == JSONFailure) {
            return JSONFailure;
        }
    }
    index = object->count;
    object->names[index] = parson_strdup(name);
    if (object->names[index] == NULL) {
        return JSONFailure;
    }
    value->parent = json_object_get_wrapping_value(object);
    object->values[index] = value;
    object->count++;
    return JSONSuccess;
}

static JSON_Status json_object_resize(JSON_Object *object, size_t new_capacity) {
    char **temp_names = NULL;
    JSON_Value **temp_values = NULL;

    if ((object->names == NULL && object->values != NULL) ||
        (object->names != NULL && object->values == NULL) ||
        new_capacity == 0) {
            return JSONFailure; /* Shouldn't happen */
    }
    temp_names = (char**)parson_malloc(new_capacity * sizeof(char*));
    if (temp_names == NULL) {
        return JSONFailure;
    }
    temp_values = (JSON_Value**)parson_malloc(new_capacity * sizeof(JSON_Value*));
    if (temp_values == NULL) {
        parson_free(temp_names);
        return JSONFailure;
    }
    if (object->names != NULL && object->values != NULL && object->count > 0) {
        memcpy(temp_names, object->names, object->count * sizeof(char*));
        memcpy(temp_values, object->values, object->count * sizeof(JSON_Value*));
    }
    parson_free(object->names);
    parson_free(object->values);
    object->names = temp_names;
    object->values = temp_values;
    object->capacity = new_capacity;
    return JSONSuccess;
}

static JSON_Value * json_object_nget_value(const JSON_Object *object, const char *name, size_t n) {
    size_t i, name_length;
    for (i = 0; i < json_object_get_count(object); i++) {
        name_length = strlen(object->names[i]);
        if (name_length != n) {
            continue;
        }
        if (strncmp(object->names[i], name, n) == 0) {
            return object->values[i];
        }
    }
    return NULL;
}

static void json_object_free(JSON_Object *object) {
    while(object->count--) {
        parson_free(object->names[object->count]);
        json_value_free(object->values[object->count]);
    }
    parson_free(object->names);
    parson_free(object->values);
    parson_free(object);
}

/* JSON Array */
static JSON_Array * json_array_init(JSON_Value *wrapping_value) {
    JSON_Array *new_array = (JSON_Array*)parson_malloc(sizeof(JSON_Array));
    if (new_array == NULL) {
        return NULL;
    }
    new_array->wrapping_value = wrapping_value;
    new_array->items = (JSON_Value**)NULL;
    new_array->capacity = 0;
    new_array->count = 0;
    return new_array;
}

static JSON_Status json_array_add(JSON_Array *array, JSON_Value *value) {
    if (array->count >= array->capacity) {
        size_t new_capacity = MAX(array->capacity * 2, STARTING_CAPACITY);
        if (new_capacity > ARRAY_MAX_CAPACITY) {
            return JSONFailure;
        }
        if (json_array_resize(array, new_capacity) == JSONFailure) {
            return JSONFailure;
        }
    }
    value->parent = json_array_get_wrapping_value(array);
    array->items[array->count] = value;
    array->count++;
    return JSONSuccess;
}

static JSON_Status json_array_resize(JSON_Array *array, size_t new_capacity) {
    JSON_Value **new_items = NULL;
    if (new_capacity == 0) {
        return JSONFailure;
    }
    new_items = (JSON_Value**)parson_malloc(new_capacity * sizeof(JSON_Value*));
    if (new_items == NULL) {
        return JSONFailure;
    }
    if (array->items != NULL && array->count > 0) {
        memcpy(new_items, array->items, array->count * sizeof(JSON_Value*));
    }
    parson_free(array->items);
    array->items = new_items;
    array->capacity = new_capacity;
    return JSONSuccess;
}

static void json_array_free(JSON_Array *array) {
    while (array->count--) {
        json_value_free(array->items[array->count]);
    }
    parson_free(array->items);
    parson_free(array);
}

/* JSON Value */
static JSON_Value * json_value_init_string_no_copy(char *string) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONString;
    new_value->value.string = string;
    return new_value;
}

/* Parser */
static JSON_Status skip_quotes(const char **string) {
    if (**string != '\"') {
        return JSONFailure;
    }
    SKIP_CHAR(string);
    while (**string != '\"') {
        if (**string == '\0') {
            return JSONFailure;
        } else if (**string == '\\') {
            SKIP_CHAR(string);
            if (**string == '\0') {
                return JSONFailure;
            }
        }
        SKIP_CHAR(string);
    }
    SKIP_CHAR(string);
    return JSONSuccess;
}

static int parse_utf16(const char **unprocessed, char **processed) {
    unsigned int cp, lead, trail;
    int parse_succeeded = 0;
    char *processed_ptr = *processed;
    const char *unprocessed_ptr = *unprocessed;
    unprocessed_ptr++; /* skips u */
    parse_succeeded = parse_utf16_hex(unprocessed_ptr, &cp);
    if (!parse_succeeded) {
        return JSONFailure;
    }
    if (cp < 0x80) {
        *processed_ptr = (char)cp; /* 0xxxxxxx */
    } else if (cp < 0x800) {
        *processed_ptr++ = ((cp >> 6) & 0x1F) | 0xC0; /* 110xxxxx */
        *processed_ptr   = ((cp     ) & 0x3F) | 0x80; /* 10xxxxxx */
    } else if (cp < 0xD800 || cp > 0xDFFF) {
        *processed_ptr++ = ((cp >> 12) & 0x0F) | 0xE0; /* 1110xxxx */
        *processed_ptr++ = ((cp >> 6)  & 0x3F) | 0x80; /* 10xxxxxx */
        *processed_ptr   = ((cp     )  & 0x3F) | 0x80; /* 10xxxxxx */
    } else if (cp >= 0xD800 && cp <= 0xDBFF) { /* lead surrogate (0xD800..0xDBFF) */
        lead = cp;
        unprocessed_ptr += 4; /* should always be within the buffer, otherwise previous sscanf would fail */
        if (*unprocessed_ptr++ != '\\' || *unprocessed_ptr++ != 'u') {
            return JSONFailure;
        }
        parse_succeeded = parse_utf16_hex(unprocessed_ptr, &trail);
        if (!parse_succeeded || trail < 0xDC00 || trail > 0xDFFF) { /* valid trail surrogate? (0xDC00..0xDFFF) */
            return JSONFailure;
        }
        cp = ((((lead-0xD800)&0x3FF)<<10)|((trail-0xDC00)&0x3FF))+0x010000;
        *processed_ptr++ = (((cp >> 18) & 0x07) | 0xF0); /* 11110xxx */
        *processed_ptr++ = (((cp >> 12) & 0x3F) | 0x80); /* 10xxxxxx */
        *processed_ptr++ = (((cp >> 6)  & 0x3F) | 0x80); /* 10xxxxxx */
        *processed_ptr   = (((cp     )  & 0x3F) | 0x80); /* 10xxxxxx */
    } else { /* trail surrogate before lead surrogate */
        return JSONFailure;
    }
    unprocessed_ptr += 3;
    *processed = processed_ptr;
    *unprocessed = unprocessed_ptr;
    return JSONSuccess;
}


/* Copies and processes passed string up to supplied length.
Example: "\u006Corem ipsum" -> lorem ipsum */
static char* process_string(const char *input, size_t len) {
    const char *input_ptr = input;
    size_t initial_size = (len + 1) * sizeof(char);
    size_t final_size = 0;
    char *output = (char*)parson_malloc(initial_size);
    char *output_ptr = output;
    char *resized_output = NULL;
    while ((*input_ptr != '\0') && (size_t)(input_ptr - input) < len) {
        if (*input_ptr == '\\') {
            input_ptr++;
            switch (*input_ptr) {
                case '\"': *output_ptr = '\"'; break;
                case '\\': *output_ptr = '\\'; break;
                case '/':  *output_ptr = '/';  break;
                case 'b':  *output_ptr = '\b'; break;
                case 'f':  *output_ptr = '\f'; break;
                case 'n':  *output_ptr = '\n'; break;
                case 'r':  *output_ptr = '\r'; break;
                case 't':  *output_ptr = '\t'; break;
                case 'u':
                    if (parse_utf16(&input_ptr, &output_ptr) == JSONFailure) {
                        goto error;
                    }
                    break;
                default:
                    goto error;
            }
        } else if ((unsigned char)*input_ptr < 0x20) {
            goto error; /* 0x00-0x19 are invalid characters for json string (http://www.ietf.org/rfc/rfc4627.txt) */
        } else {
            *output_ptr = *input_ptr;
        }
        output_ptr++;
        input_ptr++;
    }
    *output_ptr = '\0';
    /* resize to new length */
    final_size = (size_t)(output_ptr-output) + 1;
    /* todo: don't resize if final_size == initial_size */
    resized_output = (char*)parson_malloc(final_size);
    if (resized_output == NULL) {
        goto error;
    }
    memcpy(resized_output, output, final_size);
    parson_free(output);
    return resized_output;
error:
    parson_free(output);
    return NULL;
}

/* Return processed contents of a string between quotes and
   skips passed argument to a matching quote. */
static char * get_quoted_string(const char **string) {
    const char *string_start = *string;
    size_t string_len = 0;
    JSON_Status status = skip_quotes(string);
    if (status != JSONSuccess) {
        return NULL;
    }
    string_len = *string - string_start - 2; /* length without quotes */
    return process_string(string_start + 1, string_len);
}

static JSON_Value * parse_value(const char **string, size_t nesting) {
    if (nesting > MAX_NESTING) {
        return NULL;
    }
    SKIP_WHITESPACES(string);
    switch (**string) {
        case '{':
            return parse_object_value(string, nesting + 1);
        case '[':
            return parse_array_value(string, nesting + 1);
        case '\"':
            return parse_string_value(string);
        case 'f': case 't':
            return parse_boolean_value(string);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number_value(string);
        case 'n':
            return parse_null_value(string);
        default:
            return NULL;
    }
}

static JSON_Value * parse_object_value(const char **string, size_t nesting) {
    JSON_Value *output_value = json_value_init_object(), *new_value = NULL;
    JSON_Object *output_object = json_value_get_object(output_value);
    char *new_key = NULL;
    if (output_value == NULL || **string != '{') {
        return NULL;
    }
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == '}') { /* empty object */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        new_key = get_quoted_string(string);
        SKIP_WHITESPACES(string);
        if (new_key == NULL || **string != ':') {
            json_value_free(output_value);
            return NULL;
        }
        SKIP_CHAR(string);
        new_value = parse_value(string, nesting);
        if (new_value == NULL) {
            parson_free(new_key);
            json_value_free(output_value);
            return NULL;
        }
        if(json_object_add(output_object, new_key, new_value) == JSONFailure) {
            parson_free(new_key);
            parson_free(new_value);
            json_value_free(output_value);
            return NULL;
        }
        parson_free(new_key);
        SKIP_WHITESPACES(string);
        if (**string != ',') {
            break;
        }
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
    }
    SKIP_WHITESPACES(string);
    if (**string != '}' || /* Trim object after parsing is over */
        json_object_resize(output_object, json_object_get_count(output_object)) == JSONFailure) {
            json_value_free(output_value);
            return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static JSON_Value * parse_array_value(const char **string, size_t nesting) {
    JSON_Value *output_value = json_value_init_array(), *new_array_value = NULL;
    JSON_Array *output_array = json_value_get_array(output_value);
    if (!output_value || **string != '[') {
        return NULL;
    }
    SKIP_CHAR(string);
    SKIP_WHITESPACES(string);
    if (**string == ']') { /* empty array */
        SKIP_CHAR(string);
        return output_value;
    }
    while (**string != '\0') {
        new_array_value = parse_value(string, nesting);
        if (!new_array_value) {
            json_value_free(output_value);
            return NULL;
        }
        if (json_array_add(output_array, new_array_value) == JSONFailure) {
            parson_free(new_array_value);
            json_value_free(output_value);
            return NULL;
        }
        SKIP_WHITESPACES(string);
        if (**string != ',') {
            break;
        }
        SKIP_CHAR(string);
        SKIP_WHITESPACES(string);
    }
    SKIP_WHITESPACES(string);
    if (**string != ']' || /* Trim array after parsing is over */
        json_array_resize(output_array, json_array_get_count(output_array)) == JSONFailure) {
            json_value_free(output_value);
            return NULL;
    }
    SKIP_CHAR(string);
    return output_value;
}

static JSON_Value * parse_string_value(const char **string) {
    JSON_Value *value = NULL;
    char *new_string = get_quoted_string(string);
    if (new_string == NULL) {
        return NULL;
    }
    value = json_value_init_string_no_copy(new_string);
    if (value == NULL) {
        parson_free(new_string);
        return NULL;
    }
    return value;
}

static JSON_Value * parse_boolean_value(const char **string) {
    size_t true_token_size = SIZEOF_TOKEN("true");
    size_t false_token_size = SIZEOF_TOKEN("false");
    if (strncmp("true", *string, true_token_size) == 0) {
        *string += true_token_size;
        return json_value_init_boolean(1);
    } else if (strncmp("false", *string, false_token_size) == 0) {
        *string += false_token_size;
        return json_value_init_boolean(0);
    }
    return NULL;
}

static JSON_Value * parse_number_value(const char **string) {
    char *end;
    double number = 0;
    errno = 0;
    number = strtod(*string, &end);
    if (errno || !is_decimal(*string, end - *string)) {
        return NULL;
    }
    *string = end;
    return json_value_init_number(number);
}

static JSON_Value * parse_null_value(const char **string) {
    size_t token_size = SIZEOF_TOKEN("null");
    if (strncmp("null", *string, token_size) == 0) {
        *string += token_size;
        return json_value_init_null();
    }
    return NULL;
}

/* Serialization */
#define APPEND_STRING(str) do { written = append_string(buf, (str));\
                                if (written < 0) { return -1; }\
                                if (buf != NULL) { buf += written; }\
                                written_total += written; } while(0)

#define APPEND_INDENT(level) do { written = append_indent(buf, (level));\
                                  if (written < 0) { return -1; }\
                                  if (buf != NULL) { buf += written; }\
                                  written_total += written; } while(0)

static int json_serialize_to_buffer_r(const JSON_Value *value, char *buf, int level, int is_pretty, char *num_buf)
{
    const char *key = NULL, *string = NULL;
    JSON_Value *temp_value = NULL;
    JSON_Array *array = NULL;
    JSON_Object *object = NULL;
    size_t i = 0, count = 0;
    double num = 0.0;
    int written = -1, written_total = 0;

    switch (json_value_get_type(value)) {
        case JSONArray:
            array = json_value_get_array(value);
            count = json_array_get_count(array);
            APPEND_STRING("[");
            if (count > 0 && is_pretty) {
                APPEND_STRING("\n");
            }
            for (i = 0; i < count; i++) {
                if (is_pretty) {
                    APPEND_INDENT(level+1);
                }
                temp_value = json_array_get_value(array, i);
                written = json_serialize_to_buffer_r(temp_value, buf, level+1, is_pretty, num_buf);
                if (written < 0) {
                    return -1;
                }
                if (buf != NULL) {
                    buf += written;
                }
                written_total += written;
                if (i < (count - 1)) {
                    APPEND_STRING(",");
                }
                if (is_pretty) {
                    APPEND_STRING("\n");
                }
            }
            if (count > 0 && is_pretty) {
                APPEND_INDENT(level);
            }
            APPEND_STRING("]");
            return written_total;
        case JSONObject:
            object = json_value_get_object(value);
            count  = json_object_get_count(object);
            APPEND_STRING("{");
            if (count > 0 && is_pretty) {
                APPEND_STRING("\n");
            }
            for (i = 0; i < count; i++) {
                key = json_object_get_name(object, i);
                if (key == NULL) {
                    return -1;
                }
                if (is_pretty) {
                    APPEND_INDENT(level+1);
                }
                written = json_serialize_string(key, buf);
                if (written < 0) {
                    return -1;
                }
                if (buf != NULL) {
                    buf += written;
                }
                written_total += written;
                APPEND_STRING(":");
                if (is_pretty) {
                    APPEND_STRING(" ");
                }
                temp_value = json_object_get_value(object, key);
                written = json_serialize_to_buffer_r(temp_value, buf, level+1, is_pretty, num_buf);
                if (written < 0) {
                    return -1;
                }
                if (buf != NULL) {
                    buf += written;
                }
                written_total += written;
                if (i < (count - 1)) {
                    APPEND_STRING(",");
                }
                if (is_pretty) {
                    APPEND_STRING("\n");
                }
            }
            if (count > 0 && is_pretty) {
                APPEND_INDENT(level);
            }
            APPEND_STRING("}");
            return written_total;
        case JSONString:
            string = json_value_get_string(value);
            if (string == NULL) {
                return -1;
            }
            written = json_serialize_string(string, buf);
            if (written < 0) {
                return -1;
            }
            if (buf != NULL) {
                buf += written;
            }
            written_total += written;
            return written_total;
        case JSONBoolean:
            if (json_value_get_boolean(value)) {
                APPEND_STRING("true");
            } else {
                APPEND_STRING("false");
            }
            return written_total;
        case JSONNumber:
            num = json_value_get_number(value);
            if (buf != NULL) {
                num_buf = buf;
            }
            if (num == ((double)(int)num)) { /*  check if num is integer */
                written = sprintf(num_buf, "%d", (int)num);
	    } else if (num == ((double)(unsigned int)num)) {
                written = sprintf(num_buf, "%u", (unsigned int)num);
            } else {
                written = sprintf(num_buf, DOUBLE_SERIALIZATION_FORMAT, num);
            }
            if (written < 0) {
                return -1;
            }
            if (buf != NULL) {
                buf += written;
            }
            written_total += written;
            return written_total;
        case JSONNull:
            APPEND_STRING("null");
            return written_total;
        case JSONError:
            return -1;
        default:
            return -1;
    }
}

static int json_serialize_string(const char *string, char *buf) {
    size_t i = 0, len = strlen(string);
    char c = '\0';
    int written = -1, written_total = 0;
    APPEND_STRING("\"");
    for (i = 0; i < len; i++) {
        c = string[i];
        switch (c) {
            case '\"': APPEND_STRING("\\\""); break;
            case '\\': APPEND_STRING("\\\\"); break;
            case '/':  APPEND_STRING("\\/"); break; /* to make json embeddable in xml\/html */
            case '\b': APPEND_STRING("\\b"); break;
            case '\f': APPEND_STRING("\\f"); break;
            case '\n': APPEND_STRING("\\n"); break;
            case '\r': APPEND_STRING("\\r"); break;
            case '\t': APPEND_STRING("\\t"); break;
            case '\x00': APPEND_STRING("\\u0000"); break;
            case '\x01': APPEND_STRING("\\u0001"); break;
            case '\x02': APPEND_STRING("\\u0002"); break;
            case '\x03': APPEND_STRING("\\u0003"); break;
            case '\x04': APPEND_STRING("\\u0004"); break;
            case '\x05': APPEND_STRING("\\u0005"); break;
            case '\x06': APPEND_STRING("\\u0006"); break;
            case '\x07': APPEND_STRING("\\u0007"); break;
            /* '\x08' duplicate: '\b' */
            /* '\x09' duplicate: '\t' */
            /* '\x0a' duplicate: '\n' */
            case '\x0b': APPEND_STRING("\\u000b"); break;
            /* '\x0c' duplicate: '\f' */
            /* '\x0d' duplicate: '\r' */
            case '\x0e': APPEND_STRING("\\u000e"); break;
            case '\x0f': APPEND_STRING("\\u000f"); break;
            case '\x10': APPEND_STRING("\\u0010"); break;
            case '\x11': APPEND_STRING("\\u0011"); break;
            case '\x12': APPEND_STRING("\\u0012"); break;
            case '\x13': APPEND_STRING("\\u0013"); break;
            case '\x14': APPEND_STRING("\\u0014"); break;
            case '\x15': APPEND_STRING("\\u0015"); break;
            case '\x16': APPEND_STRING("\\u0016"); break;
            case '\x17': APPEND_STRING("\\u0017"); break;
            case '\x18': APPEND_STRING("\\u0018"); break;
            case '\x19': APPEND_STRING("\\u0019"); break;
            case '\x1a': APPEND_STRING("\\u001a"); break;
            case '\x1b': APPEND_STRING("\\u001b"); break;
            case '\x1c': APPEND_STRING("\\u001c"); break;
            case '\x1d': APPEND_STRING("\\u001d"); break;
            case '\x1e': APPEND_STRING("\\u001e"); break;
            case '\x1f': APPEND_STRING("\\u001f"); break;
            default:
                if (buf != NULL) {
                    buf[0] = c;
                    buf += 1;
                }
                written_total += 1;
                break;
        }
    }
    APPEND_STRING("\"");
    return written_total;
}

static int append_indent(char *buf, int level) {
    int i;
    int written = -1, written_total = 0;
    for (i = 0; i < level; i++) {
        APPEND_STRING("    ");
    }
    return written_total;
}

static int append_string(char *buf, const char *string) {
    if (buf == NULL) {
        return (int)strlen(string);
    }
    return sprintf(buf, "%s", string);
}

#undef APPEND_STRING
#undef APPEND_INDENT

/* Parser API */
JSON_Value * json_parse_file(const char *filename) {
    char *file_contents = read_file(filename);
    JSON_Value *output_value = NULL;
    if (file_contents == NULL) {
        return NULL;
    }
    output_value = json_parse_string(file_contents);
    parson_free(file_contents);
    return output_value;
}

JSON_Value * json_parse_file_with_comments(const char *filename) {
    char *file_contents = read_file(filename);
    JSON_Value *output_value = NULL;
    if (file_contents == NULL) {
        return NULL;
    }
    output_value = json_parse_string_with_comments(file_contents);
    parson_free(file_contents);
    return output_value;
}

JSON_Value * json_parse_string(const char *string) {
    if (string == NULL) {
        return NULL;
    }
    if (string[0] == '\xEF' && string[1] == '\xBB' && string[2] == '\xBF') {
        string = string + 3; /* Support for UTF-8 BOM */
    }
    return parse_value((const char**)&string, 0);
}

JSON_Value * json_parse_string_with_comments(const char *string) {
    JSON_Value *result = NULL;
    char *string_mutable_copy = NULL, *string_mutable_copy_ptr = NULL;
    string_mutable_copy = parson_strdup(string);
    if (string_mutable_copy == NULL) {
        return NULL;
    }
    remove_comments(string_mutable_copy, "/*", "*/");
    remove_comments(string_mutable_copy, "//", "\n");
    string_mutable_copy_ptr = string_mutable_copy;
    result = parse_value((const char**)&string_mutable_copy_ptr, 0);
    parson_free(string_mutable_copy);
    return result;
}

/* JSON Object API */

JSON_Value * json_object_get_value(const JSON_Object *object, const char *name) {
    if (object == NULL || name == NULL) {
        return NULL;
    }
    return json_object_nget_value(object, name, strlen(name));
}

const char * json_object_get_string(const JSON_Object *object, const char *name) {
    return json_value_get_string(json_object_get_value(object, name));
}

double json_object_get_number(const JSON_Object *object, const char *name) {
    return json_value_get_number(json_object_get_value(object, name));
}

JSON_Object * json_object_get_object(const JSON_Object *object, const char *name) {
    return json_value_get_object(json_object_get_value(object, name));
}

JSON_Array * json_object_get_array(const JSON_Object *object, const char *name) {
    return json_value_get_array(json_object_get_value(object, name));
}

int json_object_get_boolean(const JSON_Object *object, const char *name) {
    return json_value_get_boolean(json_object_get_value(object, name));
}

JSON_Value * json_object_dotget_value(const JSON_Object *object, const char *name) {
    const char *dot_position = strchr(name, '.');
    if (!dot_position) {
        return json_object_get_value(object, name);
    }
    object = json_value_get_object(json_object_nget_value(object, name, dot_position - name));
    return json_object_dotget_value(object, dot_position + 1);
}

const char * json_object_dotget_string(const JSON_Object *object, const char *name) {
    return json_value_get_string(json_object_dotget_value(object, name));
}

double json_object_dotget_number(const JSON_Object *object, const char *name) {
    return json_value_get_number(json_object_dotget_value(object, name));
}

JSON_Object * json_object_dotget_object(const JSON_Object *object, const char *name) {
    return json_value_get_object(json_object_dotget_value(object, name));
}

JSON_Array * json_object_dotget_array(const JSON_Object *object, const char *name) {
    return json_value_get_array(json_object_dotget_value(object, name));
}

int json_object_dotget_boolean(const JSON_Object *object, const char *name) {
    return json_value_get_boolean(json_object_dotget_value(object, name));
}

size_t json_object_get_count(const JSON_Object *object) {
    return object ? object->count : 0;
}

const char * json_object_get_name(const JSON_Object *object, size_t index) {
    if (object == NULL || index >= json_object_get_count(object)) {
        return NULL;
    }
    return object->names[index];
}

JSON_Value * json_object_get_value_at(const JSON_Object *object, size_t index) {
    if (object == NULL || index >= json_object_get_count(object)) {
        return NULL;
    }
    return object->values[index];
}

JSON_Value *json_object_get_wrapping_value(const JSON_Object *object) {
    return object->wrapping_value;
}

int json_object_has_value (const JSON_Object *object, const char *name) {
    return json_object_get_value(object, name) != NULL;
}

int json_object_has_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type) {
    JSON_Value *val = json_object_get_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

int json_object_dothas_value (const JSON_Object *object, const char *name) {
    return json_object_dotget_value(object, name) != NULL;
}

int json_object_dothas_value_of_type(const JSON_Object *object, const char *name, JSON_Value_Type type) {
    JSON_Value *val = json_object_dotget_value(object, name);
    return val != NULL && json_value_get_type(val) == type;
}

/* JSON Array API */
JSON_Value * json_array_get_value(const JSON_Array *array, size_t index) {
    if (array == NULL || index >= json_array_get_count(array)) {
        return NULL;
    }
    return array->items[index];
}

const char * json_array_get_string(const JSON_Array *array, size_t index) {
    return json_value_get_string(json_array_get_value(array, index));
}

double json_array_get_number(const JSON_Array *array, size_t index) {
    return json_value_get_number(json_array_get_value(array, index));
}

JSON_Object * json_array_get_object(const JSON_Array *array, size_t index) {
    return json_value_get_object(json_array_get_value(array, index));
}

JSON_Array * json_array_get_array(const JSON_Array *array, size_t index) {
    return json_value_get_array(json_array_get_value(array, index));
}

int json_array_get_boolean(const JSON_Array *array, size_t index) {
    return json_value_get_boolean(json_array_get_value(array, index));
}

size_t json_array_get_count(const JSON_Array *array) {
    return array ? array->count : 0;
}

JSON_Value * json_array_get_wrapping_value(const JSON_Array *array) {
    return array->wrapping_value;
}

/* JSON Value API */
JSON_Value_Type json_value_get_type(const JSON_Value *value) {
    return value ? value->type : JSONError;
}

JSON_Object * json_value_get_object(const JSON_Value *value) {
    return json_value_get_type(value) == JSONObject ? value->value.object : NULL;
}

JSON_Array * json_value_get_array(const JSON_Value *value) {
    return json_value_get_type(value) == JSONArray ? value->value.array : NULL;
}

const char * json_value_get_string(const JSON_Value *value) {
    return json_value_get_type(value) == JSONString ? value->value.string : NULL;
}

double json_value_get_number(const JSON_Value *value) {
    return json_value_get_type(value) == JSONNumber ? value->value.number : 0;
}

int json_value_get_boolean(const JSON_Value *value) {
    return json_value_get_type(value) == JSONBoolean ? value->value.boolean : -1;
}

JSON_Value * json_value_get_parent (const JSON_Value *value) {
    return value ? value->parent : NULL;
}

void json_value_free(JSON_Value *value) {
    switch (json_value_get_type(value)) {
        case JSONObject:
            json_object_free(value->value.object);
            break;
        case JSONString:
            parson_free(value->value.string);
            break;
        case JSONArray:
            json_array_free(value->value.array);
            break;
        default:
            break;
    }
    parson_free(value);
}

JSON_Value * json_value_init_object(void) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONObject;
    new_value->value.object = json_object_init(new_value);
    if (!new_value->value.object) {
        parson_free(new_value);
        return NULL;
    }
    return new_value;
}

JSON_Value * json_value_init_array(void) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONArray;
    new_value->value.array = json_array_init(new_value);
    if (!new_value->value.array) {
        parson_free(new_value);
        return NULL;
    }
    return new_value;
}

JSON_Value * json_value_init_string(const char *string) {
    char *copy = NULL;
    JSON_Value *value;
    size_t string_len = 0;
    if (string == NULL) {
        return NULL;
    }
    string_len = strlen(string);
    if (!is_valid_utf8(string, string_len)) {
        return NULL;
    }
    copy = parson_strndup(string, string_len);
    if (copy == NULL) {
        return NULL;
    }
    value = json_value_init_string_no_copy(copy);
    if (value == NULL) {
        parson_free(copy);
    }
    return value;
}

JSON_Value * json_value_init_number(double number) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONNumber;
    new_value->value.number = number;
    return new_value;
}

JSON_Value * json_value_init_boolean(int boolean) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONBoolean;
    new_value->value.boolean = boolean ? 1 : 0;
    return new_value;
}

JSON_Value * json_value_init_null(void) {
    JSON_Value *new_value = (JSON_Value*)parson_malloc(sizeof(JSON_Value));
    if (!new_value) {
        return NULL;
    }
    new_value->parent = NULL;
    new_value->type = JSONNull;
    return new_value;
}

JSON_Value * json_value_deep_copy(const JSON_Value *value) {
    size_t i = 0;
    JSON_Value *return_value = NULL, *temp_value_copy = NULL, *temp_value = NULL;
    const char *temp_string = NULL, *temp_key = NULL;
    char *temp_string_copy = NULL;
    JSON_Array *temp_array = NULL, *temp_array_copy = NULL;
    JSON_Object *temp_object = NULL, *temp_object_copy = NULL;

    switch (json_value_get_type(value)) {
        case JSONArray:
            temp_array = json_value_get_array(value);
            return_value = json_value_init_array();
            if (return_value == NULL) {
                return NULL;
            }
            temp_array_copy = json_value_get_array(return_value);
            for (i = 0; i < json_array_get_count(temp_array); i++) {
                temp_value = json_array_get_value(temp_array, i);
                temp_value_copy = json_value_deep_copy(temp_value);
                if (temp_value_copy == NULL) {
                    json_value_free(return_value);
                    return NULL;
                }
                if (json_array_add(temp_array_copy, temp_value_copy) == JSONFailure) {
                    json_value_free(return_value);
                    json_value_free(temp_value_copy);
                    return NULL;
                }
            }
            return return_value;
        case JSONObject:
            temp_object = json_value_get_object(value);
            return_value = json_value_init_object();
            if (return_value == NULL) {
                return NULL;
            }
            temp_object_copy = json_value_get_object(return_value);
            for (i = 0; i < json_object_get_count(temp_object); i++) {
                temp_key = json_object_get_name(temp_object, i);
                temp_value = json_object_get_value(temp_object, temp_key);
                temp_value_copy = json_value_deep_copy(temp_value);
                if (temp_value_copy == NULL) {
                    json_value_free(return_value);
                    return NULL;
                }
                if (json_object_add(temp_object_copy, temp_key, temp_value_copy) == JSONFailure) {
                    json_value_free(return_value);
                    json_value_free(temp_value_copy);
                    return NULL;
                }
            }
            return return_value;
        case JSONBoolean:
            return json_value_init_boolean(json_value_get_boolean(value));
        case JSONNumber:
            return json_value_init_number(json_value_get_number(value));
        case JSONString:
            temp_string = json_value_get_string(value);
            if (temp_string == NULL) {
                return NULL;
            }
            temp_string_copy = parson_strdup(temp_string);
            if (temp_string_copy == NULL) {
                return NULL;
            }
            return_value = json_value_init_string_no_copy(temp_string_copy);
            if (return_value == NULL) {
                parson_free(temp_string_copy);
            }
            return return_value;
        case JSONNull:
            return json_value_init_null();
        case JSONError:
            return NULL;
        default:
            return NULL;
    }
}

size_t json_serialization_size(const JSON_Value *value) {
    char num_buf[1100]; /* recursively allocating buffer on stack is a bad idea, so let's do it only once */
    int res = json_serialize_to_buffer_r(value, NULL, 0, 0, num_buf);
    return res < 0 ? 0 : (size_t)(res + 1);
}

JSON_Status json_serialize_to_buffer(const JSON_Value *value, char *buf, size_t buf_size_in_bytes) {
    int written = -1;
    size_t needed_size_in_bytes = json_serialization_size(value);
    if (needed_size_in_bytes == 0 || buf_size_in_bytes < needed_size_in_bytes) {
        return JSONFailure;
    }
    written = json_serialize_to_buffer_r(value, buf, 0, 0, NULL);
    if (written < 0) {
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_serialize_to_file(const JSON_Value *value, const char *filename) {
    JSON_Status return_code = JSONSuccess;
    FILE *fp = NULL;
    char *serialized_string = json_serialize_to_string(value);
    if (serialized_string == NULL) {
        return JSONFailure;
    }
    fp = fopen (filename, "w");
    if (fp == NULL) {
        json_free_serialized_string(serialized_string);
        return JSONFailure;
    }
    if (fputs(serialized_string, fp) == EOF) {
        return_code = JSONFailure;
    }
    if (fclose(fp) == EOF) {
        return_code = JSONFailure;
    }
    json_free_serialized_string(serialized_string);
    return return_code;
}

char * json_serialize_to_string(const JSON_Value *value) {
    JSON_Status serialization_result = JSONFailure;
    size_t buf_size_bytes = json_serialization_size(value);
    char *buf = NULL;
    if (buf_size_bytes == 0) {
        return NULL;
    }
    buf = (char*)parson_malloc(buf_size_bytes);
    if (buf == NULL) {
        return NULL;
    }
    serialization_result = json_serialize_to_buffer(value, buf, buf_size_bytes);
    if (serialization_result == JSONFailure) {
        json_free_serialized_string(buf);
        return NULL;
    }
    return buf;
}

size_t json_serialization_size_pretty(const JSON_Value *value) {
    char num_buf[1100]; /* recursively allocating buffer on stack is a bad idea, so let's do it only once */
    int res = json_serialize_to_buffer_r(value, NULL, 0, 1, num_buf);
    return res < 0 ? 0 : (size_t)(res + 1);
}

JSON_Status json_serialize_to_buffer_pretty(const JSON_Value *value, char *buf, size_t buf_size_in_bytes) {
    int written = -1;
    size_t needed_size_in_bytes = json_serialization_size_pretty(value);
    if (needed_size_in_bytes == 0 || buf_size_in_bytes < needed_size_in_bytes) {
        return JSONFailure;
    }
    written = json_serialize_to_buffer_r(value, buf, 0, 1, NULL);
    if (written < 0) {
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_serialize_to_file_pretty(const JSON_Value *value, const char *filename) {
    JSON_Status return_code = JSONSuccess;
    FILE *fp = NULL;
    char *serialized_string = json_serialize_to_string_pretty(value);
    if (serialized_string == NULL) {
        return JSONFailure;
    }
    fp = fopen (filename, "w");
    if (fp == NULL) {
        json_free_serialized_string(serialized_string);
        return JSONFailure;
    }
    if (fputs(serialized_string, fp) == EOF) {
        return_code = JSONFailure;
    }
    if (fclose(fp) == EOF) {
        return_code = JSONFailure;
    }
    json_free_serialized_string(serialized_string);
    return return_code;
}

char * json_serialize_to_string_pretty(const JSON_Value *value) {
    JSON_Status serialization_result = JSONFailure;
    size_t buf_size_bytes = json_serialization_size_pretty(value);
    char *buf = NULL;
    if (buf_size_bytes == 0) {
        return NULL;
    }
    buf = (char*)parson_malloc(buf_size_bytes);
    if (buf == NULL) {
        return NULL;
    }
    serialization_result = json_serialize_to_buffer_pretty(value, buf, buf_size_bytes);
    if (serialization_result == JSONFailure) {
        json_free_serialized_string(buf);
        return NULL;
    }
    return buf;
}

void json_free_serialized_string(char *string) {
    parson_free(string);
}

JSON_Status json_array_remove(JSON_Array *array, size_t ix) {
    JSON_Value *temp_value = NULL;
    size_t last_element_ix = 0;
    if (array == NULL || ix >= json_array_get_count(array)) {
        return JSONFailure;
    }
    last_element_ix = json_array_get_count(array) - 1;
    json_value_free(json_array_get_value(array, ix));
    if (ix != last_element_ix) { /* Replace value with one from the end of array */
        temp_value = json_array_get_value(array, last_element_ix);
        if (temp_value == NULL) {
            return JSONFailure;
        }
        array->items[ix] = temp_value;
    }
    array->count -= 1;
    return JSONSuccess;
}

JSON_Status json_array_replace_value(JSON_Array *array, size_t ix, JSON_Value *value) {
    if (array == NULL || value == NULL || value->parent != NULL || ix >= json_array_get_count(array)) {
        return JSONFailure;
    }
    json_value_free(json_array_get_value(array, ix));
    value->parent = json_array_get_wrapping_value(array);
    array->items[ix] = value;
    return JSONSuccess;
}

JSON_Status json_array_replace_string(JSON_Array *array, size_t i, const char* string) {
    JSON_Value *value = json_value_init_string(string);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_number(JSON_Array *array, size_t i, double number) {
    JSON_Value *value = json_value_init_number(number);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_boolean(JSON_Array *array, size_t i, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_replace_null(JSON_Array *array, size_t i) {
    JSON_Value *value = json_value_init_null();
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_replace_value(array, i, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_clear(JSON_Array *array) {
    size_t i = 0;
    if (array == NULL) {
        return JSONFailure;
    }
    for (i = 0; i < json_array_get_count(array); i++) {
        json_value_free(json_array_get_value(array, i));
    }
    array->count = 0;
    return JSONSuccess;
}

JSON_Status json_array_append_value(JSON_Array *array, JSON_Value *value) {
    if (array == NULL || value == NULL || value->parent != NULL) {
        return JSONFailure;
    }
    return json_array_add(array, value);
}

JSON_Status json_array_append_string(JSON_Array *array, const char *string) {
    JSON_Value *value = json_value_init_string(string);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_number(JSON_Array *array, double number) {
    JSON_Value *value = json_value_init_number(number);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_boolean(JSON_Array *array, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_array_append_null(JSON_Array *array) {
    JSON_Value *value = json_value_init_null();
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_array_append_value(array, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_object_set_value(JSON_Object *object, const char *name, JSON_Value *value) {
    size_t i = 0;
    JSON_Value *old_value;
    if (object == NULL || name == NULL || value == NULL || value->parent != NULL) {
        return JSONFailure;
    }
    old_value = json_object_get_value(object, name);
    if (old_value != NULL) { /* free and overwrite old value */
        json_value_free(old_value);
        for (i = 0; i < json_object_get_count(object); i++) {
            if (strcmp(object->names[i], name) == 0) {
                value->parent = json_object_get_wrapping_value(object);
                object->values[i] = value;
                return JSONSuccess;
            }
        }
    }
    /* add new key value pair */
    return json_object_add(object, name, value);
}

JSON_Status json_object_set_string(JSON_Object *object, const char *name, const char *string) {
    return json_object_set_value(object, name, json_value_init_string(string));
}

JSON_Status json_object_set_number(JSON_Object *object, const char *name, double number) {
    return json_object_set_value(object, name, json_value_init_number(number));
}

JSON_Status json_object_set_boolean(JSON_Object *object, const char *name, int boolean) {
    return json_object_set_value(object, name, json_value_init_boolean(boolean));
}

JSON_Status json_object_set_null(JSON_Object *object, const char *name) {
    return json_object_set_value(object, name, json_value_init_null());
}

JSON_Status json_object_dotset_value(JSON_Object *object, const char *name, JSON_Value *value) {
    const char *dot_pos = NULL;
    char *current_name = NULL;
    JSON_Object *temp_obj = NULL;
    JSON_Value *new_value = NULL;
    if (value == NULL || name == NULL || value == NULL) {
        return JSONFailure;
    }
    dot_pos = strchr(name, '.');
    if (dot_pos == NULL) {
        return json_object_set_value(object, name, value);
    } else {
        current_name = parson_strndup(name, dot_pos - name);
        temp_obj = json_object_get_object(object, current_name);
        if (temp_obj == NULL) {
            new_value = json_value_init_object();
            if (new_value == NULL) {
                parson_free(current_name);
                return JSONFailure;
            }
            if (json_object_add(object, current_name, new_value) == JSONFailure) {
                json_value_free(new_value);
                parson_free(current_name);
                return JSONFailure;
            }
            temp_obj = json_object_get_object(object, current_name);
        }
        parson_free(current_name);
        return json_object_dotset_value(temp_obj, dot_pos + 1, value);
    }
}

JSON_Status json_object_dotset_string(JSON_Object *object, const char *name, const char *string) {
    JSON_Value *value = json_value_init_string(string);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_object_dotset_value(object, name, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_object_dotset_number(JSON_Object *object, const char *name, double number) {
    JSON_Value *value = json_value_init_number(number);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_object_dotset_value(object, name, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_object_dotset_boolean(JSON_Object *object, const char *name, int boolean) {
    JSON_Value *value = json_value_init_boolean(boolean);
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_object_dotset_value(object, name, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_object_dotset_null(JSON_Object *object, const char *name) {
    JSON_Value *value = json_value_init_null();
    if (value == NULL) {
        return JSONFailure;
    }
    if (json_object_dotset_value(object, name, value) == JSONFailure) {
        json_value_free(value);
        return JSONFailure;
    }
    return JSONSuccess;
}

JSON_Status json_object_remove(JSON_Object *object, const char *name) {
    size_t i = 0, last_item_index = 0;
    if (object == NULL || json_object_get_value(object, name) == NULL) {
        return JSONFailure;
    }
    last_item_index = json_object_get_count(object) - 1;
    for (i = 0; i < json_object_get_count(object); i++) {
        if (strcmp(object->names[i], name) == 0) {
            parson_free(object->names[i]);
            json_value_free(object->values[i]);
            if (i != last_item_index) { /* Replace key value pair with one from the end */
                object->names[i] = object->names[last_item_index];
                object->values[i] = object->values[last_item_index];
            }
            object->count -= 1;
            return JSONSuccess;
        }
    }
    return JSONFailure; /* No execution path should end here */
}

JSON_Status json_object_dotremove(JSON_Object *object, const char *name) {
    const char *dot_pos = strchr(name, '.');
    char *current_name = NULL;
    JSON_Object *temp_obj = NULL;
    if (dot_pos == NULL) {
        return json_object_remove(object, name);
    } else {
        current_name = parson_strndup(name, dot_pos - name);
        temp_obj = json_object_get_object(object, current_name);
        if (temp_obj == NULL) {
            parson_free(current_name);
            return JSONFailure;
        }
        parson_free(current_name);
        return json_object_dotremove(temp_obj, dot_pos + 1);
    }
}

JSON_Status json_object_clear(JSON_Object *object) {
    size_t i = 0;
    if (object == NULL) {
        return JSONFailure;
    }
    for (i = 0; i < json_object_get_count(object); i++) {
        parson_free(object->names[i]);
        json_value_free(object->values[i]);
    }
    object->count = 0;
    return JSONSuccess;
}

JSON_Status json_validate(const JSON_Value *schema, const JSON_Value *value) {
    JSON_Value *temp_schema_value = NULL, *temp_value = NULL;
    JSON_Array *schema_array = NULL, *value_array = NULL;
    JSON_Object *schema_object = NULL, *value_object = NULL;
    JSON_Value_Type schema_type = JSONError, value_type = JSONError;
    const char *key = NULL;
    size_t i = 0, count = 0;
    if (schema == NULL || value == NULL) {
        return JSONFailure;
    }
    schema_type = json_value_get_type(schema);
    value_type = json_value_get_type(value);
    if (schema_type != value_type && schema_type != JSONNull) { /* null represents all values */
        return JSONFailure;
    }
    switch (schema_type) {
        case JSONArray:
            schema_array = json_value_get_array(schema);
            value_array = json_value_get_array(value);
            count = json_array_get_count(schema_array);
            if (count == 0) {
                return JSONSuccess; /* Empty array allows all types */
            }
            /* Get first value from array, rest is ignored */
            temp_schema_value = json_array_get_value(schema_array, 0);
            for (i = 0; i < json_array_get_count(value_array); i++) {
                temp_value = json_array_get_value(value_array, i);
                if (json_validate(temp_schema_value, temp_value) == JSONFailure) {
                    return JSONFailure;
                }
            }
            return JSONSuccess;
        case JSONObject:
            schema_object = json_value_get_object(schema);
            value_object = json_value_get_object(value);
            count = json_object_get_count(schema_object);
            if (count == 0) {
                return JSONSuccess; /* Empty object allows all objects */
            } else if (json_object_get_count(value_object) < count) {
                return JSONFailure; /* Tested object mustn't have less name-value pairs than schema */
            }
            for (i = 0; i < count; i++) {
                key = json_object_get_name(schema_object, i);
                temp_schema_value = json_object_get_value(schema_object, key);
                temp_value = json_object_get_value(value_object, key);
                if (temp_value == NULL) {
                    return JSONFailure;
                }
                if (json_validate(temp_schema_value, temp_value) == JSONFailure) {
                    return JSONFailure;
                }
            }
            return JSONSuccess;
        case JSONString: case JSONNumber: case JSONBoolean: case JSONNull:
            return JSONSuccess; /* equality already tested before switch */
        case JSONError: default:
            return JSONFailure;
    }
}

JSON_Status json_value_equals(const JSON_Value *a, const JSON_Value *b) {
    JSON_Object *a_object = NULL, *b_object = NULL;
    JSON_Array *a_array = NULL, *b_array = NULL;
    const char *a_string = NULL, *b_string = NULL;
    const char *key = NULL;
    size_t a_count = 0, b_count = 0, i = 0;
    JSON_Value_Type a_type, b_type;
    a_type = json_value_get_type(a);
    b_type = json_value_get_type(b);
    if (a_type != b_type) {
        return 0;
    }
    switch (a_type) {
        case JSONArray:
            a_array = json_value_get_array(a);
            b_array = json_value_get_array(b);
            a_count = json_array_get_count(a_array);
            b_count = json_array_get_count(b_array);
            if (a_count != b_count) {
                return 0;
            }
            for (i = 0; i < a_count; i++) {
                if (!json_value_equals(json_array_get_value(a_array, i),
                                       json_array_get_value(b_array, i))) {
                    return 0;
                }
            }
            return 1;
        case JSONObject:
            a_object = json_value_get_object(a);
            b_object = json_value_get_object(b);
            a_count = json_object_get_count(a_object);
            b_count = json_object_get_count(b_object);
            if (a_count != b_count) {
                return 0;
            }
            for (i = 0; i < a_count; i++) {
                key = json_object_get_name(a_object, i);
                if (!json_value_equals(json_object_get_value(a_object, key),
                                       json_object_get_value(b_object, key))) {
                    return 0;
                }
            }
            return 1;
        case JSONString:
            a_string = json_value_get_string(a);
            b_string = json_value_get_string(b);
            if (a_string == NULL || b_string == NULL) {
                return 0; /* shouldn't happen */
            }
            return strcmp(a_string, b_string) == 0;
        case JSONBoolean:
            return json_value_get_boolean(a) == json_value_get_boolean(b);
        case JSONNumber:
            return fabs(json_value_get_number(a) - json_value_get_number(b)) < 0.000001; /* EPSILON */
        case JSONError:
            return 1;
        case JSONNull:
            return 1;
        default:
            return 1;
    }
}

JSON_Value_Type json_type(const JSON_Value *value) {
    return json_value_get_type(value);
}

JSON_Object * json_object (const JSON_Value *value) {
    return json_value_get_object(value);
}

JSON_Array * json_array  (const JSON_Value *value) {
    return json_value_get_array(value);
}

const char * json_string (const JSON_Value *value) {
    return json_value_get_string(value);
}

double json_number (const JSON_Value *value) {
    return json_value_get_number(value);
}

int json_boolean(const JSON_Value *value) {
    return json_value_get_boolean(value);
}

void json_set_allocation_functions(JSON_Malloc_Function malloc_fun, JSON_Free_Function free_fun) {
    parson_malloc = malloc_fun;
    parson_free = free_fun;
}

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
      if(vector_size(c->documents) == 0)
      {
        continue;
      }

      if(HttpRequestComplete(c->http))
      {
        if(HttpResponseStatus(c->http) == 200)
        {
          bg->successFunc(sstream_cstr(c->name), c->lastDocumentCount);
        }

        sstream_push_cstr(ser, "{\"documents\":[");

        /* Serializing documents of collection */
        for(j = 0; j < vector_size(c->documents); j++)
        {
          v = vector_at(c->documents, j)->rootVal;
          sstream_push_cstr(ser, json_serialize_to_string(v));
          if(i < vector_size(c->documents)-1)
          {
            sstream_push_char(ser, ',');
          }
        }

        sstream_push_cstr(ser, "]}");

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

  bg->url = sstream_new();
  sstream_push_cstr(bg->url, BG_URL);

  bg->path = sstream_new();
  sstream_push_cstr(bg->path, BG_PATH);

  bg->fullUrl = sstream_new();
  sstream_push_cstr(bg->fullUrl, sstream_cstr(bg->url));
  sstream_push_cstr(bg->fullUrl, sstream_cstr(bg->path));
  sstream_push_cstr(bg->fullUrl, "/projects/collections/");

  //TODO move to sstream and delete guid+key
  //bg->guid = guid;
  //bg->key = key;
  bg->guid = sstream_new();
  sstream_push_cstr(bg->guid, guid);
  
  bg->key = sstream_new();
  sstream_push_cstr(bg->key, key);
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

  sstream_delete(bg->url);
  sstream_delete(bg->path);
  sstream_delete(bg->fullUrl);
  sstream_delete(bg->guid);
  sstream_delete(bg->key);

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

