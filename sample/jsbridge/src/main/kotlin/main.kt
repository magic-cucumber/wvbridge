@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.awt.BorderLayout
import java.awt.Dimension
import java.awt.event.WindowAdapter
import java.awt.event.WindowEvent
import javax.swing.*

private const val INITIAL_URL = "about:blank"

/**
 * A single-WebView demo panel that exercises the high-level JavaScript bridge API.
 *
 * The panel intentionally keeps one browser instance and one document-start hook handle so the
 * lifetime rules are visible: register once, unregister by handle, and close the native browser
 * before discarding the Swing component.
 */
private class JavaScriptBridgeDemoPane : JPanel(BorderLayout(8, 8)) {
    private val urlField = JTextField(INITIAL_URL)
    private val logArea = JTextArea().apply {
        isEditable = false
        rows = 8
        lineWrap = true
        wrapStyleWord = true
    }

    private var webView: WebViewBridgePanel? = null
    private var documentStartHookId: Long? = null

    val addHookItem = JMenuItem("添加 hook").apply {
        addActionListener { addHook() }
    }
    val removeHookItem = JMenuItem("移除 hook").apply {
        addActionListener { removeHook() }
    }
    val refreshItem = JMenuItem("刷新").apply {
        addActionListener { runWebViewAction("Refresh failed") { refresh() } }
    }
    val runScriptItem = JMenuItem("运行任意 script").apply {
        addActionListener { runScriptFromDialog() }
    }
    val toggleInstanceItem = JMenuItem("销毁浏览器实例").apply {
        addActionListener { toggleBrowserInstance() }
    }

    init {
        border = BorderFactory.createEmptyBorder(8, 8, 8, 8)

        val loadButton = JButton("加载").apply {
            preferredSize = Dimension(72, 28)
            addActionListener {
                runWebViewAction("Load URL failed") {
                    loadUrl(urlField.text.ifBlank { INITIAL_URL })
                }
            }
        }

        add(
            JPanel(BorderLayout(6, 0)).apply {
                add(urlField, BorderLayout.CENTER)
                add(loadButton, BorderLayout.EAST)
            },
            BorderLayout.NORTH
        )
        add(JScrollPane(logArea), BorderLayout.SOUTH)

        createBrowserInstance()
        updateMenuState()
    }

    /**
     * Creates the localized menu that drives each bridge operation.
     */
    fun createMenuBar(): JMenuBar =
        JMenuBar().apply {
            add(
                JMenu("JS Bridge").apply {
                    add(addHookItem)
                    add(removeHookItem)
                    addSeparator()
                    add(refreshItem)
                    add(runScriptItem)
                    addSeparator()
                    add(toggleInstanceItem)
                }
            )
        }

    /**
     * Releases bridge resources before the window is disposed.
     */
    fun dispose() {
        removeHook()
        webView?.close()
        webView = null
    }

    /**
     * Instantiates the native browser component and attaches it to the panel.
     */
    private fun createBrowserInstance() {
        if (webView != null) return

        val panel = WebViewBridgePanel {
            loadUrl(urlField.text.ifBlank { INITIAL_URL })
        }
        installListeners(panel)
        webView = panel
        add(panel, BorderLayout.CENTER)
        log("browser instance created")
        revalidate()
        repaint()
        updateMenuState()
    }

    /**
     * Destroys the current browser instance after unregistering any active document-start hook.
     */
    private fun destroyBrowserInstance() {
        val panel = webView ?: return
        removeHook()
        remove(panel)
        panel.close()
        webView = null
        log("browser instance destroyed")
        revalidate()
        repaint()
        updateMenuState()
    }

    private fun toggleBrowserInstance() {
        if (webView == null) {
            createBrowserInstance()
        } else {
            destroyBrowserInstance()
        }
    }

    /**
     * Wires native page events into the sample log area.
     */
    private fun installListeners(panel: WebViewBridgePanel) {
        panel.addPageLoadingStartListener {
            SwingUtilities.invokeLater {
                urlField.text = it
                log("page start: $it")
            }
        }
        panel.addPageLoadingEndListener { success, reason ->
            SwingUtilities.invokeLater {
                log("page end: success=$success, reason=${reason ?: ""}")
            }
        }
        panel.addURLChangeListener {
            SwingUtilities.invokeLater {
                urlField.text = it
                log("url changed: $it")
            }
        }
    }

    /**
     * Registers JavaScript that runs at document start and then waits for DOMContentLoaded before
     * touching document.body. The returned hook id is the cancellation handle for this desktop API.
     */
    private fun addHook() {
        val panel = webView ?: return showMessage("请先实例化浏览器实例")
        if (documentStartHookId != null) {
            showMessage("hook 已存在，请先移除")
            return
        }

        runWebViewAction("Add hook failed") {
            documentStartHookId = panel.registerDocumentStartHook(documentReadyHookScript())
            log("document-start hook registered: $documentStartHookId")
            updateMenuState()
        }
    }

    /**
     * Unregisters the active document-start hook by its handle.
     */
    private fun removeHook() {
        val panel = webView
        val hookId = documentStartHookId ?: return
        documentStartHookId = null

        if (panel != null) {
            runCatching { panel.unregisterDocumentStartHook(hookId) }
                .onFailure { log("remove hook failed: ${it.message ?: it::class.java.name}") }
        }
        log("document-start hook removed: $hookId")
        updateMenuState()
    }

    /**
     * Refreshes the current page through the browser API.
     */
    private fun refresh() {
        webView?.refresh() ?: showMessage("请先实例化浏览器实例")
    }

    /**
     * Navigates the browser to the requested URL.
     */
    private fun loadUrl(url: String) {
        webView?.loadUrl(url) ?: showMessage("请先实例化浏览器实例")
    }

    /**
     * Evaluates arbitrary JavaScript in the current page and displays the serialized result.
     */
    private fun runScriptFromDialog() {
        val panel = webView ?: return showMessage("请先实例化浏览器实例")
        val script = JOptionPane.showInputDialog(
            this,
            "输入要执行的 JavaScript",
            "document.title",
        ) ?: return

        runWebViewAction("Evaluate script failed") {
            val result = panel.evaluateScript(script)
            log("script result: ${result ?: "null"}")
            showMessage(result ?: "null")
        }
    }

    /**
     * Keeps UI error handling consistent for native browser calls.
     */
    private fun runWebViewAction(fallbackError: String, action: () -> Unit) {
        runCatching(action)
            .onFailure {
                val message = it.cause?.message ?: it.message ?: fallbackError
                log(message)
                showMessage(message)
            }
    }

    /**
     * Enables only the menu operations that make sense for the current browser and hook state.
     */
    private fun updateMenuState() {
        val hasWebView = webView != null
        addHookItem.isEnabled = hasWebView && documentStartHookId == null
        removeHookItem.isEnabled = hasWebView && documentStartHookId != null
        refreshItem.isEnabled = hasWebView
        runScriptItem.isEnabled = hasWebView
        toggleInstanceItem.text = if (hasWebView) "销毁浏览器实例" else "实例化浏览器实例"
    }

    private fun log(message: String) {
        logArea.append(message)
        logArea.append("\n")
        logArea.caretPosition = logArea.document.length
    }

    private fun showMessage(message: String) {
        JOptionPane.showMessageDialog(this, message)
    }

    /**
     * Produces a document-start script that installs a DOMContentLoaded listener. This demonstrates
     * how to register early while still waiting until document.body is safe to mutate.
     */
    private fun documentReadyHookScript(): String =
        """
        (function() {
          document.addEventListener('DOMContentLoaded', function() {
            var node = document.createElement('div');
            node.id = 'wvbridge-document-start-hook-demo';
            node.textContent = 'Injected by wvbridge document-start hook at ' + new Date().toISOString();
            node.style.cssText = [
              'position:fixed',
              'top:0',
              'left:0',
              'right:0',
              'z-index:2147483647',
              'padding:10px 14px',
              'background:#1f6feb',
              'color:white',
              'font:14px/1.4 system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif'
            ].join(';');
            if (document.body) {
              document.body.prepend(node);
              document.body.style.paddingTop = '48px';
            } else if (document.documentElement) {
              document.documentElement.appendChild(node);
            }
          }, { once: true });
        })();
        """.trimIndent()
}

fun main() = SwingUtilities.invokeLater {
    val pane = JavaScriptBridgeDemoPane()
    val frame = JFrame("wvbridge JavaScript Bridge Demo").apply {
        defaultCloseOperation = JFrame.DISPOSE_ON_CLOSE
        setSize(1100, 720)
        layout = BorderLayout()
        jMenuBar = pane.createMenuBar()
        add(pane, BorderLayout.CENTER)
        addWindowListener(object : WindowAdapter() {
            override fun windowClosed(e: WindowEvent) {
                pane.dispose()
            }
        })
    }

    frame.isVisible = true
}
