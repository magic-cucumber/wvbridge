# wvbridge

**Language:** English | [简体中文](README_ZH.MD)

| Artifact   | Latest version                                                                                                                                                                      |
|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `core`     | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/core?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/core)         |
| `jsbridge` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/jsbridge?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/jsbridge) |

| Platform artifact  | Latest version                                                                                                                                                                                      |
|--------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `platform-windows` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-windows?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-windows) |
| `platform-linux`   | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-linux?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-linux)     |
| `platform-macos`   | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-macos?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-macos)     |

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

Create a remembered controller and render `WebView`:

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

Basic navigation uses `webViewController.navigator`:

```kotlin
webViewController.navigator.loadUrl("https://kotlinlang.org")
webViewController.navigator.goBack()
webViewController.navigator.goForward()
webViewController.navigator.refresh()
webViewController.navigator.stop()
```

More features are documented at [wvbridge.kagg886.top](https://wvbridge.kagg886.top).

## Features

Legend: ✅ Implemented; ❌ Not implemented; ⚠️ Implemented with limitations; 🚧 Under construction; 🧪 Untested.

| Feature                                               | Android             | iOS | Windows JVM | Linux JVM | macOS JVM |
|-------------------------------------------------------|---------------------|-----|-------------|-----------|-----------|
| Basic navigation (forward, back, load, stop, refresh) | ✅                   | ✅   | ✅           | ✅         | ✅         |
| URL listener                                          | ✅                   | ✅   | ✅           | ✅         | ✅         |
| Loading state listener                                | ✅                   | ✅   | ✅           | ✅         | ✅         |
| Synchronous JS execution                              | ✅                   | 🚧  | ✅           | 🚧        | 🚧        |
| Add page script (document-start hook)                 | ✅                   | 🚧  | ✅           | 🚧        | 🚧        |
| Auto destroy on error                                 | 🚧                  | 🚧  | 🚧          | ✅         | 🚧        |
| Off-screen rendering                                  | ✅ System capability | 🧪  | ❌           | ❌         | ❌         |
