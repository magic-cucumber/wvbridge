package top.kagg886.wvbridge.internal

import java.awt.Canvas
import java.awt.Graphics
import java.awt.event.*
import java.io.File
import java.nio.file.Files
import java.util.function.Consumer
import javax.swing.SwingUtilities
import javax.swing.Timer

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

    private val progressListener = mutableSetOf<Consumer<Float>>()

    fun addProgressListener(consumer: Consumer<Float>): Unit {
        progressListener.add(consumer)
    }

    fun removeProgressListener(consumer: Consumer<Float>) {
        progressListener.remove(consumer)
    }

    private val urlChangeListener = mutableSetOf<Consumer<String>>()

    fun addURLChangeListener(handle: Consumer<String>) = check(urlChangeListener.add(handle)) {
        "URL change listener: [$handle] already added"
    }

    fun removeURLChangeListener(handle: Consumer<String>) = check(urlChangeListener.remove(handle)) {
        "URL change listener: [$handle] not yet exists"
    }

    override fun removeNotify() {
        super.removeNotify()
        close()
    }

    override fun addNotify() {
        super.addNotify()

        SwingUtilities.invokeLater {
            handle = initAndAttach()
            initialize()
            setProgressListener(handle) { progress ->
                progressListener.forEach {
                    it.accept(progress)
                }
            }
            setURLChangeListener(handle) { url ->
                if (urlChangeListener.isEmpty()) return@setURLChangeListener
                urlChangeListener.forEach {
                    it.accept(url)
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
    fun canGoBack(): Boolean = canGoBack(handle)
    fun canGoForward(): Boolean = canGoForward(handle)

    override fun close() = close0(handle).apply {
        handle = 0
    }

    // --------------init and close--------------
    private external fun initAndAttach(): Long
    private external fun setProgressListener(webview: Long, consumer: Consumer<Float>)
    private external fun setURLChangeListener(webview: Long, handler: Consumer<String>)
    private external fun update(webview: Long, w: Int, h: Int, x: Int, y: Int)
    private external fun requestFocus0(webview: Long)
    private external fun close0(webview: Long)

    // ------------navigate function------------
    private external fun loadUrl(webview: Long, url: String)
    private external fun refresh(webview: Long)
    private external fun goBack(webview: Long): Boolean
    private external fun goForward(webview: Long): Boolean
    private external fun canGoBack(webview: Long): Boolean
    private external fun canGoForward(webview: Long): Boolean


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
