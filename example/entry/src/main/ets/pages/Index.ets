import { MdkPlayerController, MdkPlayerView } from '@mediadevkit/libmdk-napi';

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

  aboutToAppear(): void {
    this.controller.setDecoders(['OH', 'ohcodec:copy=1', 'FFmpeg']);
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
    this.progressTimer = setInterval(() => {
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
        if (this.videoReady) {
          MdkPlayerView({
            controller: this.controller
          })
            .width('100%')
            .height('100%')
            .border({ width: 1, color: Color.White, style: BorderStyle.Solid })
        }
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
              this.controller.seek(this.currentPos);
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
