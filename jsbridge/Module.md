# Module wvbridge:jsbridge

| Artifact   | Latest version                                                                                                                                                                      |
|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `jsbridge` | [![Maven Central](https://img.shields.io/maven-central/v/top.kagg886.wvbridge/jsbridge?label=Maven%20Central)](https://central.sonatype.com/artifact/top.kagg886.wvbridge/jsbridge) |

`wvbridge:jsbridge` extends `wvbridge:core` with a small helper layer for JavaScript
evaluation. It runs JavaScript through the controller's `JavaScriptBridge` and normalizes the
platform-specific return value into a common Kotlin `Value` model.

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

## Quick Start

Use `evaluateScriptValue` on the controller's JavaScript bridge:

```kotlin
import top.kagg886.wvbridge.js.Value
import top.kagg886.wvbridge.js.evaluateScriptValue

val result = webViewController.bridge.evaluateScriptValue(
    """
    return {
        title: document.title,
        href: location.href,
    }
    """.trimIndent()
)

when (result) {
    is Value.Serializable -> println(result.value)
    is Value.ScriptObject -> println("${result.type}: ${result.value}")
    is Value.Error -> println(result.stacktrace)
    Value.Null -> println("null")
    Value.Undefined -> println("undefined")
}
```

`evaluateScriptValue` treats the script as a JavaScript function body. Use `return` when you want
to produce a value.

## Result Values

| Value type           | Meaning                                                                  |
|----------------------|--------------------------------------------------------------------------|
| `Value.Serializable` | A JavaScript value safely serialized with `JSON.stringify`.              |
| `Value.ScriptObject` | A value that cannot be represented faithfully as JSON.                   |
| `Value.Error`        | JavaScript evaluation threw an exception.                                |
| `Value.Null`         | JavaScript returned `null`.                                              |
| `Value.Undefined`    | JavaScript returned `undefined`, or the backend returned no result text. |

## Features

Legend: ✅ Supported; 🚧 Under construction; 🧪 Untested.

| Feature                          | Android | iOS | Windows JVM | Linux JVM | macOS JVM |
|----------------------------------|---------|-----|-------------|-----------|-----------|
| Typed JavaScript code evaluation | ✅       | 🧪  | ✅           | 🧪        | 🧪        |
| `postMessage` / listener API     | 🚧      | 🚧  | 🚧          | 🚧        | 🚧        |
