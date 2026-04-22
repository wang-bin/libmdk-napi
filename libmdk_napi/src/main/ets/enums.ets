export enum ColorSpace {
  Unknown = 0,
  BT709 = 1,
  BT2100_PQ = 2,
}

export enum MediaType {
  Unknown = -1,
  Video = 0,
  Audio = 1,
  Subtitle = 3,
}

export enum PlaybackState {
  Stopped = 0,
  Playing = 1,
  Paused = 2,
}
