@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.awt.*
import javax.swing.*

private class BrowserPane(
    private val title: String,
    initializeUrl: String,
    webViewInitiallyActive: Boolean = true,
) : JPanel(GridBagLayout()) {
    private val navButtonSize = Dimension(44, 28)

    private val progressBar = JProgressBar(0, 100).apply {
        value = 0
        isStringPainted = false
        isVisible = false
        preferredSize = Dimension(600, 6)
    }

    private val urlField = JTextField(initializeUrl).apply {
        preferredSize = Dimension(600, 30)
    }
    private val backButton = createNavButton("←")
    private val forwardButton = createNavButton("→")
    private val refreshButton = createNavButton("⟳")
    private val stopButton = createNavButton("⏹")

    private val webView = WebViewBridgePanel {
        loadUrl(if (urlField.text.isNullOrBlank()) initializeUrl else urlField.text)
    }

    private var isWebViewPresent = webViewInitiallyActive
    private var canGoBack = false
    private var canGoForward = false
    private var isLoading = false

    private fun debug(event: String, value: Any?) {
        println("[$title][$event] $value")
    }

    private fun createNavButton(text: String): JButton {
        return JButton(text).apply { preferredSize = navButtonSize }
    }

    init {
        border = BorderFactory.createTitledBorder(title)

        webView.addPageLoadingStartListener { debug("pageLoadingStart", it) }
        webView.addPageLoadingProgressListener { debug("pageLoadingProgress", it) }
        webView.addPageLoadingEndListener { a, b -> debug("pageLoadingEnd", "success: $a, $b") }
        webView.addURLChangeListener { debug("urlChange", it) }
        webView.addCanGoBackChangeListener { debug("canGoBackChange", it) }
        webView.addCanGoForwardChangeListener { debug("canGoForwardChange", it) }

        webView.addPageLoadingStartListener {
            SwingUtilities.invokeLater {
                isLoading = true
                progressBar.value = 0
                progressBar.isVisible = true
                updateNavButtons()
            }
        }
        webView.addPageLoadingProgressListener { progress ->
            SwingUtilities.invokeLater {
                val p = progress.coerceIn(0f, 1f)
                progressBar.value = (p * 100).toInt().coerceIn(0, 100)
                progressBar.isVisible = p > 0f && p < 1f
            }
        }
        webView.addPageLoadingEndListener { a, _ ->
            SwingUtilities.invokeLater {
                isLoading = false
                progressBar.value = if (a) 100 else progressBar.value
                progressBar.isVisible = false
                updateNavButtons()
            }
        }
        webView.addURLChangeListener {
            SwingUtilities.invokeLater {
                urlField.text = it
                updateNavButtons()
            }
        }
        webView.addCanGoBackChangeListener {
            canGoBack = it
            SwingUtilities.invokeLater(::updateNavButtons)
        }
        webView.addCanGoForwardChangeListener {
            canGoForward = it
            SwingUtilities.invokeLater(::updateNavButtons)
        }

        urlField.addActionListener {
            progressBar.value = 0
            progressBar.isVisible = false
            webView.loadUrl(urlField.text)
            updateNavButtons()
        }
        backButton.addActionListener {
            runWebViewAction("Navigation failed") {
                webView.goBack()
            }
        }
        forwardButton.addActionListener {
            runWebViewAction("Navigation failed") {
                webView.goForward()
            }
        }
        refreshButton.addActionListener {
            runWebViewAction("Refresh failed") { webView.refresh() }
        }
        stopButton.addActionListener {
            runWebViewAction("Stop failed") { webView.stop() }
        }

        val navPanel = JPanel(BorderLayout(8, 0)).apply {
            add(
                JPanel(FlowLayout(FlowLayout.LEFT, 4, 0)).apply {
                    add(backButton)
                    add(forwardButton)
                    add(refreshButton)
                    add(stopButton)
                },
                BorderLayout.WEST
            )
            add(urlField, BorderLayout.CENTER)
        }
        updateNavButtons()

        add(
            progressBar,
            GridBagConstraints().apply {
                gridx = 0
                gridy = 0
                weightx = 1.0
                fill = GridBagConstraints.HORIZONTAL
                insets = Insets(10, 10, 2, 10)
            }
        )
        add(
            navPanel,
            GridBagConstraints().apply {
                gridx = 0
                gridy = 1
                weightx = 1.0
                fill = GridBagConstraints.HORIZONTAL
                insets = Insets(2, 10, 10, 10)
            }
        )
        if (isWebViewPresent) {
            addWebView()
        }
    }

    fun toggleWebView(): Boolean {
        if (isWebViewPresent) {
            remove(webView)
        } else {
            addWebView()
        }
        isWebViewPresent = !isWebViewPresent
        revalidate()
        repaint()
        return isWebViewPresent
    }

    private fun addWebView() {
        add(
            webView,
            GridBagConstraints().apply {
                gridx = 0
                gridy = 2
                weightx = 1.0
                weighty = 1.0
                fill = GridBagConstraints.BOTH
                insets = Insets(0, 10, 10, 10)
            }
        )
    }

    private fun updateNavButtons() {
        backButton.isEnabled = canGoBack
        forwardButton.isEnabled = canGoForward
        stopButton.isEnabled = isLoading
    }

    private fun runWebViewAction(fallbackError: String, action: () -> Any?) {
        val result = runCatching(action)
        result.exceptionOrNull()?.let {
            JOptionPane.showMessageDialog(this, it.cause?.message ?: it.message ?: fallbackError)
            updateNavButtons()
            return
        }
        updateNavButtons()
    }
}

fun main() = SwingUtilities.invokeLater {
    val frame = JFrame("WebView Bridge Panel Demo").apply {
        setSize(1200, 600)
        defaultCloseOperation = JFrame.EXIT_ON_CLOSE
        layout = GridLayout(1, 2, 0, 0)
    }

    val initializeUrl = "https://www.baidu.com"
    val leftPane = BrowserPane("左侧 WebView", initializeUrl)
    val rightPane = BrowserPane("右侧 WebView", initializeUrl, webViewInitiallyActive = false)

    frame.add(leftPane)
    frame.add(rightPane)

    val menuBar = JMenuBar()
    val menu = JMenu("操作")
    val toggleLeftItem = JMenuItem("删除左侧 WebView")
    val toggleRightItem = JMenuItem("显示右侧 WebView")

    toggleLeftItem.addActionListener {
        val isPresent = leftPane.toggleWebView()
        toggleLeftItem.text = if (isPresent) "删除左侧 WebView" else "显示左侧 WebView"
        frame.revalidate()
    }

    toggleRightItem.addActionListener {
        val isPresent = rightPane.toggleWebView()
        toggleRightItem.text = if (isPresent) "删除右侧 WebView" else "显示右侧 WebView"
        frame.revalidate()
    }

    menu.add(toggleLeftItem)
    menu.add(toggleRightItem)
    menuBar.add(menu)
    frame.jMenuBar = menuBar

    frame.isVisible = true
}
