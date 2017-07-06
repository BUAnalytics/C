#ifdef AMALGAMATION_EXAMPLE
  #include "bg_analytics.h"
#else
  #include <bg/analytics.h>
  #include <http/http.h>
#endif

#ifdef _WIN32
  #include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>

void on_error(const char *cln, int code)
{
  printf("Error Code: %i\n",code);
}

void on_success(const char *cln, int count)
{
  printf("Success Count: %i\n", count);
}

void multiDocTest()
{
  size_t i = 0;

  bgCollectionCreate("Test");

  for(i = 0; i < 10; i++)
  {
    struct bgDocument *doc = bgDocumentCreate();
    struct bgDocument *docX = bgDocumentCreate();
    struct bgDocument *docY = bgDocumentCreate();
  
    /*  Bunch of data to test  */
    bgDocumentAddCStr(doc, "String", "lotsaString and whitespace too\t and some \n backslash");
    bgDocumentAddCStr(doc, "boop.test", "blahblahblah");
    bgDocumentAddCStr(doc, "beep.floop", "fdjsklfjds");
    bgDocumentAddInt(doc, "Val.a", 32);
    bgDocumentAddInt(doc, "Val.b", 35);
    bgDocumentAddDouble(doc, "double", 3.14159265789);
    bgDocumentAddBool(doc, "int", 1);

    bgDocumentAddCStr(docX, "String", "X");
    bgDocumentAddCStr(docX, "boop.test", "X");
    bgDocumentAddCStr(docX, "beep.floop", "X");
    bgDocumentAddInt(docX, "Val.a", 32);
    bgDocumentAddInt(docX, "Val.b", 35);
    bgDocumentAddDouble(docX, "double", 3.14159265789);
    bgDocumentAddBool(docX, "int", 1);

    bgDocumentAddCStr(docY, "String", "y");
    bgDocumentAddCStr(docY, "boop.test", "y");
    bgDocumentAddCStr(docY, "beep.floop", "y");
    bgDocumentAddInt(docY, "Val.a", 32);
    bgDocumentAddInt(docY, "Val.b", 35);
    bgDocumentAddDouble(docY, "double", 3.14159265789);
    bgDocumentAddBool(docY, "int", 1);

    bgCollectionAdd("Test", doc);
    bgCollectionAdd("Test", docX);
    bgCollectionAdd("Test", docY);

#ifdef _WIN32
    Sleep(1000);
#endif
  }

  bgCollectionUpload("Test");
}

int main(int argc, char *argv[])
{
  bgAuth("59537b89d18a11000b16bd80", "d06d4d9835b363142fe729bca1ce441c18811787b5991e9c4399ad6b0b8f9c07");

  bgErrorFunc(on_error);
  bgSuccessFunc(on_success);

  multiDocTest();

  bgCleanup();

  return 0;
}
