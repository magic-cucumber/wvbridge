# Module wvbridge:jsbridge

| Artifact   | Latest version                                                                                                                                                                      |
|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `jsbridge` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/jsbridge?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/jsbridge) |

`wvbridge:jsbridge` extends `wvbridge:core` with a small helper layer for JavaScript
evaluation and WebView messages. It runs JavaScript through the controller's `JavaScriptBridge`
and normalizes platform-specific values into a common Kotlin `JSValue` model.

Documentation site: [wvbridge.kagg886.top](https://wvbridge.kagg886.top)

> Note: This project is under active development. APIs and platform coverage may change.

## Installation

Add `jsbridge` to your shared source set:

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("top.kagg886.wvbridge:jsbridge:<version>")
        }
    }
}
```

On JVM, keep the same platform-specific runtime setup required by `wvbridge:core`:

## Quick Start

### Typed JavaScript Evaluation

Use `evaluateScriptValue` on the controller's JavaScript bridge to execute a script and receive
a typed `JSValue` result:

```kotlin
import top.kagg886.wvbridge.js.evaluateScriptValue
import top.kagg886.wvbridge.js.protocol.JSValue

val result = webViewController.bridge.evaluateScriptValue(
    """
    return {
        title: document.title,
        href: location.href,
    }
    """.trimIndent()
)

when (result) {
    is JSValue.Serializable -> println(result.value)
    is JSValue.ScriptObject -> println("${result.type}: ${result.value}")
    is JSValue.Error -> println(result.stacktrace)
    JSValue.Null -> println("null")
    JSValue.Undefined -> println("undefined")
}
```

`evaluateScriptValue` treats the script as a JavaScript function body. Use `return` when you want
to produce a value.

### Typed Message Bridge

Use `registerWebMessageHandler(type)` to receive typed messages posted from JavaScript. The
handler only receives packets whose type matches the registered type, and the payload is decoded
as `JSValue`.

```kotlin
import top.kagg886.wvbridge.js.registerWebMessageHandler
import top.kagg886.wvbridge.js.protocol.JSValue

val closeHandle = webViewController.bridge.registerWebMessageHandler("profile:update") { message ->
    when (message) {
        is JSValue.Serializable -> println(message.value)
        is JSValue.ScriptObject -> println("${message.type}: ${message.value}")
        is JSValue.Error -> println(message.stacktrace)
        JSValue.Null -> println("null")
        JSValue.Undefined -> println("undefined")
    }
}
```

After the handler is registered, pages can post typed messages through `window.wvbridge`:

```javascript
window.wvbridge.postMessage("profile:update", {
    name: "Kagg886",
    loggedIn: true,
});
```

Call `closeHandle.close()` when the native handler is no longer needed.

## Result Values

| Value type             | Meaning                                                                  |
|------------------------|--------------------------------------------------------------------------|
| `JSValue.Serializable` | A JavaScript value safely serialized with `JSON.stringify`.              |
| `JSValue.ScriptObject` | A value that cannot be represented faithfully as JSON.                   |
| `JSValue.Error`        | JavaScript evaluation threw an exception.                                |
| `JSValue.Null`         | JavaScript returned `null`.                                              |
| `JSValue.Undefined`    | JavaScript returned `undefined`, or the backend returned no result text. |

## Features

Legend: ✅ Supported; 🚧 Under construction; 🧪 Untested.

| Feature                          | Android | iOS | Windows JVM | Linux JVM | macOS JVM |
|----------------------------------|---------|-----|-------------|-----------|-----------|
| Typed JavaScript code evaluation | ✅       | ✅   | ✅           | 🧪        | ✅         |
| `postMessage`                    | ✅       | ✅   | ✅           | ✅         | ✅         |
