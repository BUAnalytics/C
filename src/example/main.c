#ifdef AMALGAMATION_EXAMPLE
  #include "bg_analytics.h"
#else
  #include <bg/analytics.h>
  #include <http/http.h>
#endif

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

void on_error(char *cln, int code)
{
  printf("\nError Code: %i\n",code);
}

void on_success(char *cln, int count)
{
  printf("\nSucces dCount: %i\n", count);
}

void http_test()
{
  struct Http *http = NULL;

  http = HttpCreate();
  HttpAddCustomHeader(http, "Some-Header", "Thisisavalue");

  HttpRequest(http,
    "http://portal.quickvr.me/nv/client/ChannelList-getChanList?something=7",
    NULL);

  while(!HttpRequestComplete(http))
  {
#ifdef _WIN32
    Sleep(10);
#else
    usleep(1000);
#endif
  }

  printf("Portal status: %i\n", HttpResponseStatus(http));
  printf("Portal content: %s\n", HttpResponseContent(http));

  HttpDestroy(http);
}

int main(int argc, char *argv[])
{
  int t = 0;

  struct bgDocument *doc;
  doc = bgDocumentCreate();

  http_test();
  bgAuth("59537b89d18a11000b16bd80", "d06d4d9835b363142fe729bca1ce441c18811787b5991e9c4399ad6b0b8f9c07");

  bgErrorFunc(on_error);
  bgSuccessFunc(on_success);

  bgCollectionCreate("Test");
  
  /*  Bunch of data to test  */
  bgDocumentAddCStr(doc, "String", "lotsaString and whitespace too\t and some \n backslash");
  bgDocumentAddCStr(doc, "boop.test", "blahblahblah");
  bgDocumentAddCStr(doc, "beep.floop", "fdjsklfjds");
  //bgDocumentAddCStr(doc, "boop.test", "floop");
  
  bgDocumentAddInt(doc, "Val.a", 32);
  bgDocumentAddInt(doc, "Val.b", 35);
  //bgDocumentAddInt(doc, "Val.a", 543);

  bgDocumentAddDouble(doc, "double", 3.14159265789);
  //bgDocumentAddDouble(doc, "doubledot.a", 1.1818181818);
  //bgDocumentAddDouble(doc, "doubledot.b", 2.3636363636);

  bgDocumentAddBool(doc, "int", 1);
  //bgDocumentAddBool(doc, "b.a", 1);
  //bgDocumentAddBool(doc, "b.b", 0);

  bgCollectionAdd("Test", doc);

  bgCollectionUpload("Test");

  bgCleanup();

  //system("iexplore http://...");


  return 0;
}
