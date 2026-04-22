/*
 * Reusable MDK N-API bridge for OHOS.
 *
 * Derived from the upstream example app, but keyed by XComponent id so it can
 * back multiple player views inside a publishable HAR package.
 */
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <napi/native_api.h>

#include "mdk/Player.h"

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

using namespace MDK_NS;

namespace {

struct PlayerContext {
    std::unique_ptr<Player> player;
    void* window = nullptr;
};

std::mutex gMutex;
std::map<std::string, PlayerContext> gPlayers;
std::once_flag gLogHandlerOnce;

PlayerContext& EnsureContextLocked(const std::string& id)
{
    auto& context = gPlayers[id];
    if (!context.player)
        context.player = std::make_unique<Player>();
    return context;
}

PlayerContext* FindContextLocked(const std::string& id)
{
    auto it = gPlayers.find(id);
    return it == gPlayers.end() ? nullptr : &it->second;
}

std::string GetStringArg(napi_env env, napi_value value)
{
    size_t length = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &length);
    std::vector<char> buffer(length + 1);
    napi_get_value_string_utf8(env, value, buffer.data(), buffer.size(), &length);
    return std::string(buffer.data(), length);
}

std::vector<std::string> GetStringArrayArg(napi_env env, napi_value value)
{
    std::vector<std::string> result;
    bool isArray = false;
    napi_is_array(env, value, &isArray);
    if (!isArray)
        return result;

    uint32_t length = 0;
    napi_get_array_length(env, value, &length);
    result.reserve(length);
    for (uint32_t index = 0; index < length; ++index) {
        napi_value item = nullptr;
        napi_get_element(env, value, index, &item);
        result.push_back(GetStringArg(env, item));
    }
    return result;
}

std::set<int> GetIntSetArg(napi_env env, napi_value value)
{
    std::set<int> result;
    bool isArray = false;
    napi_is_array(env, value, &isArray);
    if (!isArray)
        return result;

    uint32_t length = 0;
    napi_get_array_length(env, value, &length);
    for (uint32_t index = 0; index < length; ++index) {
        napi_value item = nullptr;
        int32_t track = 0;
        napi_get_element(env, value, index, &item);
        napi_get_value_int32(env, item, &track);
        result.insert(track);
    }
    return result;
}

void RegisterLogHandlerOnce()
{
    std::call_once(gLogHandlerOnce, [] {
        setLogHandler([](MDK_NS::LogLevel level, const char* msg) {
            static const ::LogLevel ohLevels[] = {
                LOG_INFO,
                LOG_ERROR,
                LOG_WARN,
                LOG_INFO,
                LOG_DEBUG,
                LOG_DEBUG,
            };
            const int index = (int)level >= 0 && (int)level < 6 ? (int)level : 0;
            OH_LOG_Print(LOG_APP, ohLevels[index], 0xFF00, "mdk", "%{public}s", msg);
        });
    });
}

std::string XComponentIdOf(OH_NativeXComponent* component)
{
    char id[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idLength = OH_XCOMPONENT_ID_LEN_MAX + 1;
    OH_NativeXComponent_GetXComponentId(component, id, &idLength);
    return std::string(id);
}

void OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    const std::string id = XComponentIdOf(component);
    uint64_t width = 0;
    uint64_t height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);

    std::lock_guard<std::mutex> lock(gMutex);
    auto& context = EnsureContextLocked(id);
    context.window = window;
    context.player->updateNativeSurface(window, (int)width, (int)height);
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    const std::string id = XComponentIdOf(component);
    uint64_t width = 0;
    uint64_t height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);

    std::lock_guard<std::mutex> lock(gMutex);
    if (auto* context = FindContextLocked(id))
        context->player->updateNativeSurface(context->window, (int)width, (int)height);
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window)
{
    (void)window;
    const std::string id = XComponentIdOf(component);

    std::lock_guard<std::mutex> lock(gMutex);
    if (auto* context = FindContextLocked(id)) {
        context->window = nullptr;
        context->player->updateNativeSurface(nullptr, 0, 0);
    }
}

napi_value Undefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value SetVideoSurfaceSize(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t width = 0;
    int32_t height = 0;
    napi_get_value_int32(env, args[1], &width);
    napi_get_value_int32(env, args[2], &height);

    std::lock_guard<std::mutex> lock(gMutex);
    if (auto* context = FindContextLocked(id))
        context->player->setVideoSurfaceSize(width, height, context->window);
    return Undefined(env);
}

napi_value EnsurePlayer(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id);
    return Undefined(env);
}

napi_value ReleasePlayer(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gPlayers.find(id);
    if (it != gPlayers.end()) {
        if (it->second.window)
            it->second.player->updateNativeSurface(nullptr, 0, 0);
        it->second.player->set(State::Stopped);
        gPlayers.erase(it);
    }
    return Undefined(env);
}

napi_value SetMedia(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    const std::string url = GetStringArg(env, args[1]);

    std::lock_guard<std::mutex> lock(gMutex);
    auto& context = EnsureContextLocked(id);
    if (url.rfind("fd://", 0) == 0) {
        context.player->setProperty("avio", url.substr(5));
        context.player->setMedia("fd:");
    } else {
        context.player->setMedia(url.c_str());
    }
    return Undefined(env);
}

napi_value SetMediaSource(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    const std::string url = GetStringArg(env, args[1]);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[2], &mediaType);

    std::lock_guard<std::mutex> lock(gMutex);
    auto& context = EnsureContextLocked(id);
    context.player->setMedia(url.empty() ? nullptr : url.c_str(), (MediaType)mediaType);
    return Undefined(env);
}

napi_value Play(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->set(State::Playing);
    return Undefined(env);
}

napi_value Pause(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->set(State::Paused);
    return Undefined(env);
}

napi_value Stop(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->set(State::Stopped);
    return Undefined(env);
}

napi_value Prepare(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int64_t startPosition = 0;
    if (argc > 1)
        napi_get_value_int64(env, args[1], &startPosition);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->prepare(startPosition);
    return Undefined(env);
}

napi_value Seek(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int64_t ms = 0;
    napi_get_value_int64(env, args[1], &ms);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->seek(ms);
    return Undefined(env);
}

napi_value SetPlaybackRate(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    double rate = 1.0;
    napi_get_value_double(env, args[1], &rate);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setPlaybackRate((float)rate);
    return Undefined(env);
}

napi_value SetVolume(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    double volume = 1.0;
    napi_get_value_double(env, args[1], &volume);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setVolume((float)volume);
    return Undefined(env);
}

napi_value SetLoop(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t count = 0;
    napi_get_value_int32(env, args[1], &count);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setLoop(count);
    return Undefined(env);
}

napi_value SetProperty(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    const std::string key = GetStringArg(env, args[1]);
    const std::string value = GetStringArg(env, args[2]);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setProperty(key, value);
    return Undefined(env);
}

napi_value GetProperty(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    const std::string key = GetStringArg(env, args[1]);

    std::string value;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        value = EnsureContextLocked(id).player->property(key);
    }

    napi_value result = nullptr;
    napi_create_string_utf8(env, value.c_str(), value.size(), &result);
    return result;
}

napi_value SetColorSpace(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t colorSpace = 0;
    napi_get_value_int32(env, args[1], &colorSpace);

    std::lock_guard<std::mutex> lock(gMutex);
    auto& context = EnsureContextLocked(id);
    context.player->set((ColorSpace)colorSpace, context.window);
    return Undefined(env);
}

napi_value SetDecoders(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[1], &mediaType);
    const std::vector<std::string> decoders = GetStringArrayArg(env, args[2]);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setDecoders((MediaType)mediaType, decoders);
    return Undefined(env);
}

napi_value SetActiveTracks(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[1], &mediaType);
    const std::set<int> tracks = GetIntSetArg(env, args[2]);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setActiveTracks((MediaType)mediaType, tracks);
    return Undefined(env);
}

napi_value SetAudioBackends(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    const std::vector<std::string> backends = GetStringArrayArg(env, args[1]);

    std::lock_guard<std::mutex> lock(gMutex);
    EnsureContextLocked(id).player->setAudioBackends(backends);
    return Undefined(env);
}

napi_value GetPosition(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int64_t position = 0;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        position = EnsureContextLocked(id).player->position();
    }

    napi_value result = nullptr;
    napi_create_int64(env, position, &result);
    return result;
}

napi_value GetDuration(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int64_t duration = 0;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        duration = EnsureContextLocked(id).player->mediaInfo().duration;
    }

    napi_value result = nullptr;
    napi_create_int64(env, duration, &result);
    return result;
}

napi_value Buffered(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int64_t buffered = 0;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        buffered = EnsureContextLocked(id).player->buffered();
    }

    napi_value result = nullptr;
    napi_create_int64(env, buffered, &result);
    return result;
}

napi_value GetState(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t state = 0;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        state = (int32_t)EnsureContextLocked(id).player->state();
    }

    napi_value result = nullptr;
    napi_create_int32(env, state, &result);
    return result;
}

napi_value GetMediaStatus(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    int32_t status = 0;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        status = (int32_t)EnsureContextLocked(id).player->mediaStatus();
    }

    napi_value result = nullptr;
    napi_create_int32(env, status, &result);
    return result;
}

napi_value IsPlaying(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    const std::string id = GetStringArg(env, args[0]);
    bool isPlaying = false;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        isPlaying = EnsureContextLocked(id).player->state() == State::Playing;
    }

    napi_value result = nullptr;
    napi_get_boolean(env, isPlaying, &result);
    return result;
}

napi_value Init(napi_env env, napi_value exports)
{
    RegisterLogHandlerOnce();

    bool hasXComponent = false;
    if (napi_has_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &hasXComponent) == napi_ok && hasXComponent) {
        napi_value xcompInstance = nullptr;
        if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &xcompInstance) == napi_ok) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, xcompInstance, &valueType);
            if (valueType != napi_undefined && valueType != napi_null) {
                OH_NativeXComponent* nativeXComponent = nullptr;
                napi_unwrap(env, xcompInstance, (void**)&nativeXComponent);
                if (nativeXComponent) {
                    const std::string id = XComponentIdOf(nativeXComponent);
                    {
                        std::lock_guard<std::mutex> lock(gMutex);
                        EnsureContextLocked(id);
                    }

                    static OH_NativeXComponent_Callback callbacks {};
                    callbacks.OnSurfaceCreated = OnSurfaceCreated;
                    callbacks.OnSurfaceChanged = OnSurfaceChanged;
                    callbacks.OnSurfaceDestroyed = OnSurfaceDestroyed;
                    callbacks.DispatchTouchEvent = nullptr;
                    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callbacks);
                }
            }
        }
    }

    napi_property_descriptor descriptors[] = {
        {"ensurePlayer", nullptr, EnsurePlayer, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"releasePlayer", nullptr, ReleasePlayer, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setMedia", nullptr, SetMedia, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setMediaSource", nullptr, SetMediaSource, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"play", nullptr, Play, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"pause", nullptr, Pause, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"prepare", nullptr, Prepare, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"seek", nullptr, Seek, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setPlaybackRate", nullptr, SetPlaybackRate, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setVolume", nullptr, SetVolume, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setLoop", nullptr, SetLoop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setProperty", nullptr, SetProperty, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getProperty", nullptr, GetProperty, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setColorSpace", nullptr, SetColorSpace, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setDecoders", nullptr, SetDecoders, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setActiveTracks", nullptr, SetActiveTracks, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setAudioBackends", nullptr, SetAudioBackends, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getPosition", nullptr, GetPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getDuration", nullptr, GetDuration, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"buffered", nullptr, Buffered, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getState", nullptr, GetState, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getMediaStatus", nullptr, GetMediaStatus, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isPlaying", nullptr, IsPlaying, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setVideoSurfaceSize", nullptr, SetVideoSurfaceSize, nullptr, nullptr, nullptr, napi_default, nullptr},
    };

    napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    return exports;
}

static napi_module playerModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "mdk_napi",
    .nm_priv = nullptr,
    .reserved = {nullptr},
};

} // namespace

extern "C" __attribute__((constructor)) void RegisterPlayerNapiModule(void)
{
    napi_module_register(&playerModule);
}