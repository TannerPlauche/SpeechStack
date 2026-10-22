import { registerWebModule, NativeModule } from 'expo';

import { VowelTrackerModuleEvents } from './VowelTracker.types';

// Live formant tracking isn't available on the web; these no-ops let the UI render.
class VowelTrackerModule extends NativeModule<VowelTrackerModuleEvents> {
  async start() {}
  async stop() {}
  setVoiceType() {}
  setVoicingSensitivity() {}
  setInputGain() {}
  async startCapture() {
    return '';
  }
  async stopCapture() {
    return -1;
  }
  async playSound() {}
}

export default registerWebModule(VowelTrackerModule, 'VowelTrackerModule');
