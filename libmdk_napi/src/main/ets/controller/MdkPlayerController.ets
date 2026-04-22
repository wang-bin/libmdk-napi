import nativeModule from 'libmdk_napi.so';
import { ColorSpace, MediaType, PlaybackState } from '../enums';

function createPlayerId(): string {
  const now = Date.now();
  const randomPart = Math.floor(Math.random() * 1000000);
  return `mdk_${now}_${randomPart}`;
}

export class MdkPlayerController {
  public readonly playerId: string;

  constructor(playerId?: string) {
    this.playerId = playerId ?? createPlayerId();
  }

  setMedia(url: string): void {
    nativeModule.setMedia(this.playerId, url);
  }

  setMediaSource(url: string, mediaType: MediaType): void {
    nativeModule.setMediaSource(this.playerId, url, mediaType);
  }

  play(): void {
    nativeModule.play(this.playerId);
  }

  pause(): void {
    nativeModule.pause(this.playerId);
  }

  stop(): void {
    nativeModule.stop(this.playerId);
  }

  prepare(startPosition: number = 0): void {
    nativeModule.prepare(this.playerId, startPosition);
  }

  seek(positionMs: number): void {
    nativeModule.seek(this.playerId, positionMs);
  }

  setPlaybackRate(rate: number): void {
    nativeModule.setPlaybackRate(this.playerId, rate);
  }

  setVolume(volume: number): void {
    nativeModule.setVolume(this.playerId, volume);
  }

  setLoop(count: number): void {
    nativeModule.setLoop(this.playerId, count);
  }

  setProperty(key: string, value: string): void {
    nativeModule.setProperty(this.playerId, key, value);
  }

  getProperty(key: string): string {
    return nativeModule.getProperty(this.playerId, key);
  }

  setColorSpace(colorSpace: ColorSpace): void {
    nativeModule.setColorSpace(this.playerId, colorSpace);
  }

  setDecoders(names: string[], mediaType: MediaType = MediaType.Video): void {
    nativeModule.setDecoders(this.playerId, mediaType, names);
  }

  setActiveTracks(tracks: number[], mediaType: MediaType): void {
    nativeModule.setActiveTracks(this.playerId, mediaType, tracks);
  }

  setAudioBackends(names: string[]): void {
    nativeModule.setAudioBackends(this.playerId, names);
  }

  getPosition(): number {
    return nativeModule.getPosition(this.playerId);
  }

  getDuration(): number {
    return nativeModule.getDuration(this.playerId);
  }

  buffered(): number {
    return nativeModule.buffered(this.playerId);
  }

  getState(): PlaybackState {
    return nativeModule.getState(this.playerId);
  }

  getMediaStatus(): number {
    return nativeModule.getMediaStatus(this.playerId);
  }

  isPlaying(): boolean {
    return nativeModule.isPlaying(this.playerId);
  }

  setVideoSurfaceSize(width: number, height: number): void {
    nativeModule.setVideoSurfaceSize(this.playerId, width, height);
  }

  dispose(): void {
    nativeModule.releasePlayer(this.playerId);
  }
}

export { ColorSpace, MediaType, PlaybackState };