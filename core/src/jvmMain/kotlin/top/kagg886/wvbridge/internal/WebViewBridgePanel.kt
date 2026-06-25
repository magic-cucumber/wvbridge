package top.kagg886.wvbridge.internal

import java.awt.Canvas
import java.awt.Color
import java.awt.Graphics
import java.awt.Point
import java.awt.event.*
import java.io.File
import java.nio.file.Files
import java.util.concurrent.CopyOnWriteArraySet
import java.util.concurrent.locks.ReentrantLock
import java.util.function.BiConsumer
import java.util.function.Consumer
import javax.swing.SwingUtilities
import kotlin.concurrent.withLock
import top.kagg886.wvbridge.internal.listener.NativeBridge

/**
 * WebView 页面加载生命周期说明：
 * - `onPageLoadingStart(url)`
 * - `onPageLoadingProgress(progress)`
 * - `onPageLoadingEnd(success)`
 *
 * ```text
 * [一次页面加载周期]
 *
 * (可能先到达) onPageLoadingProgress(0.1)
 *              |
 *              v
 *      onPageLoadingStart(url)
 *              |
 *              v
 * onPageLoadingProgress(p1) -> onPageLoadingProgress(p2) -> ... -> onPageLoadingProgress(1.0)
 *              |
 *              v
 *      onPageLoadingEnd(true/false)
 *
 * 结论：
 * - `onPageLoadingProgress` 可能先于 `onPageLoadingStart`。
 * - 一次用户操作（如点击）可能触发多个“start -> progress* -> end”完整周期（跳转 + 重定向）。
 */
internal class WebViewBridgePanel(private val initialize: WebViewBridgePanel.() -> Unit) : Canvas(), AutoCloseable {
    @Volatile
    internal var handle = 0L
        private set

    init {
        if (jvmTarget == JvmTarget.LINUX) { //linux should stop gtk thread when closing application
            isFocusable = true
            Runtime.getRuntime().addShutdownHook(Thread {
                close()
            })
            addFocusListener(object : FocusAdapter() {
                override fun focusGained(e: FocusEvent) {
                    if (handle != 0L) {
                        requestFocus0(handle)
                    }
                }
            })
            addMouseListener(object : MouseAdapter() {
                override fun mousePressed(e: MouseEvent) {
                    if (handle != 0L) {
                        requestFocus0(handle)
                    }
                }
            })
        }
        addComponentListener(object : ComponentAdapter() {
            override fun componentResized(e: ComponentEvent) {
                if (handle == 0L) return
                update(handle, width, height, locationOnScreen.x, locationOnScreen.y)
            }

            override fun componentMoved(e: ComponentEvent) {
                if (handle == 0L) return
                update(handle, width, height, locationOnScreen.x, locationOnScreen.y)
            }
        })
    }

    public override fun getLocationOnScreen(): Point {
        val window = SwingUtilities.getWindowAncestor(this) ?: return bounds.location
        val insets = window.insets
        val point = SwingUtilities.convertPoint(this, 0, 0, window)
        point.translate(-insets.left, -insets.top)
        return point
    }

    override fun paint(g: Graphics?) {
        super.paint(g)
        if (g == null) return
        g.color = Color.GRAY
        g.drawRect(0, 0, width - 1, height - 1)
    }

    internal val urlChangeListener = CopyOnWriteArraySet<Consumer<String>>()
    internal val pageLoadingStartListener = CopyOnWriteArraySet<Consumer<String>>()
    internal val pageLoadingProgressListener = CopyOnWriteArraySet<Consumer<Float>>()
    internal val pageLoadingEndListener = CopyOnWriteArraySet<BiConsumer<Boolean, String?>>()
    internal val canGoBackChangeListener = CopyOnWriteArraySet<Consumer<Boolean>>()
    internal val canGoForwardChangeListener = CopyOnWriteArraySet<Consumer<Boolean>>()

    internal val closeListener = CopyOnWriteArraySet<Consumer<String?>>()

    public fun addPageLoadingStartListener(handle: Consumer<String>): Unit =
        check(pageLoadingStartListener.add(handle)) {
            "Page loading start listener: [$handle] already added"
        }

    public fun removePageLoadingStartListener(handle: Consumer<String>): Unit =
        check(pageLoadingStartListener.remove(handle)) {
            "Page loading start listener: [$handle] not yet exists"
        }

    public fun addPageLoadingProgressListener(handle: Consumer<Float>): Unit =
        check(pageLoadingProgressListener.add(handle)) {
            "Page loading progress listener: [$handle] already added"
        }

    public fun removePageLoadingProgressListener(handle: Consumer<Float>): Unit =
        check(pageLoadingProgressListener.remove(handle)) {
            "Page loading progress listener: [$handle] not yet exists"
        }

    public fun addPageLoadingEndListener(handle: BiConsumer<Boolean, String?>): Unit =
        check(pageLoadingEndListener.add(handle)) {
            "Page loading end listener: [$handle] already added"
        }

    public fun removePageLoadingEndListener(handle: BiConsumer<Boolean, String?>): Unit =
        check(pageLoadingEndListener.remove(handle)) {
            "Page loading end listener: [$handle] not yet exists"
        }

    public fun addURLChangeListener(handle: Consumer<String>): Unit = check(urlChangeListener.add(handle)) {
        "URL change listener: [$handle] already added"
    }

    public fun removeURLChangeListener(handle: Consumer<String>): Unit = check(urlChangeListener.remove(handle)) {
        "URL change listener: [$handle] not yet exists"
    }

    public fun addCanGoBackChangeListener(handle: Consumer<Boolean>): Unit =
        check(canGoBackChangeListener.add(handle)) {
            "canGoBack change listener: [$handle] already added"
        }

    public fun removeCanGoBackChangeListener(handle: Consumer<Boolean>): Unit =
        check(canGoBackChangeListener.remove(handle)) {
            "canGoBack change listener: [$handle] not yet exists"
        }

    public fun addCanGoForwardChangeListener(handle: Consumer<Boolean>): Unit =
        check(canGoForwardChangeListener.add(handle)) {
            "canGoForward change listener: [$handle] already added"
        }

    public fun removeCanGoForwardChangeListener(handle: Consumer<Boolean>): Unit =
        check(canGoForwardChangeListener.remove(handle)) {
            "canGoForward change listener: [$handle] not yet exists"
        }

    public fun addProgressListener(consumer: Consumer<Float>): Unit = addPageLoadingProgressListener(consumer)
    public fun removeProgressListener(consumer: Consumer<Float>): Unit = removePageLoadingProgressListener(consumer)


    public fun addWebViewCloseListener(handle: Consumer<String?>): Unit =
        check(closeListener.add(handle)) {
            "webview close listener: [$handle] already added"
        }

    public fun removeWebViewCloseListener(handle: Consumer<String?>): Unit =
        check(closeListener.remove(handle)) {
            "webview close listener: [$handle] not yet exists"
        }
    override fun removeNotify() {
        super.removeNotify()
        close()
    }

    override fun addNotify() {
        super.addNotify()

        SwingUtilities.invokeLater {
            handle = initAndAttach()
            NativeBridge.register(this)
            initialize()
            SwingUtilities.invokeLater {
                update(handle, width, height, locationOnScreen.x, locationOnScreen.y)
                if (jvmTarget == JvmTarget.LINUX && isFocusOwner) {
                    requestFocus0(handle)
                }
                revalidate()
                repaint()
            }
        }
    }

    public fun loadUrl(url: String): Unit = loadUrl(handle, url)
    public fun refresh(): Unit = refresh(handle)
    public fun stop(): Unit = stop(handle)
    public fun goBack(): Boolean = goBack(handle)
    public fun goForward(): Boolean = goForward(handle)
    public fun evaluateScript(script: String): String? = evaluateScript(handle, script)
    public fun registerDocumentStartHook(script: String): Long = registerDocumentStartHook(handle, script)
    public fun unregisterDocumentStartHook(hookId: Long): Unit = unregisterDocumentStartHook(handle, hookId)

    private val closeLock = ReentrantLock()
    override fun close(): Unit = close(null)

    internal fun close(cause: String?) = closeLock.withLock {
        val handle = handle
        if (handle == 0L) return@withLock
        NativeBridge.unregister(this)
        this.handle = 0L
        close0(handle)
        closeListener.forEach { it.accept(cause) }
    }

    // --------------init and close--------------
    private external fun initAndAttach(): Long
    private external fun update(webview: Long, w: Int, h: Int, x: Int, y: Int)
    private external fun requestFocus0(webview: Long)
    private external fun close0(webview: Long)

    // ------------navigate function------------
    private external fun loadUrl(webview: Long, url: String)
    private external fun refresh(webview: Long)
    private external fun goBack(webview: Long): Boolean
    private external fun goForward(webview: Long): Boolean
    private external fun stop(webview: Long)
    private external fun evaluateScript(webview: Long, script: String): String?
    private external fun registerDocumentStartHook(webview: Long, script: String): Long
    private external fun unregisterDocumentStartHook(webview: Long, hookId: Long)


    @Suppress("UnsafeDynamicallyLoadedCode")
    internal companion object {
        init {
            val name = when (jvmTarget) {
                JvmTarget.MACOS, JvmTarget.LINUX -> "libwvbridge"
                JvmTarget.WINDOWS -> "wvbridge"
            }
            val ext = when (jvmTarget) {
                JvmTarget.MACOS -> "dylib"
                JvmTarget.LINUX -> "so"
                JvmTarget.WINDOWS -> "dll"
            }

            val tmpFile: File = Files.createTempFile(
                "wvbridge",
                ".$ext"
            ).toFile()

            tmpFile.deleteOnExit()

            // 将资源里的库写到临时文件
            val stream = WebViewBridgePanel::class.java.getResourceAsStream("/$name.$ext")
                ?: error("Library resource /$name.$ext not found")
            stream.use { tmpFile.writeBytes(it.readAllBytes()) }

            System.load(tmpFile.absolutePath)
        }
    }
}
