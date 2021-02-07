#ifndef __AJSON_RPC_HPP__
#define __AJSON_RPC_HPP__

#include "Arduino.h"
#include "aJSON.h"

#define AJSONRPC_PARSE_ERROR -32700
#define AJSONRPC_INVALID_REQUEST -32600
#define AJSONRPC_METHOD_NOT_FOUND -32601
#define AJSONRPC_INVALID_PARAM -32602
#define AJSONRPC_INTERNAL_ERROR -32603

namespace ajsonrpc {
struct FuncMap;
}

class aJsonRPC
{
public:
  aJsonRPC(int capacity);
  void registerMethod(const char* methodname, void (*callback)(const aJsonObject*, aJsonObject*));
  bool processMessage(const aJsonObject* msg, aJsonObject* result);

  static void writeError(aJsonObject* result, int errorCode);

  static bool getPositionalParam(const aJsonObject* params, aJsonObject* result, int index, bool& value);
  static bool getPositionalParam(const aJsonObject* params, aJsonObject* result, int index, float& value);

private:
  ajsonrpc::FuncMap* mymap;
};

#endif
