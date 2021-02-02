#include "Arduino.h"
#include "aJSON.h"
#include "JsonRPC.h"

JsonRPC::JsonRPC(int capacity)
{
  mymap = (FuncMap*)malloc(sizeof(FuncMap));
  mymap->capacity = capacity;
  mymap->used = 0;
  mymap->mappings = (Mapping*)malloc(capacity * sizeof(Mapping));
  memset(mymap->mappings, 0, capacity * sizeof(Mapping));
}

void JsonRPC::registerMethod(const char* methodName, void (*callback)(const aJsonObject*, aJsonObject*))
{
  // only write keyvalue pair if we allocated enough memory for it
  if(mymap->used < mymap->capacity)
  {
    Mapping* mapping = &(mymap->mappings[mymap->used++]);
    const size_t nameLen = strlen(methodName);
    if(nameLen > JSONRPC_MAX_FUNCTION_NAME)
      return;

    strcpy(mapping->name, methodName);
    mapping->callback = callback;
  }
}

bool JsonRPC::processMessage(const aJsonObject* msg, aJsonObject* result)
{
  // Make sure to have an answer.
  // Even if previous version didn't use result, this new version of JsonRPC MUST use result.
  if(!result)
    return false;

  // A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
  aJson.addStringToObject(result, "jsonrpc", "2.0");

  if(!msg)
  {
    aJson.addNullToObject(result, "id");
    writeError(result, JSONRPC_PARSE_ERROR);
    return false;
  }

  // An identifier established by the Client that MUST contain a String, Number, or NULL value if included.
  // If it is not included it is assumed to be a notification. The value SHOULD normally not be Null [1]
  // and Numbers SHOULD NOT contain fractional parts [2]
  {
    aJsonObject* id = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "id");
    if(id->type == aJson_Int)
    {
      aJson.addNumberToObject(result, "id", id->valueint);
    }
    else if(id->type == aJson_String)
    {
      aJson.addStringToObject(result, "id", id->valuestring);
    }
    else
    {
      aJson.addNullToObject(result, "id");
      writeError(result, JSONRPC_INVALID_REQUEST);
      return false;
    }
  }

  // A String containing the name of the method to be invoked. Method names that begin with the word rpc
  // followed by a period character (U+002E or ASCII 46) are reserved for rpc-internal methods and extensions and MUST NOT be used for anything else.
  aJsonObject* method = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "method");
  if(!method || !(method && method->type == aJson_String))
  {
    writeError(result, JSONRPC_INVALID_REQUEST);
    return false;
  }

  // A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
  aJsonObject* jsonrpc = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "jsonrpc");
  if(!jsonrpc || !(jsonrpc && jsonrpc->type == aJson_String && strcmp(jsonrpc->valuestring, "2.0") == 0))
  {
    writeError(result, JSONRPC_INVALID_REQUEST);
    return false;
  }

  // A Structured value that holds the parameter values to be used during the invocation of the method. This member MAY be omitted.
  // This means params might be nullptr
  aJsonObject* params = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "params");

  const char* methodName = method->valuestring;
  for(unsigned int i = 0; i < mymap->used; i++)
  {
    Mapping* mapping = &(mymap->mappings[i]);
    if(strcmp(methodName, mapping->name) == 0)
    {
      mapping->callback(params, result);
      return true;
    }
  }

  writeError(result, JSONRPC_METHOD_NOT_FOUND);
  return false;
}

void JsonRPC::writeError(aJsonObject* result, int errorCode)
{
  auto* error = aJson.createObject();
  aJson.addItemToObject(result, "error", error);
  aJson.addNumberToObject(error, "code", errorCode);

  switch(errorCode)
  {
  case JSONRPC_PARSE_ERROR: aJson.addStringToObject(error, "message", "Parse error"); break;
  case JSONRPC_INVALID_REQUEST: aJson.addStringToObject(error, "message", "Invalid Request"); break;
  case JSONRPC_METHOD_NOT_FOUND: aJson.addStringToObject(error, "message", "Method not found"); break;
  case JSONRPC_INVALID_PARAM: aJson.addStringToObject(error, "message", "Invalid params"); break;
  case JSONRPC_INTERNAL_ERROR: aJson.addStringToObject(error, "message", "Internal error"); break;
  default: aJson.addStringToObject(error, "message", "Server error"); break;
  }
}
