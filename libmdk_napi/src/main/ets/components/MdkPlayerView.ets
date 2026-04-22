import { MdkPlayerController } from '../controller/MdkPlayerController';

@Component
export struct MdkPlayerView {
  controller: MdkPlayerController = new MdkPlayerController();
  playerBgColor: ResourceColor = Color.Black;

  build() {
    Stack() {
      XComponent({
        id: this.controller.playerId,
        type: XComponentType.SURFACE,
        libraryname: 'mdk_napi'
      })
        .width('100%')
        .height('100%')
        .backgroundColor(this.playerBgColor)
    }
    .width('100%')
    .height('100%')
    .onAreaChange((_old, area) => {
      const pw = vp2px(area.width as number);
      const ph = vp2px(area.height as number);
      if (pw > 0 && ph > 0) {
        this.controller.setVideoSurfaceSize(pw, ph);
      }
    })
  }
}