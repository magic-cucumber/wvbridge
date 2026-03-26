# wvbridge

**Language:** English | [简体中文](README_ZH.MD)

`wvbridge` is a **Compose Multiplatform** library that embeds the **host platform WebView** and
exposes a small, Kotlin-friendly common API for loading pages, observing URL and loading state
changes, and driving basic browser navigation.

Documentation site: [wvbridge.kagg886.top](https://wvbridge.kagg886.top)

> Note: This project is under active development. APIs and platform coverage may change.

## Installation

Add the common API to your shared source set:

```kotlin
repositories {
    mavenCentral()
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("top.kagg886.wvbridge:core:<version>")
        }
    }
}
```

On JVM, `wvbridge` also needs a platform-specific native runtime library. The recommended approach is
to use Google's `os-detector` Gradle plugin and choose the runtime artifact from
`osdetector.classifier`.

```kotlin
plugins {
    id("com.google.osdetector") version "1.7.3"
}

repositories {
    mavenCentral()
}

kotlin {
    jvm()

    sourceSets {
        jvmMain.dependencies {
            val platform = when (osdetector.classifier) {
                "windows-x86_64" -> "platform-windows"
                "linux-x86_64" -> "platform-linux"
                "osx-aarch_64" -> "platform-macos"
                else -> error(
                    "Unsupported JVM runtime for wvbridge: ${osdetector.classifier}. " +
                        "Supported classifiers are windows-x86_64, linux-x86_64, and osx-aarch_64."
                )
            }

            runtimeOnly("top.kagg886.wvbridge:$platform:<version>")
        }
    }
}
```

Current JVM native runtime support is limited to:

- Windows x64: `windows-x86_64`
- Linux x64: `linux-x86_64`
- macOS arm64: `osx-aarch_64`

## Quick Start

Create a remembered state and render `WebView`:

```kotlin
import androidx.compose.ui.Modifier
import top.kagg886.wvbridge.WebView
import top.kagg886.wvbridge.rememberWebViewState

val webViewState = rememberWebViewState("https://example.com")

WebView(
    state = webViewState,
    modifier = Modifier,
)
```

Basic navigation uses `WebViewState.navigator`:

```kotlin
webViewState.navigator.loadUrl("https://kotlinlang.org")
webViewState.navigator.goBack()
webViewState.navigator.goForward("")
webViewState.navigator.refresh()
webViewState.navigator.stop()
```

More features are documented at [wvbridge.kagg886.top](https://wvbridge.kagg886.top).

## Notes

- `WebViewState.url` is suitable for address-bar synchronization and may contain custom URL schemes
  if the underlying native engine supports them.
- Desktop backends use native on-screen rendered WebViews. Compose or Swing content cannot be drawn
  above the WebView on desktop platforms.
- The library intentionally wraps the platform-native WebView instead of embedding a Chromium-based
  browser engine.

## Run Sample App

- Desktop JVM: `./gradlew :sample:compose:desktopApp:run`
- Android: open the project in Android Studio and run `sample/compose/androidApp`
- iOS: open `sample/compose/iosApp/iosApp.xcodeproj` in Xcode and run the sample app

## Project Layout

- `core`: common interfaces, states, and Compose API
- `platform/*`: JVM native runtime backends for each desktop platform
- `sample/*`: sample applications
