/*
 * Reusable MDK N-API bridge for OHOS.
 *
 * Derived from the upstream example app, but keyed by XComponent id so it can
 * back multiple player views inside a publishable HAR package.
 */
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>

#include "mdk/Player.h"
#include "global_napi.h"
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

// Heap-allocated data passed through threadsafe-function queues.
struct MediaStatusData { int32_t oldStatus; int32_t newStatus; };
struct EventData { int64_t error; string category; string detail; };
// One-shot callback: carries both the tsfn (to self-release) and the result value.
struct OneShotData { napi_threadsafe_function tsfn; int64_t value; };

struct PlayerContext {
    unique_ptr<Player> player;
    void* window = nullptr;
    // Per-player JS callback threadsafe functions (nullptr = not registered).
    napi_threadsafe_function stateChangedTsfn  = nullptr;
    napi_threadsafe_function mediaStatusTsfn   = nullptr;
    napi_threadsafe_function eventTsfn         = nullptr;
    napi_threadsafe_function loopTsfn          = nullptr;
    napi_threadsafe_function mediaChangedTsfn  = nullptr;
    // Tokens used to remove token-keyed callbacks (onMediaStatus/onEvent/onLoop).
    CallbackToken mediaStatusToken = 0;
    CallbackToken eventToken       = 0;
    CallbackToken loopToken        = 0;
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

    // Collect tsfns so we can release them outside the lock (avoiding potential deadlock with the JS main thread finalizer).
    vector<napi_threadsafe_function> tsfns;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it != gPlayers.end()) {
            auto& ctx = it->second;
            if (ctx.window)
                ctx.player->updateNativeSurface(nullptr, 0, 0);
            ctx.player->set(State::Stopped);
            // Clear MDK-side callbacks so no new items are queued to the tsfns.
            if (ctx.stateChangedTsfn)
                ctx.player->onStateChanged(nullptr);
            if (ctx.mediaStatusTsfn)
                ctx.player->onMediaStatus(nullptr, &ctx.mediaStatusToken);
            if (ctx.eventTsfn)
                ctx.player->onEvent(nullptr, &ctx.eventToken);
            if (ctx.loopTsfn)
                ctx.player->onLoop(nullptr, &ctx.loopToken);
            if (ctx.mediaChangedTsfn)
                ctx.player->currentMediaChanged(nullptr);
            tsfns.emplace_back(ctx.stateChangedTsfn);
            tsfns.emplace_back(ctx.mediaStatusTsfn);
            tsfns.emplace_back(ctx.eventTsfn);
            tsfns.emplace_back(ctx.loopTsfn);
            tsfns.emplace_back(ctx.mediaChangedTsfn);
            gPlayers.erase(it);
        }
    }
    for (auto& fn : tsfns) {
        if (fn)
            napi_release_threadsafe_function(fn, napi_tsfn_release);
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

static bool IsFunction(napi_env env, napi_value v)
{
    auto t = napi_undefined;
    napi_typeof(env, v, &t);
    return t == napi_function;
}

// Create a named threadsafe function wrapping a JS function.
static napi_threadsafe_function CreateTsfn(napi_env env, napi_value fn, const char* name,
                                    napi_threadsafe_function_call_js callJsCb)
{
    napi_value asyncName = nullptr;
    napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &asyncName);
    napi_threadsafe_function tsfn = nullptr;
    napi_create_threadsafe_function(env, fn, nullptr, asyncName,
                                    /*max_queue_size=*/0, /*initial_thread_count=*/1,
                                    nullptr, nullptr, nullptr, callJsCb, &tsfn);
    return tsfn;
}

// forward declaration — defined below with SeekWithFlags
static void SeekCallJs(napi_env env, napi_value fn, void*, void* data);

napi_value Prepare(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t startPosition = 0;
    if (argc > 1)
        napi_get_value_int64(env, args[1], &startPosition);

    if (argc > 2 && IsFunction(env, args[2])) {
        napi_threadsafe_function tsfn = CreateTsfn(env, args[2], "prepareCallback", SeekCallJs);
        lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) {
            ctx.player->prepare(startPosition, [tsfn](int64_t pos, bool* /*boost*/) -> bool {
                napi_call_threadsafe_function(tsfn, new OneShotData{tsfn, pos}, napi_tsfn_nonblocking);
                return true;
            });
        });
    } else {
        lockFor(ToString(env, args[0]), [=](PlayerContext& ctx) { ctx.player->prepare(startPosition); });
    }
    return Undefined(env);
}

// Shared helper: call seek with optional one-shot callback tsfn (nullptr = no callback).
static void DoSeek(const string& id, int64_t ms, SeekFlag flags, napi_threadsafe_function tsfn)
{
    lockFor(id, [=](PlayerContext& ctx) {
        if (tsfn) {
            ctx.player->seek(ms, flags, [tsfn](int64_t pos) {
                napi_call_threadsafe_function(tsfn, new OneShotData{tsfn, pos}, napi_tsfn_nonblocking);
            });
        } else {
            ctx.player->seek(ms, flags);
        }
    });
}

static void SeekCallJs(napi_env env, napi_value fn, void*, void* data)
{
    auto* d = static_cast<OneShotData*>(data);
    napi_value arg = nullptr;
    napi_create_int64(env, d->value, &arg);
    napi_value recv = nullptr;
    napi_get_undefined(env, &recv);
    napi_call_function(env, recv, fn, 1, &arg, nullptr);
    napi_release_threadsafe_function(d->tsfn, napi_tsfn_release);
    delete d;
}

napi_value Seek(napi_env env, napi_callback_info info)
{
    size_t argc = 3; napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t ms = 0;
    napi_get_value_int64(env, args[1], &ms);
    napi_threadsafe_function tsfn = (argc > 2 && IsFunction(env, args[2]))
        ? CreateTsfn(env, args[2], "seekCallback", SeekCallJs) : nullptr;
    DoSeek(ToString(env, args[0]), ms, SeekFlag::Default, tsfn);
    return Undefined(env);
}

napi_value SeekWithFlags(napi_env env, napi_callback_info info)
{
    size_t argc = 4; napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int64_t ms = 0;
    int32_t flags = 0;
    napi_get_value_int64(env, args[1], &ms);
    napi_get_value_int32(env, args[2], &flags);
    napi_threadsafe_function tsfn = (argc > 3 && IsFunction(env, args[3]))
        ? CreateTsfn(env, args[3], "seekCallback", SeekCallJs) : nullptr;
    DoSeek(ToString(env, args[0]), ms, (SeekFlag)flags, tsfn);
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

// onStateChanged(playerId, callback | null)
napi_value OnStateChanged(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);

    napi_threadsafe_function oldTsfn = nullptr;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it == gPlayers.end())
            return Undefined(env);
        auto& ctx = it->second;

        // Always clear previous C++ callback first.
        ctx.player->onStateChanged(nullptr);
        oldTsfn = ctx.stateChangedTsfn;
        ctx.stateChangedTsfn = nullptr;

        if (IsFunction(env, args[1])) {
            auto callJs = [](napi_env env, napi_value fn, void*, void* data) {
                napi_value arg = nullptr;
                napi_create_int32(env, (int32_t)(intptr_t)data, &arg);
                napi_value recv = nullptr; napi_get_undefined(env, &recv);
                napi_call_function(env, recv, fn, 1, &arg, nullptr);
            };
            ctx.stateChangedTsfn = CreateTsfn(env, args[1], "onStateChanged", callJs);
            napi_threadsafe_function tsfn = ctx.stateChangedTsfn;
            ctx.player->onStateChanged([tsfn](State state) {
                napi_call_threadsafe_function(tsfn, (void*)(intptr_t)(int32_t)state, napi_tsfn_nonblocking);
            });
        }
    }
    if (oldTsfn)
        napi_release_threadsafe_function(oldTsfn, napi_tsfn_release);
    return Undefined(env);
}

// onMediaStatus(playerId, callback | null)
// callback(oldStatus: number, newStatus: number)
napi_value OnMediaStatus(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);

    napi_threadsafe_function oldTsfn = nullptr;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it == gPlayers.end())
            return Undefined(env);
        auto& ctx = it->second;

        if (ctx.mediaStatusTsfn)
            ctx.player->onMediaStatus(nullptr, &ctx.mediaStatusToken);
        oldTsfn = ctx.mediaStatusTsfn;
        ctx.mediaStatusTsfn = nullptr;
        ctx.mediaStatusToken = 0;

        if (IsFunction(env, args[1])) {
            auto callJs = [](napi_env env, napi_value fn, void*, void* data) {
                auto d = static_cast<MediaStatusData*>(data);
                napi_value argv[2] = {};
                napi_create_int32(env, d->oldStatus, &argv[0]);
                napi_create_int32(env, d->newStatus, &argv[1]);
                delete d;
                napi_value recv = nullptr; napi_get_undefined(env, &recv);
                napi_call_function(env, recv, fn, 2, argv, nullptr);
            };
            ctx.mediaStatusTsfn = CreateTsfn(env, args[1], "onMediaStatus", callJs);
            napi_threadsafe_function tsfn = ctx.mediaStatusTsfn;
            ctx.player->onMediaStatus([tsfn](MediaStatus oldS, MediaStatus newS) {
                auto d = new MediaStatusData{(int32_t)oldS, (int32_t)newS};
                napi_call_threadsafe_function(tsfn, d, napi_tsfn_nonblocking);
                return true;
            }, &ctx.mediaStatusToken);
        }
    }
    if (oldTsfn)
        napi_release_threadsafe_function(oldTsfn, napi_tsfn_release);
    return Undefined(env);
}

// onEvent(playerId, callback | null)
// callback(error: number, category: string, detail: string)
napi_value OnEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);

    napi_threadsafe_function oldTsfn = nullptr;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it == gPlayers.end())
            return Undefined(env);
        auto& ctx = it->second;

        if (ctx.eventTsfn)
            ctx.player->onEvent(nullptr, &ctx.eventToken);
        oldTsfn = ctx.eventTsfn;
        ctx.eventTsfn = nullptr;
        ctx.eventToken = 0;

        if (IsFunction(env, args[1])) {
            auto callJs = [](napi_env env, napi_value fn, void*, void* data) {
                auto* d = static_cast<EventData*>(data);
                napi_value argv[3] = {};
                napi_create_int64(env, d->error, &argv[0]);
                napi_create_string_utf8(env, d->category.c_str(), d->category.size(), &argv[1]);
                napi_create_string_utf8(env, d->detail.c_str(), d->detail.size(), &argv[2]);
                delete d;
                napi_value recv = nullptr; napi_get_undefined(env, &recv);
                napi_call_function(env, recv, fn, 3, argv, nullptr);
            };
            ctx.eventTsfn = CreateTsfn(env, args[1], "onEvent", callJs);
            napi_threadsafe_function tsfn = ctx.eventTsfn;
            ctx.player->onEvent([tsfn](const MediaEvent& e) {
                auto* d = new EventData{e.error, e.category, e.detail};
                napi_call_threadsafe_function(tsfn, d, napi_tsfn_nonblocking);
                return false;
            }, &ctx.eventToken);
        }
    }
    if (oldTsfn)
        napi_release_threadsafe_function(oldTsfn, napi_tsfn_release);
    return Undefined(env);
}

// onLoop(playerId, callback | null)
// callback(loopCount: number)
napi_value OnLoop(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);

    napi_threadsafe_function oldTsfn = nullptr;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it == gPlayers.end())
            return Undefined(env);
        auto& ctx = it->second;

        if (ctx.loopTsfn)
            ctx.player->onLoop(nullptr, &ctx.loopToken);
        oldTsfn = ctx.loopTsfn;
        ctx.loopTsfn = nullptr;
        ctx.loopToken = 0;

        if (IsFunction(env, args[1])) {
            auto callJs = [](napi_env env, napi_value fn, void*, void* data) {
                napi_value arg = nullptr;
                napi_create_int32(env, (int32_t)(intptr_t)data, &arg);
                napi_value recv = nullptr; napi_get_undefined(env, &recv);
                napi_call_function(env, recv, fn, 1, &arg, nullptr);
            };
            ctx.loopTsfn = CreateTsfn(env, args[1], "onLoop", callJs);
            napi_threadsafe_function tsfn = ctx.loopTsfn;
            ctx.player->onLoop([tsfn](int count) {
                napi_call_threadsafe_function(tsfn, (void*)(intptr_t)count, napi_tsfn_nonblocking);
            }, &ctx.loopToken);
        }
    }
    if (oldTsfn)
        napi_release_threadsafe_function(oldTsfn, napi_tsfn_release);
    return Undefined(env);
}

// onCurrentMediaChanged(playerId, callback | null)
// callback()  — fired when gapless next media starts
napi_value OnCurrentMediaChanged(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    const auto id = ToString(env, args[0]);

    napi_threadsafe_function oldTsfn = nullptr;
    {
        const scoped_lock lock(gMutex);
        auto it = gPlayers.find(id);
        if (it == gPlayers.end())
            return Undefined(env);
        auto& ctx = it->second;

        ctx.player->currentMediaChanged(nullptr);
        oldTsfn = ctx.mediaChangedTsfn;
        ctx.mediaChangedTsfn = nullptr;

        if (IsFunction(env, args[1])) {
            auto callJs = [](napi_env env, napi_value fn, void*, void* /*data*/) {
                napi_value recv = nullptr; napi_get_undefined(env, &recv);
                napi_call_function(env, recv, fn, 0, nullptr, nullptr);
            };
            ctx.mediaChangedTsfn = CreateTsfn(env, args[1], "onCurrentMediaChanged", callJs);
            napi_threadsafe_function tsfn = ctx.mediaChangedTsfn;
            ctx.player->currentMediaChanged([tsfn]() {
                napi_call_threadsafe_function(tsfn, nullptr, napi_tsfn_nonblocking);
            });
        }
    }
    if (oldTsfn)
        napi_release_threadsafe_function(oldTsfn, napi_tsfn_release);
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
        {"setResourceManager", nullptr, SetResourceManager, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onStateChanged", nullptr, OnStateChanged, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onMediaStatus", nullptr, OnMediaStatus, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onEvent", nullptr, OnEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onLoop", nullptr, OnLoop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onCurrentMediaChanged", nullptr, OnCurrentMediaChanged, nullptr, nullptr, nullptr, napi_default, nullptr},
    };

    napi_define_properties(env, exports, std::size(descriptors), descriptors);
    return exports;
}

NAPI_MODULE(mdk_napi, Init)

} // namespace
