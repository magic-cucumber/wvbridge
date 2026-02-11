# wvbridge

**语言：** [English](README.MD) | 简体中文

wvbridge 是一个 **Compose Multiplatform** 库，用于在各个平台上接入**宿主系统的 WebView**，并提供一套统一、对 Kotlin 友好的 API，用来桥接常见的 WebView 能力。

> 注意：本项目仍处于活跃开发阶段，API 与平台覆盖范围可能会发生变化。

## 项目状态

### 计划（Roadmap）

**已完成**
- macOS AWT 实现

**待完成 / TODO**
- Windows AWT 实现
- Linux AWT 实现
- iOS 实现
- Android 实现
- common（共享）平台 API 设计

### 特性矩阵

图例：✅ 已实现 · 🚧 施工中 · ❌ 未实现 · ⛔ 无法支持

| 特性 | macOS | Windows | Linux | iOS | Android |
| --- | --- | --- | --- | --- | --- |
| 请求拦截 | ✅ | ❌ | ❌ | ❌ | ❌ |
| 请求修改 | ❌ | ❌ | ❌ | ❌ | ❌ |
| 进度监听 | ✅ | ❌ | ❌ | ❌ | ❌ |
| 主动加载 URL | ✅ | ❌ | ❌ | ❌ | ❌ |
| JS 注入 | ❌ | ❌ | ❌ | ❌ | ❌ |

## common 平台设计（高层说明）

目标是在 `core` 中提供**单一的 common API**，并由各个平台提供各自的后端实现：

- `core`：通用接口、事件与共享逻辑
- `platform/*`：平台侧 WebView 适配/后端实现
- `sample/*`：示例应用

## 运行示例应用

- Desktop JVM：`./gradlew :sample:composeApp:run`
- Android：`open project in Android Studio and run the sample app`
- iOS：`open 'sample/iosApp/iosApp.xcodeproj' in Xcode and run the sample app`
- JavaScript：`./gradlew :sample:composeApp:jsBrowserRun`
- Wasm：`./gradlew :sample:composeApp:wasmJsBrowserRun`

## 发布到 MavenLocal

1) 运行 `./gradlew :lib:publishToMavenLocal`
2) 打开 `~/.m2/repository/top/kagg886/wvbridge/`

## 发布到 MavenCentral

1) 在 Sonatype 创建账号与 namespace：  
   https://central.sonatype.org/register/central-portal/#create-an-account
2) 在 `./lib/build.gradle.kts` 中添加 developer id、name、email 与项目 URL  
3) 生成 GPG key：  
   https://getstream.io/blog/publishing-libraries-to-mavencentral-2021/#generating-a-gpg-key-pair
   ```
   gpg --full-gen-key
   gpg --keyserver keyserver.ubuntu.com --send-keys XXXXXXXX
   gpg --export-secret-key XXXXXXXX > XXXXXXXX.gpg
   ```
4) 在 `gradle.properties` 中添加：
   ```
   signing.keyId=XXXXXXXX
   signing.password=[key password]
   signing.secretKeyRingFile=../XXXXXXXX.gpg
   mavenCentralUsername=[generated username]
   mavenCentralPassword=[generated password]
   ```
5) 运行 `./gradlew :lib:publishAndReleaseToMavenCentral --no-configuration-cache`
