export const enum ColorSpace {
  Unknown = 0,
  BT709 = 1,
  BT2100_PQ = 2,
  SCRGB = 3,
  ExtendedLinearDisplayP3 = 4,
  ExtendedSRGB = 5,
  ExtendedLinearSRGB = 6,
  BT2100_HLG = 7,
}

export const enum MediaType {
  Unknown = -1,
  Video = 0,
  Audio = 1,
  Subtitle = 3,
}

export const enum PlaybackState {
  Stopped = 0,
  Running = 1,
  Playing = 1,
  Paused = 2,
}

export const enum MediaStatus {
  NoMedia = 0,
  Unloaded = 1,
  Loading = 1 << 1,
  Loaded = 1 << 2,
  Stalled = 1 << 3,
  Buffering = 1 << 4,
  Buffered = 1 << 5,
  End = 1 << 6,
  Seeking = 1 << 7,
  Prepared = 1 << 8,
  Invalid = 1 << 31,
}

export const enum SeekFlag {
  From0 = 1,
  FromStart = 1 << 1,
  FromNow = 1 << 2,
  Frame = 1 << 6,
  KeyFrame = 1 << 8,
  Fast = KeyFrame,
  AnyFrame = 1 << 9,
  InCache = 1 << 10,
  Backward = 1 << 16,
  Default = KeyFrame | FromStart | InCache,
}

export const enum LogLevel {
  Off = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
  Debug = 4,
  All = 5,
}

export const enum VideoEffect {
  Brightness = 0,
  Contrast = 1,
  Hue = 2,
  Saturation = 3,
  ScaleChannels = 4,
  ShiftChannels = 5,
}

// Note: these enums are defined as ArkTS enums in the HAR package.
// Import enums from ETS files when writing app code.

export interface NativeMdkPlayerModule {
  ensurePlayer: (playerId: string) => void;
  releasePlayer: (playerId: string) => void;
  setMedia: (playerId: string, url: string) => void;
  setMediaSource: (playerId: string, url: string, mediaType: MediaType) => void;
  play: (playerId: string) => void;
  pause: (playerId: string) => void;
  stop: (playerId: string) => void;
  prepare: (playerId: string, startPosition?: number) => void;
  seek: (playerId: string, positionMs: number) => void;
  seekWithFlags: (playerId: string, positionMs: number, flags: SeekFlag) => void;
  setPlaybackRate: (playerId: string, rate: number) => void;
  setVolume: (playerId: string, volume: number) => void;
  setLoop: (playerId: string, count: number) => void;
  setProperty: (playerId: string, key: string, value: string) => void;
  getProperty: (playerId: string, key: string) => string;
  setColorSpace: (playerId: string, colorSpace: ColorSpace) => void;
  setVideoEffect: (playerId: string, effect: VideoEffect, value: number) => void;
  setDecoders: (playerId: string, mediaType: MediaType, names: string[]) => void;
  setActiveTracks: (playerId: string, mediaType: MediaType, tracks: number[]) => void;
  setAudioBackends: (playerId: string, names: string[]) => void;
  getPosition: (playerId: string) => number;
  getDuration: (playerId: string) => number;
  buffered: (playerId: string) => number;
  getState: (playerId: string) => PlaybackState;
  getMediaStatus: (playerId: string) => MediaStatus;
  isPlaying: (playerId: string) => boolean;
  setVideoSurfaceSize: (playerId: string, width: number, height: number) => void;
  version: () => number;
  setGlobalOptionString: (key: string, value: string) => void;
  getGlobalOptionString: (key: string) => string | null;
  setGlobalOptionInt: (key: string, value: number) => void;
  getGlobalOptionInt: (key: string) => number | null;
  setGlobalOptionFloat: (key: string, value: number) => void;
}

declare const nativeModule: NativeMdkPlayerModule;

export default nativeModule;
