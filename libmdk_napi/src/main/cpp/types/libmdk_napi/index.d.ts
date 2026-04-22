export const enum ColorSpace {
  Unknown = 0,
  BT709 = 1,
  BT2100_PQ = 2,
}

export const enum MediaType {
  Unknown = -1,
  Video = 0,
  Audio = 1,
  Subtitle = 3,
}

export const enum PlaybackState {
  Stopped = 0,
  Playing = 1,
  Paused = 2,
}

// Note: ColorSpace, MediaType, PlaybackState are defined as ArkTS enums in enums.ets
// and should be imported from the HAR package, not from this native module.

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
  setPlaybackRate: (playerId: string, rate: number) => void;
  setVolume: (playerId: string, volume: number) => void;
  setLoop: (playerId: string, count: number) => void;
  setProperty: (playerId: string, key: string, value: string) => void;
  getProperty: (playerId: string, key: string) => string;
  setColorSpace: (playerId: string, colorSpace: ColorSpace) => void;
  setDecoders: (playerId: string, mediaType: MediaType, names: string[]) => void;
  setActiveTracks: (playerId: string, mediaType: MediaType, tracks: number[]) => void;
  setAudioBackends: (playerId: string, names: string[]) => void;
  getPosition: (playerId: string) => number;
  getDuration: (playerId: string) => number;
  buffered: (playerId: string) => number;
  getState: (playerId: string) => PlaybackState;
  getMediaStatus: (playerId: string) => number;
  isPlaying: (playerId: string) => boolean;
  setVideoSurfaceSize: (playerId: string, width: number, height: number) => void;
}

declare const nativeModule: NativeMdkPlayerModule;

export default nativeModule;