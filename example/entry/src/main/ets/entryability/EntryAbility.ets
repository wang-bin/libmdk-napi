import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'EntryAbility';
const DOMAIN = 0x0001;

export default class EntryAbility extends UIAbility {
  private handleWant(want: Want): void {
    hilog.info(DOMAIN, TAG, 'handleWant: %{public}s', JSON.stringify(want));
    const uri = want.uri ?? '';
    if (uri) {
      AppStorage.setOrCreate<string>('openFileUri', uri);
    }
  }

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'onCreate want: %{public}s', JSON.stringify(want));
    this.handleWant(want);
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG, 'onNewWant want: %{public}s', JSON.stringify(want));
    this.handleWant(want);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    try {
      windowStage.loadContent('pages/Index');
    } catch (err) {
      // ignore
    }
  }

  onWindowStageDestroy(): void {}
  onForeground(): void {}
  onBackground(): void {}
}
