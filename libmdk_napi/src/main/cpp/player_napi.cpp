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
#include "media_info_napi.h"

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

using namespace MDK_NS;
using namespace std;

namespace { // no need to add static for each function/var in an anonymous namespace

struct PlayerContext {
    unique_ptr<Player> player;
    void* window = nullptr;
};

mutex gMutex;
map<string, PlayerContext> gPlayers;

// Lock gMutex, ensure player context exists, then call f(ctx).
auto lockFor(const string& id, auto&& f)
{
    [[maybe_unused]] const scoped_lock lock(gMutex);
    auto& ctx = gPlayers[id];
    if (!ctx.player) {
        ctx.player = make_unique<Player>();
        const char* vdecs = nullptr;
        if (GetGlobalOption("video.decoders.hint", &vdecs)) {
            ctx.player->setProperty("video.decoders", vdecs);
        }
    }
    if constexpr (is_invocable_v<decltype(f), PlayerContext&>)
        return f(ctx);
}

// Lock gMutex and call f(ctx) only if the context already exists.
void lockFind(const string& id, auto&& f)
{
    [[maybe_unused]] const scoped_lock lock(gMutex);
    if (auto it = gPlayers.find(id); it != gPlayers.end())
        f(it->second);
}

string ToString(napi_env env, napi_value value)
{
    size_t length = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &length);
    vector<char> buffer(length + 1);
    napi_get_value_string_utf8(env, value, buffer.data(), buffer.size(), &length);
    return {buffer.data(), length};
}

template<class Container>
Container FromArray(napi_env env, napi_value value)
{
    Container result;
    bool isArray = false;
    napi_is_array(env, value, &isArray);
    if (!isArray)
        return result;

    uint32_t length = 0;
    napi_get_array_length(env, value, &length);
    for (uint32_t index = 0; index < length; ++index) {
        napi_value item = nullptr;
        napi_get_element(env, value, index, &item);
        if constexpr (is_same_v<typename Container::value_type, int>) {
            int32_t v = 0;
            napi_get_value_int32(env, item, &v);
            result.insert(result.end(), v);
        }
        if constexpr (is_same_v<typename Container::value_type, string>) {
            result.insert(result.end(), ToString(env, item));
        }
    }
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

string IdOf(OH_NativeXComponent* component)
{
    char id[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idLength = OH_XCOMPONENT_ID_LEN_MAX + 1;
    OH_NativeXComponent_GetXComponentId(component, id, &idLength);
    return id;
}

void OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
        uint64_t width = 0, height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    lockFor(IdOf(component), [=](PlayerContext& ctx) {
        ctx.window = window;
        ctx.player->updateNativeSurface(window, (int)width, (int)height);
        //ctx.player->set(ColorSpaceUnknown, window); // hdr passthrough
    });
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    uint64_t width = 0, height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    lockFind(IdOf(component), [=](PlayerContext& ctx) {
        ctx.player->updateNativeSurface(ctx.window, (int)width, (int)height);
    });
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* /*window*/)
{
    lockFind(IdOf(component), [](PlayerContext& ctx) {
        ctx.window = nullptr;
        ctx.player->updateNativeSurface(nullptr, 0, 0);
    });
}

napi_value Undefined(napi_env env)
{
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value SetVideoSurfaceSize(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t width = 0, height = 0;
    napi_get_value_int32(env, args[1], &width);
    napi_get_value_int32(env, args[2], &height);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) {
        ctx.player->setVideoSurfaceSize(width, height, ctx.window);
    });
    return Undefined(env);
}

napi_value EnsurePlayer(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    lockFor(ToString(env, args[0]), nullptr);
    return Undefined(env);
}

napi_value ReleasePlayer(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);
    const scoped_lock lock(gMutex);
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
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const string url = ToString(env, args[1]);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) {
        ctx.player->setMedia(url.c_str());
    });
    return Undefined(env);
}

napi_value SetMediaSource(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const string url = ToString(env, args[1]);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[2], &mediaType);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) {
        ctx.player->setMedia(url.empty() ? nullptr : url.c_str(), (MediaType)mediaType);
    });
    return Undefined(env);
}

napi_value Play(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { ctx.player->set(State::Playing); });
    return Undefined(env);
}

napi_value Pause(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { ctx.player->set(State::Paused); });
    return Undefined(env);
}

napi_value Stop(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { ctx.player->set(State::Stopped); });
    return Undefined(env);
}

napi_value Prepare(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t startPosition = 0;
    if (argc > 1)
        napi_get_value_int64(env, args[1], &startPosition);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->prepare(startPosition); });
    return Undefined(env);
}

napi_value Seek(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t ms = 0;
    napi_get_value_int64(env, args[1], &ms);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->seek(ms); });
    return Undefined(env);
}

napi_value SeekWithFlags(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t ms = 0;
    int32_t flags = 0;
    napi_get_value_int64(env, args[1], &ms);
    napi_get_value_int32(env, args[2], &flags);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) {
        ctx.player->seek(ms, (SeekFlag)flags);
    });
    return Undefined(env);
}

napi_value SetPlaybackRate(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double rate = 1.0;
    napi_get_value_double(env, args[1], &rate);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->setPlaybackRate((float)rate); });
    return Undefined(env);
}

napi_value SetVolume(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double volume = 1.0;
    napi_get_value_double(env, args[1], &volume);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->setVolume((float)volume); });
    return Undefined(env);
}

napi_value SetLoop(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t count = 0;
    napi_get_value_int32(env, args[1], &count);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->setLoop(count); });
    return Undefined(env);
}

napi_value SetProperty(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[1]);
    const auto value = ToString(env, args[2]);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) { ctx.player->setProperty(key, value); });
    return Undefined(env);
}

napi_value GetProperty(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto key = ToString(env, args[1]);
    const auto value = lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) {
        return ctx.player->property(key);
    });
    napi_value result = nullptr;
    napi_create_string_utf8(env, value.c_str(), value.size(), &result);
    return result;
}

napi_value SetColorSpace(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t colorSpace = 0;
    napi_get_value_int32(env, args[1], &colorSpace);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->set((ColorSpace)colorSpace, ctx.window); });
    return Undefined(env);
}

napi_value SetVideoEffect(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t effect = 0;
    double value = 0;
    napi_get_value_int32(env, args[1], &effect);
    napi_get_value_double(env, args[2], &value);
    lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) {
        const float v = (float)value;
        ctx.player->set((VideoEffect)effect, v, ctx.window);
    });
    return Undefined(env);
}

napi_value SetDecoders(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[1], &mediaType);
    const auto decoders = FromArray<vector<string>>(env, args[2]);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) {
        ctx.player->setDecoders((MediaType)mediaType, decoders);
    });
    return Undefined(env);
}

napi_value SetActiveTracks(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int32_t mediaType = 0;
    napi_get_value_int32(env, args[1], &mediaType);
    const auto tracks = FromArray<set<int>>(env, args[2]);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) {
        ctx.player->setActiveTracks((MediaType)mediaType, tracks);
    });
    return Undefined(env);
}

napi_value SetAudioBackends(napi_env env, napi_callback_info info)
{
    size_t argc = 2; napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto backends = FromArray<vector<string>>(env, args[1]);
    lockFor(ToString(env, args[0]), [&](PlayerContext& ctx) { ctx.player->setAudioBackends(backends); });
    return Undefined(env);
}

napi_value GetPosition(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto pos = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { return ctx.player->position(); });
    napi_value result = nullptr;
    napi_create_int64(env, pos, &result);
    return result;
}

napi_value Buffered(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto buf = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { return ctx.player->buffered(); });
    napi_value result = nullptr;
    napi_create_int64(env, buf, &result);
    return result;
}

napi_value GetState(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto state = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { return (int32_t)ctx.player->state(); });
    napi_value result = nullptr;
    napi_create_int32(env, state, &result);
    return result;
}

napi_value GetMediaStatus(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto status = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { return (int32_t)ctx.player->mediaStatus(); });
    napi_value result = nullptr;
    napi_create_int32(env, status, &result);
    return result;
}

napi_value GetMediaInfo(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto mediaInfo = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) { return ctx.player->mediaInfo(); });
    return MediaInfoToNapi(env, mediaInfo);
}

napi_value IsPlaying(napi_env env, napi_callback_info info)
{
    size_t argc = 1; napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto playing = lockFor(ToString(env, args[0]), [](PlayerContext& ctx) {
        return ctx.player->state() == State::Playing;
    });
    napi_value result = nullptr;
    napi_get_boolean(env, playing, &result);
    return result;
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
                    lockFor(IdOf(nativeXComponent), nullptr);

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
        {"seekWithFlags", nullptr, SeekWithFlags, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setPlaybackRate", nullptr, SetPlaybackRate, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setVolume", nullptr, SetVolume, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setLoop", nullptr, SetLoop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setProperty", nullptr, SetProperty, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getProperty", nullptr, GetProperty, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setColorSpace", nullptr, SetColorSpace, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setVideoEffect", nullptr, SetVideoEffect, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setDecoders", nullptr, SetDecoders, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setActiveTracks", nullptr, SetActiveTracks, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setAudioBackends", nullptr, SetAudioBackends, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getPosition", nullptr, GetPosition, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"buffered", nullptr, Buffered, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getState", nullptr, GetState, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getMediaStatus", nullptr, GetMediaStatus, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getMediaInfo", nullptr, GetMediaInfo, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isPlaying", nullptr, IsPlaying, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setVideoSurfaceSize", nullptr, SetVideoSurfaceSize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"version", nullptr, Version, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setGlobalOptionString", nullptr, SetGlobalOptionString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getGlobalOptionString", nullptr, GetGlobalOptionString, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setGlobalOptionInt", nullptr, SetGlobalOptionInt, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getGlobalOptionInt", nullptr, GetGlobalOptionInt, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setGlobalOptionFloat", nullptr, SetGlobalOptionFloat, nullptr, nullptr, nullptr, napi_default, nullptr},
    };

    napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    return exports;
}

NAPI_MODULE(mdk_napi, Init)

} // namespace
