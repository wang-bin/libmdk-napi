# @mediadevkit/libmdk-napi

Reusable OHOS HAR package for embedding an MDK-backed video surface in ArkTS apps.

This package is derived from the upstream MDK OHOS example app, but reshaped into a library developers can publish and consume via `ohpm install @mediadevkit/libmdk-napi`.

## What the package provides

- `MdkPlayerView`: an `XComponent` wrapper that loads the native `mdk_napi` library.
- `MdkPlayerController`: an ArkTS controller for core playback operations.
- A native N-API bridge keyed by `XComponent` id, so multiple player views can exist in the same app.
- Escape hatches for MDK features via `setProperty`, `setDecoders`, `setActiveTracks`, `setMediaSource`, and `setAudioBackends`.

## Package layout

```text
ohos/
├── libmdk_napi/          # HAR library package
│   ├── src/main/cpp/
│   │   ├── CMakeLists.txt
│   │   ├── player_napi.cpp
│   │   └── types/libmdk_napi/
│   ├── src/main/ets/
│   │   ├── components/MdkPlayerView.ets
│   │   ├── controller/MdkPlayerController.ets
│   │   └── index.ets
│   ├── build-profile.json5
│   ├── hvigorfile.ts
│   └── oh-package.json5
├── example/              # Standalone example app
│   ├── AppScope/
│   ├── entry/
│   │   └── src/main/ets/pages/Index.ets
│   ├── build-profile.json5
│   ├── hvigorfile.ts
│   └── oh-package.json5
├── build-profile.json5
├── hvigorfile.ts
└── oh-package.json5
```

## MDK SDK prerequisite

This repository contains the package source, not the vendored MDK SDK binaries. Before building the HAR, place the OHOS MDK SDK under:

```text
ohos/libmdk_napi/src/main/cpp/mdk-sdk/
├── include/
│   └── mdk/
└── lib/
    ├── cmake/FindMDK.cmake
    └── ohos/
        ├── arm64-v8a/libmdk.so
        └── x86_64/libmdk.so
```

The default lookup path is configured in [ohos/libmdk_napi/src/main/cpp/CMakeLists.txt](/Users/hr/develop/libmdk-napi/ohos/libmdk_napi/src/main/cpp/CMakeLists.txt). If you package MDK elsewhere, pass `-DMDK_SDK_DIR=/absolute/path/to/mdk-sdk` through your module build settings.

## Build the HAR

```bash
cd ohos
hvigorw assembleHar --mode module -p module=libmdk_napi@default
```

The output HAR is written to `libmdk_napi/build/default/outputs/default/`.

## Run the example app

The `example/` directory is a self-contained OHOS app that depends on the local HAR via a `file:` reference. It shows a video player with play/pause, stop, seek, and a URL input.

```bash
cd ohos/example
ohpm install
hvigorw assembleHap --mode module -p module=entry@default
```

Then install and launch on a connected device or emulator:

```bash
hdc app install build/default/outputs/default/entry-default-signed.hap
hdc shell aa start -b com.mediadevkit.mdkexample -a EntryAbility
```

> The example page pre-loads an HLS stream on `aboutToAppear`. Change the URL in the text input at runtime or edit `Index.ets` to point at your own source.

## Consumer setup

1. Publish the generated HAR to your private registry or package feed under `@mediadevkit/libmdk-napi`.
2. In an OHOS app, add the dependency with `ohpm install @mediadevkit/libmdk-napi`.
3. Add app permissions as needed:
   - `ohos.permission.INTERNET` for remote playback
   - `ohos.permission.READ_MEDIA` if your app opens local files and passes `fd://` URLs

## Example usage

```ts
import { ColorSpace, MdkPlayerController, MdkPlayerView } from '@mediadevkit/libmdk-napi';

@Entry
@Component
struct DemoPage {
  private controller: MdkPlayerController = new MdkPlayerController();

  aboutToAppear(): void {
    this.controller.setDecoders(['OH', 'ohcodec:copy=1', 'FFmpeg', 'dav1d']);
    this.controller.setMedia('https://example.com/video.mp4');
    this.controller.play();
  }

  aboutToDisappear(): void {
    this.controller.dispose();
  }

  build() {
    Column({ space: 12 }) {
      MdkPlayerView({ controller: this.controller, playerWidth: '100%', playerHeight: 240 })

      Button('Pause').onClick(() => this.controller.pause())
      Button('BT.709').onClick(() => this.controller.setColorSpace(ColorSpace.BT709))
    }
  }
}
```

## Exposed MDK coverage

The wrapper intentionally focuses on the player features that map cleanly to a reusable UI package:

- playback: `setMedia`, `setMediaSource`, `play`, `pause`, `stop`, `prepare`, `seek`
- transport tuning: `setPlaybackRate`, `setLoop`, `setVolume`, `buffered`
- decoder and track control: `setDecoders`, `setActiveTracks`, `setAudioBackends`
- rendering options: `setColorSpace`
- escape hatch: `setProperty` and `getProperty`

Advanced async callbacks from the C++ API are not wrapped yet. For features like `onMediaStatus`, `onEvent`, `snapshot`, or subtitle callbacks, extend the native bridge with either event emitters or typed async callbacks.

## API notes from MDK

- `setMedia("fd://N")` is translated to `setProperty("avio", "N")` plus `setMedia("fd:")`, matching the upstream OHOS example.
- `setDecoders(MediaType.Video, ["OH", "ohcodec:copy=1", "FFmpeg", "dav1d"])` is the recommended OHOS decoder order from the MDK Player APIs documentation.
- `set(State::Stopped)` releases playback resources. If you re-open immediately after stop, the underlying MDK state transition is still async.

## Current limits

- This package scaffold was created on macOS without DevEco Studio or an OHOS SDK runtime, so build and device validation still need to be run in an OHOS environment.
- Publishing as `@mediadevkit/libmdk-napi` is a registry step outside this repository; the package metadata and source layout are prepared for that flow.