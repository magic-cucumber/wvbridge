# wvbridge

**语言：** [English](README.MD) | 简体中文

`wvbridge` 是一个 **Compose Multiplatform** 库，用于接入**宿主平台自带的 WebView**，并提供一套小而统一、对 Kotlin 友好的 common API，用来加载页面、监听 URL 与加载状态变化，以及执行基础导航操作。

文档站点：[wvbridge.kagg886.top](https://wvbridge.kagg886.top)

> 注意：本项目仍处于活跃开发阶段，API 与平台覆盖范围可能发生变化。

## 安装

先在共享代码的 `commonMain` 中引入 common API：

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

在 JVM 侧，`wvbridge` 还需要额外引入对应平台的原生动态库。推荐使用 Google 的
`os-detector` Gradle 插件，通过 `osdetector.classifier` 选择对应运行时产物。

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

当前 JVM 原生运行时仅支持：

- Windows x64: `windows-x86_64`
- Linux x64: `linux-x86_64`
- macOS arm64: `osx-aarch_64`

## 快速使用

创建一个 remembered state，然后渲染 `WebView`：

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

基础导航通过 `WebViewState.navigator` 完成：

```kotlin
webViewState.navigator.loadUrl("https://kotlinlang.org")
webViewState.navigator.goBack()
webViewState.navigator.goForward("")
webViewState.navigator.refresh()
webViewState.navigator.stop()
```

更多特性请参考 [wvbridge.kagg886.top](https://wvbridge.kagg886.top)。

## 说明

- `WebViewState.url` 很适合同步地址栏；如果底层原生引擎支持，也可以承载自定义 URL scheme。
- 桌面端后端使用的是原生 on-screen 渲染 WebView，因此不能在 WebView 上方叠放 Compose 或 Swing 内容。
- 这个库刻意选择封装平台原生 WebView，而不是引入 Chromium 一类更重的浏览器内核。

## 运行示例

- Desktop JVM：`./gradlew :sample:compose:desktopApp:run`
- Android：使用 Android Studio 打开项目并运行 `sample/compose/androidApp`
- iOS：使用 Xcode 打开 `sample/compose/iosApp/iosApp.xcodeproj` 并运行示例

## 项目结构

- `core`：common 接口、状态与 Compose API
- `platform/*`：各桌面平台对应的 JVM 原生运行时后端
- `sample/*`：示例应用
