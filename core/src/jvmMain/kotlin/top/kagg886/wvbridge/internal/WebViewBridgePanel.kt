package top.kagg886.wvbridge.internal

import java.awt.Canvas
import java.awt.Graphics
import java.awt.event.*
import java.io.File
import java.nio.file.Files
import java.util.function.Consumer
import javax.swing.SwingUtilities

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/2/11 09:44
 * ================================================
 */
internal class WebViewBridgePanel(private val initialize: WebViewBridgePanel.() -> Unit) : Canvas(), AutoCloseable {
    private var handle = 0L

    init {
        if (jvmTarget == JvmTarget.LINUX) { //linux should stop gtk thread when closing application
            isFocusable = true
            Runtime.getRuntime().addShutdownHook(Thread {
                if (handle != 0L) {
                    close0(handle)
                }
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

    override fun paint(g: Graphics?) {
        super.paint(g)
        if (g == null) return
        g.color = java.awt.Color.GRAY
        g.drawRect(0, 0, width - 1, height - 1)
    }

    private val urlChangeListener = mutableSetOf<Consumer<String>>()
    private val pageLoadingStartListener = mutableSetOf<Consumer<String>>()
    private val pageLoadingProgressListener = mutableSetOf<Consumer<Float>>()
    private val pageLoadingEndListener = mutableSetOf<Consumer<Boolean>>()
    private val canGoBackChangeListener = mutableSetOf<Consumer<Boolean>>()
    private val canGoForwardChangeListener = mutableSetOf<Consumer<Boolean>>()

    fun addPageLoadingStartListener(handle: Consumer<String>) = check(pageLoadingStartListener.add(handle)) {
        "Page loading start listener: [$handle] already added"
    }

    fun removePageLoadingStartListener(handle: Consumer<String>) = check(pageLoadingStartListener.remove(handle)) {
        "Page loading start listener: [$handle] not yet exists"
    }

    fun addPageLoadingProgressListener(handle: Consumer<Float>) = check(pageLoadingProgressListener.add(handle)) {
        "Page loading progress listener: [$handle] already added"
    }

    fun removePageLoadingProgressListener(handle: Consumer<Float>) = check(pageLoadingProgressListener.remove(handle)) {
        "Page loading progress listener: [$handle] not yet exists"
    }

    fun addPageLoadingEndListener(handle: Consumer<Boolean>) = check(pageLoadingEndListener.add(handle)) {
        "Page loading end listener: [$handle] already added"
    }

    fun removePageLoadingEndListener(handle: Consumer<Boolean>) = check(pageLoadingEndListener.remove(handle)) {
        "Page loading end listener: [$handle] not yet exists"
    }

    fun addURLChangeListener(handle: Consumer<String>) = check(urlChangeListener.add(handle)) {
        "URL change listener: [$handle] already added"
    }

    fun removeURLChangeListener(handle: Consumer<String>) = check(urlChangeListener.remove(handle)) {
        "URL change listener: [$handle] not yet exists"
    }

    fun addCanGoBackChangeListener(handle: Consumer<Boolean>) = check(canGoBackChangeListener.add(handle)) {
        "canGoBack change listener: [$handle] already added"
    }

    fun removeCanGoBackChangeListener(handle: Consumer<Boolean>) = check(canGoBackChangeListener.remove(handle)) {
        "canGoBack change listener: [$handle] not yet exists"
    }

    fun addCanGoForwardChangeListener(handle: Consumer<Boolean>) = check(canGoForwardChangeListener.add(handle)) {
        "canGoForward change listener: [$handle] already added"
    }

    fun removeCanGoForwardChangeListener(handle: Consumer<Boolean>) = check(canGoForwardChangeListener.remove(handle)) {
        "canGoForward change listener: [$handle] not yet exists"
    }

    fun addProgressListener(consumer: Consumer<Float>) = addPageLoadingProgressListener(consumer)
    fun removeProgressListener(consumer: Consumer<Float>) = removePageLoadingProgressListener(consumer)

    override fun removeNotify() {
        super.removeNotify()
        close()
    }

    override fun addNotify() {
        super.addNotify()

        SwingUtilities.invokeLater {
            handle = initAndAttach()
            initialize()
            setPageLoadingStartListener(handle) { url ->
                pageLoadingStartListener.forEach {
                    it.accept(url)
                }
            }
            setPageLoadingProgressListener(handle) { progress ->
                pageLoadingProgressListener.forEach {
                    it.accept(progress)
                }
            }
            setPageLoadingEndListener(handle) { success ->
                pageLoadingEndListener.forEach {
                    it.accept(success)
                }
            }
            setURLChangeListener(handle) { url ->
                if (urlChangeListener.isEmpty()) return@setURLChangeListener
                urlChangeListener.forEach {
                    it.accept(url)
                }
            }
            setCanGoBackChangeListener(handle) { canGoBack ->
                canGoBackChangeListener.forEach {
                    it.accept(canGoBack)
                }
            }
            setCanGoForwardChangeListener(handle) { canGoForward ->
                canGoForwardChangeListener.forEach {
                    it.accept(canGoForward)
                }
            }
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

    fun loadUrl(url: String) = loadUrl(handle, url)
    fun refresh() = refresh(handle)
    fun goBack() = goBack(handle)
    fun goForward() = goForward(handle)

    override fun close() = close0(handle).apply {
        handle = 0
    }

    // --------------init and close--------------
    private external fun initAndAttach(): Long
    private external fun setPageLoadingStartListener(webview: Long, consumer: Consumer<String>)
    private external fun setPageLoadingProgressListener(webview: Long, consumer: Consumer<Float>)
    private external fun setPageLoadingEndListener(webview: Long, consumer: Consumer<Boolean>)
    private external fun setURLChangeListener(webview: Long, handler: Consumer<String>)
    private external fun setCanGoBackChangeListener(webview: Long, handler: Consumer<Boolean>)
    private external fun setCanGoForwardChangeListener(webview: Long, handler: Consumer<Boolean>)
    private external fun update(webview: Long, w: Int, h: Int, x: Int, y: Int)
    private external fun requestFocus0(webview: Long)
    private external fun close0(webview: Long)

    // ------------navigate function------------
    private external fun loadUrl(webview: Long, url: String)
    private external fun refresh(webview: Long)
    private external fun goBack(webview: Long): Boolean
    private external fun goForward(webview: Long): Boolean


    @Suppress("UnsafeDynamicallyLoadedCode")
    companion object {
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
