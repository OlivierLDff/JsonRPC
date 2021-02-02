#ifndef JsonRPC_h
#define JsonRPC_h

#include "Arduino.h"
#include "aJSON.h"

#define JSONRPC_MAX_FUNCTION_NAME 32

#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAM -32602
#define JSONRPC_INTERNAL_ERROR -32603

struct Mapping
{
  char name[JSONRPC_MAX_FUNCTION_NAME];
  void (*callback)(const aJsonObject*, aJsonObject*);
};

struct FuncMap
{
  Mapping* mappings;
  unsigned int capacity;
  unsigned int used;
};

class JsonRPC
{
public:
  JsonRPC(int capacity);
  void registerMethod(const char* methodname, void (*callback)(const aJsonObject*, aJsonObject*));
  bool processMessage(const aJsonObject* msg, aJsonObject* result);

  static void writeError(aJsonObject* result, int errorCode);

private:
  FuncMap* mymap;
};

#endif
