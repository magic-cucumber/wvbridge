# Native entry points and callbacks.
-keep class top.kagg886.wvbridge.internal.WebViewBridgePanel { *; }
-keep class top.kagg886.wvbridge.internal.listener.NativeBridge { *; }
-keep class top.kagg886.wvbridge.bridge.WebMessageConsumer { *; }

# Cookie constructor/fields and platform settings accessed through JNI.
-keep class top.kagg886.wvbridge.internal.cookie.StructedCookie { *; }
-keep class top.kagg886.wvbridge.config.internal.** { *; }
