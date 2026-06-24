# wvbridge

**语言：** [English](README.MD) | 简体中文

| 产物         | 最新版本                                                                                                                                                                                   |
|------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `core`     | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/core?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/core)            |
| `jsbridge` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/jsbridge?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/jsbridge)    |

| 平台产物               | 最新版本                                                                                                                                                                                                |
|--------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `platform-windows` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-windows?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-windows) |
| `platform-linux`   | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-linux?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-linux)     |
| `platform-macos`   | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/platform-macos?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/platform-macos)     |

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

创建一个 remembered controller，然后渲染 `WebView`：

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

基础导航通过 `webViewController.navigator` 完成：

```kotlin
webViewController.navigator.loadUrl("https://kotlinlang.org")
webViewController.navigator.goBack()
webViewController.navigator.goForward()
webViewController.navigator.refresh()
webViewController.navigator.stop()
```

更多特性请参考 [wvbridge.kagg886.top](https://wvbridge.kagg886.top)。

## 特性

图例：✅ 已实现；❌ 未实现；⚠️ 已实现，但存在限制；🚧 施工中；🧪 未测试。

| 功能                          | Android | iOS | Windows JVM | Linux JVM | macOS JVM |
|-----------------------------|---------|-----|-------------|-----------|-----------|
| 基本导航（前进、后退、加载、停止、刷新）        | ✅       | ✅   | ✅           | ✅         | ✅         |
| URL 监听                      | ✅       | ✅   | ✅           | ✅         | ✅         |
| 加载状态监听                      | ✅       | ✅   | ✅           | ✅         | ✅         |
| 同步执行 JS 代码                  | ✅       | 🚧  | ✅           | 🚧        | 🚧        |
| 添加网页脚本（document-start hook） | 🚧      | 🚧  | ✅           | 🚧        | 🚧        |
| 报错自动销毁                      | 🚧      | 🚧  | ✅           | 🚧        | 🚧        |
| 离屏渲染                        | ✅ 系统能力  | 🧪  | ❌           | ❌         | ❌         |
