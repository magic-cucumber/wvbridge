# Module wvbridge:core

`wvbridge` is a Compose Multiplatform library that embeds the host platform WebView and exposes a
small common API for loading pages, observing URL and loading state changes, and driving basic
browser navigation.

The common entry points are:

- [WebView] to display the native WebView inside Compose UI.
- [rememberWebViewController] to create and remember a [WebViewController].
- [webViewController.url] to observe the current top-level URL.
- [WebViewController.loadingState] to observe readiness and page loading progress through [LoadingState].
- [webViewController.navigator] to perform imperative navigation through [WebViewNavigator].

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
to use Google's `os-detector` Gradle plugin and select the matching runtime artifact from
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

## Simple Usage

Create a remembered controller and render [WebView]:

```kotlin
import androidx.compose.ui.Modifier
import top.kagg886.wvbridge.WebView
import top.kagg886.wvbridge.rememberWebViewController

val webViewController = rememberWebViewController("https://example.com")

WebView(
    controller = webViewController,
    modifier = Modifier,
)
```

To navigate after the first load, use [webViewController.navigator]:

```kotlin
webViewController.navigator.loadUrl("https://kotlinlang.org")
webViewController.navigator.goBack()
webViewController.navigator.goForward()
webViewController.navigator.refresh()
webViewController.navigator.stop()
```

## Typical State Flow

- [rememberWebViewController] creates a [WebViewController] whose initial [WebViewController.loadingState] is
  [LoadingState.NotReady].
- When the native WebView is ready, the loading state moves to [LoadingState.Ready].
- The initial URL is loaded automatically once the loading state becomes [LoadingState.Ready].
- While a page is loading, the loading state becomes [LoadingState.Loading].
- When the navigation finishes or fails, the loading state becomes [LoadingState.LoadingEnd].

## Notes

- [webViewController.url] is suitable for address-bar synchronization and may contain custom URL schemes
  if the underlying native engine supports them.
- Desktop backends use native on-screen rendered WebViews. Compose or Swing content cannot be drawn
  above the WebView on desktop platforms.
- The library intentionally wraps the platform-native WebView instead of embedding a Chromium-based
  browser engine.
