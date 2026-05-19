import {
  ColorSpace,
  MdkPlayerController,
  MdkPlayerView,
  MediaStatus,
  setGlobalOption,
  setResourceManager,
  SeekFlag,
  VideoEffect,
} from '@mediadevkit/libmdk-napi';
import common from '@ohos.app.ability.common';
import picker from '@ohos.file.picker';
import promptAction from '@ohos.promptAction';

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
  @State showMediaInfoDialog: boolean = false;
  @State mediaInfoDialogText: string = '';
  @StorageProp('openFileUri') @Watch('onOpenFileUriChanged') openFileUri: string = '';

  aboutToAppear(): void {
    //this.controller.setDecoders(['OH', 'ohcodec:copy=1', 'FFmpeg']);
    const initialUri = this.openFileUri;
    if (initialUri) {
      this.url = initialUri;
      AppStorage.setOrCreate<string>('openFileUri', '');
    }
    this.controller.setColorSpace(ColorSpace.Unknown);
    this.controller.setMedia(this.url);
    //const ctx = AppStorage.get('abilityContext') as Context;
    const hostContext = this.getUIContext().getHostContext() as common.UIAbilityContext;
    setResourceManager(hostContext.resourceManager);
    //this.controller.setMedia("rawfile://mercury.mov"); // resouces/rawfile/mercury.mov
    //setGlobalOption('subtitle.fonts.file', "rawfile:fonts/subfont.ttf");
    this.controller.onMediaStatus((_oldStatus: MediaStatus, newStatus: MediaStatus): void => {
      this.mediaStatus = newStatus;
    });
    this.mediaStatus = this.controller.getMediaStatus();
    this.controller.prepare(0, (pos: number): void => {
      this.showToast(`prepare done at ${this.formatTime(pos)} (${pos} ms)`);
    });

    //this.controller.play();

    this.isPlaying = false;
    this.startProgressTimer();
  }

  aboutToDisappear(): void {
    this.stopProgressTimer();
    this.controller.onMediaStatus(null);
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
    this.controller.setColorSpace(ColorSpace.Unknown); // hdr
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

  private showToast(message: string): void {
    try {
      promptAction.showToast({
        message,
        duration: 1500,
      });
    } catch (_err) {
    }
  }

  private mediaInfoText(): string {
    const info = this.controller.getMediaInfo();
    const text = JSON.stringify(
      info,
      (key: string, value: Object | string | number | boolean | null | undefined) => {
        if (value === null || value === undefined) {
          return undefined;
        }
        if (typeof value === 'string' && value.length === 0) {
          return undefined;
        }
        if (typeof value === 'boolean' && value === false) {
          return undefined;
        }
        if (typeof value === 'number') {
          if (!Number.isFinite(value)) {
            return undefined;
          }
          if (value === 0 && key !== 'index') {
            return undefined;
          }
        }
        if (Array.isArray(value) && value.length === 0) {
          return undefined;
        }
        if (typeof value === 'object' && !Array.isArray(value) && Object.keys(value).length === 0) {
          return undefined;
        }
        return value;
      },
      2
    );
    return text ?? '{}';
  }

  build() {
    Stack() {
    Column() {
      // Video area — double tap to toggle fullscreen
      Stack() {
        Stack() {
          MdkPlayerView({
            controller: this.controller,
            playerBgColor: Color.Green,
          })
            //.backgroundColor(Color.Green)
            .width('100%')
            .height('100%')
            .border({ width: 1, color: Color.White, style: BorderStyle.Solid })
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
              this.controller.seek(this.currentPos, SeekFlag.FromStart, (pos: number) => {
                this.currentPos = pos;
                this.showToast(`seek result: ${this.formatTime(pos)} (${pos} ms)`);
              });
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
      .margin({ top: 4 })
      .padding({ left: 2, right: 2 })

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
      .margin({ top: 4 })
      .padding({ left: 2, right: 2 })

      // VideoEffect: brightness and saturation
      Column({ space: 4 }) {
        Row({ space: 6 }) {
          Text('Brightness')
            .fontColor(Color.White)
            .fontSize(12)
            .width(72)
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
        .padding({ left: 2, right: 2 })

        Row({ space: 6 }) {
          Text('Saturation')
            .fontColor(Color.White)
            .fontSize(12)
            .width(72)
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
        .padding({ left: 2, right: 2 })
      }
      .margin({ top: 6 })

      // Playback controls
      Row({ space: 8 }) {
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
      .margin({ top: 6 })

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
      .margin({ top: 6 })
      .padding({ left: 2, right: 2 })

      TextInput({ placeholder: 'Enter URL', text: this.url })
        .fontColor(Color.White)
        .margin({ top: 10 })
        .onChange((val) => {
          this.url = val;
        })

      Row({ space: 6 }) {
        Button('Load URL')
          .layoutWeight(1)
          .onClick(() => {
            this.currentPos = 0;
            this.duration = 0;
            this.controller.setColorSpace(ColorSpace.Unknown);
            this.controller.setMedia(this.url);
            this.controller.play();
            this.isPlaying = true;
          })

        Button('Files')
          .layoutWeight(1)
          .onClick(() => {
            const documentPicker = new picker.DocumentViewPicker();
            const selectOptions = new picker.DocumentSelectOptions();
            selectOptions.maxSelectNumber = 20;
            // Hint by filtering selectable files to fonts, subtitles and media.
            selectOptions.fileSuffixFilters = [
              '.ttf',
              '.srt', '.ass', '.ssa', '.vtt', '.sup', '.txt', '.lrc',
              '.mp4', '.mkv', '.mov', '.flv', '.webm', '.avi', '.ts', '.wmv', '.mxf', 'mpg', '.m4s', '.rm', '.rmvb',
              '.jpg', '.jpeg', '.png', '.gif', '.bmp',
              '.mp3', '.aac', '.wav', '.m4a', '.flac', '.ape'
            ];
            documentPicker.select(selectOptions).then((uris: Array<string>) => {
              let mediaUri: string = '';
              uris.forEach((uri: string) => {
                if (uri.toLowerCase().endsWith('.ttf')) {
                  setGlobalOption('subtitle.fonts.file', uri);
                } else if (!mediaUri) {
                  mediaUri = uri;
                }
              });

              if (mediaUri) {
                this.url = mediaUri;
                this.currentPos = 0;
                this.duration = 0;
                this.controller.setColorSpace(ColorSpace.Unknown);

/*
                const file = fs.openSync( this.url, fs.OpenMode.READ_ONLY);
                const fdUrl = `fd://${file.fd}`;
                this.controller.setMedia(fdUrl);*/

                this.controller.setMedia(mediaUri);
                this.controller.play();
                this.isPlaying = true;
              }
            }).catch((err: Error) => {
              console.error('DocumentViewPicker select failed: ' + err.message);
            });
          })

        Button('Info')
          .layoutWeight(1)
          .onClick(() => {
            this.mediaInfoDialogText = this.mediaInfoText();
            this.showMediaInfoDialog = true;
          })
      }
      .width('100%')
      .margin({ top: 6 })

        } // end Column
      } // end Scroll

      } // end if (!this.isFullscreen)
    }
    .width('100%')
    .height('100%')
    .padding(this.isFullscreen ? 0 : 16)
    .backgroundColor(Color.Black)

    if (this.showMediaInfoDialog) {
      Column() {
        Column({ space: 8 }) {
          Row() {
            Text('MediaInfo')
              .fontColor(Color.White)
              .fontSize(18)
              .fontWeight(FontWeight.Medium)
            Blank()
            Button('Close')
              .fontSize(12)
              .onClick(() => {
                this.showMediaInfoDialog = false;
              })
          }
          .width('100%')

          Scroll() {
            Text(this.mediaInfoDialogText)
              .fontColor(Color.White)
              .fontSize(12)
              .textAlign(TextAlign.Start)
              .width('100%')
          }
          .width('100%')
          .layoutWeight(1)
          .scrollBar(BarState.On)
        }
        .width('92%')
        .height('80%')
        .padding(12)
        .backgroundColor('#1f1f1f')
        .borderRadius(12)
      }
      .width('100%')
      .height('100%')
      .justifyContent(FlexAlign.Center)
      .backgroundColor('#80000000')
    }
    }
    .width('100%')
    .height('100%')
  }
}
