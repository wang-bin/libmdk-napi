#include "global_napi.h"
#include "mdk/global.h"
#include <hilog/log.h>
#include <mutex>

using namespace MDK_NS;
using namespace std;

string ToString(napi_env env, napi_value value)
{
    size_t length = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &length);
    vector<char> buffer(length + 1);
    napi_get_value_string_utf8(env, value, buffer.data(), buffer.size(), &length);
    return {buffer.data(), length};
}

napi_value Undefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

void RegisterLogHandlerOnce()
{
    static once_flag gLogHandlerOnce;
    call_once(gLogHandlerOnce, [] {
        static const ::LogLevel ohLevels[] = {
            LOG_INFO,
            LOG_ERROR,
            LOG_WARN,
            LOG_INFO,
            LOG_DEBUG,
            LOG_DEBUG,
        };
        setLogHandler([](MDK_NS::LogLevel level, const char* msg) {
            const int index = (int)level >= 0 && (int)level < 6 ? (int)level : 0;
            OH_LOG_Print(LOG_APP, ohLevels[index], 0xFF00, "mdk", "%{public}s", msg);
        });
    });
}

napi_value Version(napi_env env, napi_callback_info info)
{
    napi_value result = nullptr;
    napi_create_int32(env, version(), &result);
    return result;
}

napi_value SetGlobalOptionString(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[0]);
    const auto value = ToString(env, args[1]);
    SetGlobalOption(key.c_str(), value.c_str());
    return Undefined(env);
}

napi_value GetGlobalOptionString(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[0]);
    const char* value = nullptr;
    if (!GetGlobalOption(key.c_str(), &value) || !value) {
        napi_value result = nullptr;
        napi_get_null(env, &result);
        return result;
    }
    napi_value result = nullptr;
    napi_create_string_utf8(env, value, NAPI_AUTO_LENGTH, &result);
    return result;
}

napi_value SetGlobalOptionInt(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[0]);
    int32_t value = 0;
    napi_get_value_int32(env, args[1], &value);
    SetGlobalOption(key.c_str(), value);
    return Undefined(env);
}

napi_value GetGlobalOptionInt(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[0]);
    int value = 0;
    if (!GetGlobalOption(key.c_str(), &value)) {
        napi_value result = nullptr;
        napi_get_null(env, &result);
        return result;
    }
    napi_value result = nullptr;
    napi_create_int32(env, value, &result);
    return result;
}

napi_value SetGlobalOptionFloat(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[0]);
    double value = 0;
    napi_get_value_double(env, args[1], &value);
    SetGlobalOption(key.c_str(), (float)value);
    return Undefined(env);
}
