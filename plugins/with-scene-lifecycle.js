// Adopts the UIScene life cycle, which iOS 27 requires at launch (apps without it crash with
// _UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption). Expo SDK 57 ships
// ExpoAppSceneDelegate for this, but the prebuild template's AppDelegate still creates the
// window itself. Remove this plugin once the Expo template adopts scenes.
const { withAppDelegate, withInfoPlist } = require('expo/config-plugins');

const WINDOW_SETUP = /\n#if os\(iOS\) \|\| os\(tvOS\)\n\s*window = UIWindow\(frame: UIScreen\.main\.bounds\)\n\s*factory\.startReactNative\([\s\S]*?\)\n#endif\n/;

function withSceneLifecycle(config) {
  config = withInfoPlist(config, (config) => {
    config.modResults.UIApplicationSceneManifest = {
      UIApplicationSupportsMultipleScenes: false,
      UISceneConfigurations: {
        UIWindowSceneSessionRoleApplication: [
          {
            UISceneConfigurationName: 'Default Configuration',
            UISceneDelegateClassName: 'EXExpoAppSceneDelegate',
          },
        ],
      },
    };
    return config;
  });

  config = withAppDelegate(config, (config) => {
    if (config.modResults.language !== 'swift') {
      throw new Error('with-scene-lifecycle expects a Swift AppDelegate');
    }
    let contents = config.modResults.contents;
    if (contents.includes('ExpoReactNativeFactoryProvider')) {
      return config;
    }

    const declaration = 'class AppDelegate: ExpoAppDelegate {';
    if (!contents.includes(declaration) || !WINDOW_SETUP.test(contents)) {
      throw new Error('with-scene-lifecycle: AppDelegate.swift no longer matches the expected template');
    }
    contents = contents.replace(declaration, 'class AppDelegate: ExpoAppDelegate, ExpoReactNativeFactoryProvider {');
    // ExpoAppSceneDelegate creates the window and starts React Native when the scene connects
    contents = contents.replace(WINDOW_SETUP, '\n');
    config.modResults.contents = contents;
    return config;
  });

  return config;
}

module.exports = withSceneLifecycle;
