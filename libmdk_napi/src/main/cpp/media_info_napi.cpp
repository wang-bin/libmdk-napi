#include "media_info_napi.h"

#include <string>
#include <type_traits>
#include <unordered_map>

using namespace MDK_NS;
using namespace std;

namespace {

napi_value NewObject(napi_env env)
{
    napi_value result = nullptr;
    napi_create_object(env, &result);
    return result;
}

template<typename>
inline constexpr bool kAlwaysFalse = false;

template<typename T>
napi_value to_napi(napi_env env, T value)
{
    using U = remove_cvref_t<T>;
    napi_value v = nullptr;
    if constexpr (is_same_v<U, napi_value>) {
        v = value;
    } else if constexpr (is_same_v<U, int32_t>) {
        napi_create_int32(env, value, &v);
    } else if constexpr (is_same_v<U, int64_t>) {
        napi_create_int64(env, value, &v);
    } else if constexpr (is_unsigned_v<U>) {
        napi_create_uint32(env, value, &v);
    } else if constexpr (is_same_v<U, double> || is_same_v<U, float>) {
        napi_create_double(env, value, &v);
    } else if constexpr (is_same_v<U, bool>) {
        napi_get_boolean(env, value, &v);
    } else if constexpr (is_same_v<U, string>) {
        napi_create_string_utf8(env, value.c_str(), value.size(), &v);
    } else if constexpr (is_same_v<U, const char*> || is_same_v<U, char*>) {
        napi_create_string_utf8(env, value ? value : "", NAPI_AUTO_LENGTH, &v);
    } else {
        static_assert(kAlwaysFalse<U>, "Unsupported type for to_napi");
    }
    return v;
}

template<typename T>
void SetNamed(napi_env env, napi_value object, const char* name, T value)
{
    napi_set_named_property(env, object, name, to_napi(env, value));
}

napi_value MetadataObject(napi_env env, const unordered_map<string, string>& metadata)
{
    napi_value object = NewObject(env);
    for (const auto& [key, value] : metadata)
        SetNamed(env, object, key.c_str(), value);
    return object;
}

napi_value AudioCodecParametersObject(napi_env env, const AudioCodecParameters& codec)
{
    napi_value object = NewObject(env);
    SetNamed(env, object, "codec", codec.codec);
    SetNamed(env, object, "codecTag", codec.codec_tag);
    SetNamed(env, object, "extraDataSize", codec.extra_data_size);
    SetNamed(env, object, "bitRate", codec.bit_rate);
    SetNamed(env, object, "profile", codec.profile);
    SetNamed(env, object, "level", codec.level);
    SetNamed(env, object, "frameRate", codec.frame_rate);
    SetNamed(env, object, "isFloat", codec.is_float);
    SetNamed(env, object, "isUnsigned", codec.is_unsigned);
    SetNamed(env, object, "isPlanar", codec.is_planar);
    SetNamed(env, object, "rawSampleSize", codec.raw_sample_size);
    SetNamed(env, object, "channels", codec.channels);
    SetNamed(env, object, "sampleRate", codec.sample_rate);
    SetNamed(env, object, "blockAlign", codec.block_align);
    SetNamed(env, object, "frameSize", codec.frame_size);
    return object;
}

napi_value VideoCodecParametersObject(napi_env env, const VideoCodecParameters& codec)
{
    napi_value object = NewObject(env);
    SetNamed(env, object, "codec", codec.codec);
    SetNamed(env, object, "codecTag", codec.codec_tag);
    SetNamed(env, object, "extraDataSize", codec.extra_data_size);
    SetNamed(env, object, "bitRate", codec.bit_rate);
    SetNamed(env, object, "profile", codec.profile);
    SetNamed(env, object, "level", codec.level);
    SetNamed(env, object, "frameRate", codec.frame_rate);
    SetNamed(env, object, "format", codec.format);
    SetNamed(env, object, "formatName", codec.format_name);
    SetNamed(env, object, "width", codec.width);
    SetNamed(env, object, "height", codec.height);
    SetNamed(env, object, "bFrames", codec.b_frames);
    SetNamed(env, object, "par", codec.par);
    SetNamed(env, object, "colorSpace", (int32_t)codec.color_space);
    SetNamed(env, object, "doviProfile", codec.dovi_profile);
    return object;
}

napi_value SubtitleCodecParametersObject(napi_env env, const SubtitleCodecParameters& codec)
{
    napi_value object = NewObject(env);
    SetNamed(env, object, "codec", codec.codec);
    SetNamed(env, object, "codecTag", codec.codec_tag);
    SetNamed(env, object, "extraDataSize", codec.extra_data_size);
    SetNamed(env, object, "width", codec.width);
    SetNamed(env, object, "height", codec.height);
    return object;
}

} // namespace

napi_value MediaInfoToNapi(napi_env env, const MediaInfo& info)
{
    napi_value result = NewObject(env);
    SetNamed(env, result, "startTime", info.start_time);
    SetNamed(env, result, "duration", info.duration);
    SetNamed(env, result, "bitRate", info.bit_rate);
    SetNamed(env, result, "format", info.format);
    SetNamed(env, result, "streams", info.streams);
    SetNamed(env, result, "metadata", MetadataObject(env, info.metadata));

    napi_value audio = nullptr;
    napi_create_array_with_length(env, info.audio.size(), &audio);
    for (uint32_t i = 0; i < info.audio.size(); ++i) {
        const auto& stream = info.audio[i];
        napi_value item = NewObject(env);
        SetNamed(env, item, "index", stream.index);
        SetNamed(env, item, "startTime", stream.start_time);
        SetNamed(env, item, "duration", stream.duration);
        SetNamed(env, item, "frames", stream.frames);
        SetNamed(env, item, "codec", AudioCodecParametersObject(env, stream.codec));
        SetNamed(env, item, "metadata", MetadataObject(env, stream.metadata));
        napi_set_element(env, audio, i, item);
    }
    SetNamed(env, result, "audio", audio);

    napi_value video = nullptr;
    napi_create_array_with_length(env, info.video.size(), &video);
    for (uint32_t i = 0; i < info.video.size(); ++i) {
        const auto& stream = info.video[i];
        napi_value item = NewObject(env);
        SetNamed(env, item, "index", stream.index);
        SetNamed(env, item, "startTime", stream.start_time);
        SetNamed(env, item, "duration", stream.duration);
        SetNamed(env, item, "frames", stream.frames);
        SetNamed(env, item, "rotation", stream.rotation);
        SetNamed(env, item, "codec", VideoCodecParametersObject(env, stream.codec));
        SetNamed(env, item, "width", stream.codec.width);
        SetNamed(env, item, "height", stream.codec.height);
        SetNamed(env, item, "metadata", MetadataObject(env, stream.metadata));
        napi_set_element(env, video, i, item);
    }
    SetNamed(env, result, "video", video);

    napi_value subtitle = nullptr;
    napi_create_array_with_length(env, info.subtitle.size(), &subtitle);
    for (uint32_t i = 0; i < info.subtitle.size(); ++i) {
        const auto& stream = info.subtitle[i];
        napi_value item = NewObject(env);
        SetNamed(env, item, "index", stream.index);
        SetNamed(env, item, "startTime", stream.start_time);
        SetNamed(env, item, "duration", stream.duration);
        SetNamed(env, item, "codec", SubtitleCodecParametersObject(env, stream.codec));
        SetNamed(env, item, "metadata", MetadataObject(env, stream.metadata));
        napi_set_element(env, subtitle, i, item);
    }
    SetNamed(env, result, "subtitle", subtitle);

    return result;
}
