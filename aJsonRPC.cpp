#include "Arduino.h"
#include "aJSON.h"
#include "aJsonRPC.h"

#define AJSONRPC_MAX_FUNCTION_NAME 32

namespace ajsonrpc {

struct Mapping
{
  char name[AJSONRPC_MAX_FUNCTION_NAME];
  void (*callback)(const aJsonObject*, aJsonObject*);
};

struct FuncMap
{
  Mapping* mappings;
  unsigned int capacity;
  unsigned int used;
};

} // namespace ajsonrpc

aJsonRPC::aJsonRPC(int capacity)
{
  mymap = (ajsonrpc::FuncMap*)malloc(sizeof(ajsonrpc::FuncMap));
  mymap->capacity = capacity;
  mymap->used = 0;
  mymap->mappings = (ajsonrpc::Mapping*)malloc(capacity * sizeof(ajsonrpc::Mapping));
  memset(mymap->mappings, 0, capacity * sizeof(ajsonrpc::Mapping));
}

void aJsonRPC::registerMethod(const char* methodName, void (*callback)(const aJsonObject*, aJsonObject*))
{
  // only write keyvalue pair if we allocated enough memory for it
  if(mymap->used < mymap->capacity)
  {
    ajsonrpc::Mapping* mapping = &(mymap->mappings[mymap->used++]);
    const size_t nameLen = strlen(methodName);
    if(nameLen > AJSONRPC_MAX_FUNCTION_NAME)
      return;

    strcpy(mapping->name, methodName);
    mapping->callback = callback;
  }
}

bool aJsonRPC::processMessage(const aJsonObject* msg, aJsonObject* result)
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
    writeError(result, AJSONRPC_PARSE_ERROR);
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
      writeError(result, AJSONRPC_INVALID_REQUEST);
      return false;
    }
  }

  // A String containing the name of the method to be invoked. Method names that begin with the word rpc
  // followed by a period character (U+002E or ASCII 46) are reserved for rpc-internal methods and extensions and MUST NOT be used for anything else.
  aJsonObject* method = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "method");
  if(!method || !(method && method->type == aJson_String))
  {
    writeError(result, AJSONRPC_INVALID_REQUEST);
    return false;
  }

  // A String specifying the version of the JSON-RPC protocol. MUST be exactly "2.0".
  aJsonObject* jsonrpc = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "jsonrpc");
  if(!jsonrpc || !(jsonrpc && jsonrpc->type == aJson_String && strcmp(jsonrpc->valuestring, "2.0") == 0))
  {
    writeError(result, AJSONRPC_INVALID_REQUEST);
    return false;
  }

  // A Structured value that holds the parameter values to be used during the invocation of the method. This member MAY be omitted.
  // This means params might be nullptr
  aJsonObject* params = aJson.getObjectItem(const_cast<aJsonObject*>(msg), "params");

  const char* methodName = method->valuestring;
  for(unsigned int i = 0; i < mymap->used; i++)
  {
    ajsonrpc::Mapping* mapping = &(mymap->mappings[i]);
    if(strcmp(methodName, mapping->name) == 0)
    {
      mapping->callback(params, result);
      return true;
    }
  }

  writeError(result, AJSONRPC_METHOD_NOT_FOUND);
  return false;
}

void aJsonRPC::writeError(aJsonObject* result, int errorCode)
{
  auto* error = aJson.createObject();
  aJson.addItemToObject(result, "error", error);
  aJson.addNumberToObject(error, "code", errorCode);

  switch(errorCode)
  {
  case AJSONRPC_PARSE_ERROR: aJson.addStringToObject(error, "message", "Parse error"); break;
  case AJSONRPC_INVALID_REQUEST: aJson.addStringToObject(error, "message", "Invalid Request"); break;
  case AJSONRPC_METHOD_NOT_FOUND: aJson.addStringToObject(error, "message", "Method not found"); break;
  case AJSONRPC_INVALID_PARAM: aJson.addStringToObject(error, "message", "Invalid params"); break;
  case AJSONRPC_INTERNAL_ERROR: aJson.addStringToObject(error, "message", "Internal error"); break;
  default: aJson.addStringToObject(error, "message", "Server error"); break;
  }
}

static bool isPositionalParams(const aJsonObject* params, aJsonObject* result)
{
  if(!params || params->type != aJson_Array)
  {
    aJsonRPC::writeError(result, AJSONRPC_INVALID_PARAM);
    return false;
  }

  return true;
}
struct aJson_BoolId
{
};

static bool getJsonMember(const aJsonObject* param, aJson_BoolId) { return param->valuebool; }

struct aJson_FloatId
{
};
static float getJsonMember(const aJsonObject* param, aJson_FloatId) { return param->valuefloat; }

template<typename _Type, typename _MemberId>
static _Type getJsonMember(const aJsonObject* param)
{
  return getJsonMember(param, _MemberId());
}

template<typename _Type, int _TypeId, typename _MemberId>
static bool getPositionalParam(const aJsonObject* params, aJsonObject* result, int index, _Type& value)
{
  if(!isPositionalParams(params, result))
    return false;

  const aJsonObject* param = aJson.getArrayItem(const_cast<aJsonObject*>(params), index);

  if(!param || param->type != _TypeId)
  {
    aJsonRPC::writeError(result, AJSONRPC_INVALID_PARAM);
    return false;
  }

  value = getJsonMember<_Type, _MemberId>(param);
  return true;
}

bool aJsonRPC::getPositionalParam(const aJsonObject* params, aJsonObject* result, int index, bool& value)
{
  return ::getPositionalParam<bool, aJson_Boolean, aJson_BoolId>(params, result, index, value);
}

bool aJsonRPC::getPositionalParam(const aJsonObject* params, aJsonObject* result, int index, float& value)
{
  return ::getPositionalParam<float, aJson_Float, aJson_FloatId>(params, result, index, value);
}