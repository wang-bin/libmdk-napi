import {
  MdkPlayerController,
  MdkPlayerView,
  MediaStatus,
  SeekFlag,
  VideoEffect,
} from '@mediadevkit/libmdk-napi';
import picker from '@ohos.file.picker';

const SPEEDS: number[] = [0.5, 1.0, 1.5, 2.0];

@Entry
@Component
struct Index {
  private controller: MdkPlayerController = new MdkPlayerController();
  private progressTimer: number = -1;
  @State isPlaying: boolean = false;
  @State currentPos: number = 0;
  @State duration: number = 0;
  @State isSeeking: boolean = false;
  @State volume: number = 1.0;
  @State isMuted: boolean = false;
  @State speedIndex: number = 1;
  @State url: string = 'https://ks3-cn-beijing.ksyun.com/ksplayer/h265/outside_demo/v1.1.3/720P2M30fpsh265-wasmtest.flv';
  @State isFullscreen: boolean = false;
  @State videoReady: boolean = false;
  @State mediaStatus: number = MediaStatus.NoMedia;
  @State vfxBrightness: number = 0;
  @State vfxSaturation: number = 0;
  @StorageProp('openFileUri') @Watch('onOpenFileUriChanged') openFileUri: string = '';

  aboutToAppear(): void {
    this.controller.setDecoders(['OH', 'ohcodec:copy=1', 'FFmpeg']);
    const initialUri = this.openFileUri;
    if (initialUri) {
      this.url = initialUri;
      AppStorage.setOrCreate<string>('openFileUri', '');
    }
    this.controller.setMedia(this.url);
    this.controller.play();
    this.isPlaying = true;
    this.startProgressTimer();
  }

  aboutToDisappear(): void {
    this.stopProgressTimer();
    this.controller.dispose();
  }

  private startProgressTimer(): void {
    this.progressTimer = setInterval((): void => {
      if (!this.isSeeking) {
        this.currentPos = this.controller.getPosition();
        const dur = this.controller.getDuration();
        if (dur > 0) {
          this.duration = dur;
        }
      }
      this.mediaStatus = this.controller.getMediaStatus();
    }, 500);
  }

  private stopProgressTimer(): void {
    if (this.progressTimer !== -1) {
      clearInterval(this.progressTimer);
      this.progressTimer = -1;
    }
  }

  private onOpenFileUriChanged(): void {
    const uri = this.openFileUri;
    if (!uri) {
      return;
    }
    AppStorage.setOrCreate<string>('openFileUri', '');
    this.url = uri;
    this.currentPos = 0;
    this.duration = 0;
    this.controller.setMedia(uri);
    this.controller.play();
    this.isPlaying = true;
  }

  private formatTime(ms: number): string {
    const totalSec = Math.floor(ms / 1000);
    const h = Math.floor(totalSec / 3600);
    const m = Math.floor((totalSec % 3600) / 60);
    const s = totalSec % 60;
    const mm = m.toString().padStart(2, '0');
    const ss = s.toString().padStart(2, '0');
    return h > 0 ? `${h}:${mm}:${ss}` : `${mm}:${ss}`;
  }

  build() {
    Column() {
      // Video area — double tap to toggle fullscreen
      Stack() {
        Stack() {
          MdkPlayerView({
            controller: this.controller
          })
        }
        .width('100%')
        .height('100%')
        .border({ width: 1, color: Color.White, style: BorderStyle.Solid })
        .visibility(this.videoReady ? Visibility.Visible : Visibility.Hidden)
      }
      .width('100%')
      .layoutWeight(1)
      .alignContent(Alignment.Center)
      .onAreaChange((_old, area) => {
        if ((area.width as number) > 0 && (area.height as number) > 0) {
          this.videoReady = true;
        }
      })
      .gesture(
        TapGesture({ count: 2 })
          .onAction(() => {
            this.isFullscreen = !this.isFullscreen;
          })
      )

      if (!this.isFullscreen) {

      Scroll() {
        Column({ space: 0 }) {

      // Progress bar
      Column({ space: 4 }) {
        Slider({
          value: this.duration > 0 ? (this.currentPos / this.duration) * 100 : 0,
          min: 0,
          max: 100,
          step: 0.1,
          style: SliderStyle.OutSet
        })
          .width('100%')
          .trackColor(Color.Gray)
          .selectedColor(Color.White)
          .onChange((value: number) => {
            this.isSeeking = true;
            this.currentPos = Math.floor((value / 100) * this.duration);
          })
          .onTouch((event: TouchEvent) => {
            if (event.type === TouchType.Up || event.type === TouchType.Cancel) {
              // Accurate seek by default (no KeyFrame flag)
              this.controller.seek(this.currentPos, SeekFlag.FromStart);
              this.isSeeking = false;
            }
          })

        Row() {
          Text(this.formatTime(this.currentPos))
            .fontColor(Color.White)
            .fontSize(12)
          Blank()
          Text(this.formatTime(this.duration))
            .fontColor(Color.White)
            .fontSize(12)
        }
        .width('100%')
      }
      .margin({ top: 8 })
      .padding({ left: 4, right: 4 })

      // MediaStatus badges
      Flex({ wrap: FlexWrap.Wrap }) {
        Text('Loading')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Loading) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Loading) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('Loaded')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Loaded) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Loaded) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('Buffering')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Buffering) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Buffering) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('Buffered')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Buffered) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Buffered) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('Seeking')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Seeking) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Seeking) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('End')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.End) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.End) !== 0 ? '#0070c0' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
        Text('Error')
          .fontSize(10)
          .fontColor((this.mediaStatus & MediaStatus.Invalid) !== 0 ? Color.White : '#666666')
          .backgroundColor((this.mediaStatus & MediaStatus.Invalid) !== 0 ? '#cc0000' : '#222222')
          .borderRadius(4)
          .padding({ left: 4, right: 4, top: 2, bottom: 2 })
      }
      .width('100%')
      .margin({ top: 8 })
      .padding({ left: 4, right: 4 })

      // SeekFlag demo buttons
      Row({ space: 8 }) {
        Button('−10s Fast')
          .fontSize(12)
          .onClick(() => {
            // Fast (key-frame only) seek relative to current position
            this.controller.seek(-10000, SeekFlag.KeyFrame | SeekFlag.FromNow);
          })
        Button('+10s Fast')
          .fontSize(12)
          .onClick(() => {
            this.controller.seek(10000, SeekFlag.KeyFrame | SeekFlag.FromNow);
          })
        Button('+10s Accurate')
          .fontSize(12)
          .onClick(() => {
            // Accurate seek: no KeyFrame flag
            this.controller.seek(this.currentPos + 10000, SeekFlag.FromStart);
          })
      }
      .margin({ top: 8 })

      // VideoEffect: brightness and saturation
      Column({ space: 4 }) {
        Row({ space: 8 }) {
          Text('Brightness')
            .fontColor(Color.White)
            .fontSize(12)
            .width(80)
          Slider({ value: this.vfxBrightness * 100, min: -100, max: 100, step: 1, style: SliderStyle.OutSet })
            .layoutWeight(1)
            .trackColor(Color.Gray)
            .selectedColor(Color.White)
            .onChange((v: number) => {
              this.vfxBrightness = v / 100;
              this.controller.setVideoEffect(VideoEffect.Brightness, this.vfxBrightness);
            })
          Text(`${Math.round(this.vfxBrightness * 100)}`)
            .fontColor(Color.White)
            .fontSize(12)
            .width(36)
            .textAlign(TextAlign.End)
        }
        .width('100%')
        .padding({ left: 4, right: 4 })

        Row({ space: 8 }) {
          Text('Saturation')
            .fontColor(Color.White)
            .fontSize(12)
            .width(80)
          Slider({ value: this.vfxSaturation * 100, min: -100, max: 100, step: 1, style: SliderStyle.OutSet })
            .layoutWeight(1)
            .trackColor(Color.Gray)
            .selectedColor(Color.White)
            .onChange((v: number) => {
              this.vfxSaturation = v / 100;
              this.controller.setVideoEffect(VideoEffect.Saturation, this.vfxSaturation);
            })
          Text(`${Math.round(this.vfxSaturation * 100)}`)
            .fontColor(Color.White)
            .fontSize(12)
            .width(36)
            .textAlign(TextAlign.End)
        }
        .width('100%')
        .padding({ left: 4, right: 4 })
      }
      .margin({ top: 8 })

      // Playback controls
      Row({ space: 12 }) {
        Button(this.isPlaying ? 'Pause' : 'Play')
          .onClick(() => {
            if (this.isPlaying) {
              this.controller.pause();
            } else {
              this.controller.play();
            }
            this.isPlaying = !this.isPlaying;
          })

        Button('Stop')
          .onClick(() => {
            this.controller.stop();
            this.isPlaying = false;
          })

        // Speed cycle button
        Button(`${SPEEDS[this.speedIndex]}x`)
          .onClick(() => {
            this.speedIndex = (this.speedIndex + 1) % SPEEDS.length;
            this.controller.setPlaybackRate(SPEEDS[this.speedIndex]);
          })
      }
      .margin({ top: 8 })

      // Volume row  (slider 0-100 → volume 0-4x, quadratic mapping)
      Row({ space: 8 }) {
        Button(this.isMuted ? 'Unmute' : 'Mute')
          .width(80)
          .onClick(() => {
            this.isMuted = !this.isMuted;
            this.controller.setVolume(this.isMuted ? 0 : this.volume);
          })

        Slider({
          value: this.isMuted ? 0 : Math.sqrt(this.volume / 4) * 100,
          min: 0,
          max: 100,
          step: 0.5,
          style: SliderStyle.OutSet
        })
          .layoutWeight(1)
          .trackColor(Color.Gray)
          .selectedColor(Color.White)
          .onChange((value: number) => {
            const t = value / 100;
            this.volume = t * t * 4;
            this.isMuted = this.volume === 0;
            this.controller.setVolume(this.volume);
          })

        Text(`${Math.round(this.isMuted ? 0 : this.volume * 100)}%`)
          .fontColor(Color.White)
          .fontSize(12)
          .width(42)
          .textAlign(TextAlign.End)
      }
      .width('100%')
      .margin({ top: 8 })
      .padding({ left: 4, right: 4 })

      TextInput({ placeholder: 'Enter URL', text: this.url })
        .fontColor(Color.White)
        .margin({ top: 16 })
        .onChange((val) => {
          this.url = val;
        })

      Button('Load URL')
        .margin({ top: 8 })
        .onClick(() => {
          this.currentPos = 0;
          this.duration = 0;
          this.controller.setMedia(this.url);
          this.controller.play();
          this.isPlaying = true;
        })

      Button('Open File')
        .margin({ top: 8 })
        .onClick(() => {
          const documentPicker = new picker.DocumentViewPicker();
          const selectOptions = new picker.DocumentSelectOptions();
          selectOptions.maxSelectNumber = 1;
          documentPicker.select(selectOptions).then((uris: Array<string>) => {
            if (uris.length > 0) {
              this.url = uris[0];
              this.currentPos = 0;
              this.duration = 0;
              this.controller.setMedia(this.url);
              this.controller.play();
              this.isPlaying = true;
            }
          }).catch((err: Error) => {
            console.error('DocumentViewPicker select failed: ' + err.message);
          });
        })

        } // end Column
      } // end Scroll

      } // end if (!this.isFullscreen)
    }
    .width('100%')
    .height('100%')
    .padding(this.isFullscreen ? 0 : 16)
    .backgroundColor(Color.Black)
  }
}
