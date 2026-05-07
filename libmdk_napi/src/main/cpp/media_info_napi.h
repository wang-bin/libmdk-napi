#pragma once

#include <napi/native_api.h>

#include "mdk/MediaInfo.h"

napi_value MediaInfoToNapi(napi_env env, const MDK_NS::MediaInfo& info);
