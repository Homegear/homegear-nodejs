#include <node_api.h>

#include "HomegearObject.h"

napi_value Init(napi_env env, napi_value exports) {
  return Homegear::Init(env, exports);
}

NAPI_MODULE(homegear, Init)
