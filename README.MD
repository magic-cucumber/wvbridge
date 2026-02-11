# wvbridge

**Language:** English | [简体中文](README_ZH.MD)

wvbridge is a **Compose Multiplatform** library that embeds the **host system WebView** and provides a unified, Kotlin-friendly API for bridging common WebView capabilities across platforms.

> Note: This project is under active development. APIs and platform coverage may change.

## Project Status

### Roadmap

**Done**
- macOS AWT implementation

**Planned / TODO**
- Windows AWT implementation
- Linux AWT implementation
- iOS implementation
- Android implementation
- Common (shared) platform API design

### Feature Matrix

Legend: ✅ Implemented · 🚧 In progress · ❌ Not implemented · ⛔ Not supported

| Feature | macOS | Windows | Linux | iOS | Android |
| --- | --- | --- | --- | --- | --- |
| Request interception | ✅ | ❌ | ❌ | ❌ | ❌ |
| Request modification | ❌ | ❌ | ❌ | ❌ | ❌ |
| Progress listener | ✅ | ❌ | ❌ | ❌ | ❌ |
| Programmatic URL loading | ✅ | ❌ | ❌ | ❌ | ❌ |
| JavaScript injection | ❌ | ❌ | ❌ | ❌ | ❌ |

## Common Platform Design (High-level)

The goal is to expose a **single common API** from `core`, while each platform provides its own backend implementation:

- `core`: common interfaces, events, and shared logic
- `platform/*`: platform-specific WebView adapters/backends
- `sample/*`: demo apps

## Run Sample App

- Desktop JVM: `./gradlew :sample:composeApp:run`
- Android: `open project in Android Studio and run the sample app`
- iOS: `open 'sample/iosApp/iosApp.xcodeproj' in Xcode and run the sample app`
- JavaScript: `./gradlew :sample:composeApp:jsBrowserRun`
- Wasm: `./gradlew :sample:composeApp:wasmJsBrowserRun`

## Publish to MavenLocal

1) Run `./gradlew :lib:publishToMavenLocal`
2) Open `~/.m2/repository/top/kagg886/wvbridge/`

## Publish to MavenCentral

1) Create an account and a namespace on Sonatype:  
   https://central.sonatype.org/register/central-portal/#create-an-account
2) Add developer id, name, email and the project url to  
   `./lib/build.gradle.kts`
3) Generate a GPG key:  
   https://getstream.io/blog/publishing-libraries-to-mavencentral-2021/#generating-a-gpg-key-pair
   ```
   gpg --full-gen-key
   gpg --keyserver keyserver.ubuntu.com --send-keys XXXXXXXX
   gpg --export-secret-key XXXXXXXX > XXXXXXXX.gpg
   ```
4) Add these lines to `gradle.properties`:
   ```
   signing.keyId=XXXXXXXX
   signing.password=[key password]
   signing.secretKeyRingFile=../XXXXXXXX.gpg
   mavenCentralUsername=[generated username]
   mavenCentralPassword=[generated password]
   ```
5) Run `./gradlew :lib:publishAndReleaseToMavenCentral --no-configuration-cache`
