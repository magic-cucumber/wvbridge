@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.awt.*
import java.awt.event.ComponentAdapter
import java.awt.event.ComponentEvent
import javax.swing.*

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/2/11 09:59
 * ================================================
 */

private class BrowserPane(
    private val title: String,
    initializeUrl: String,
) : JPanel(GridBagLayout()) {
    private val progressBar = JProgressBar(0, 100).apply {
        value = 0
        isStringPainted = false
        isVisible = false
        preferredSize = Dimension(600, 6)
    }

    private val urlField = JTextField(initializeUrl).apply {
        preferredSize = Dimension(600, 30)
    }
    private val backButton = JButton("←")
    private val forwardButton = JButton("→")
    private val refreshButton = JButton("⟳")

    private val webView = WebViewBridgePanel {
        loadUrl(if (urlField.text.isNullOrBlank()) initializeUrl else urlField.text)
    }

    private var isWebViewPresent = true

    init {
        border = BorderFactory.createTitledBorder(title)

        webView.addProgressListener(::println)
        webView.addURLChangeListener {
            println("[$title] $it")
        }
        webView.addProgressListener { progress ->
            SwingUtilities.invokeLater {
                val p = progress.coerceIn(0f, 1f)
                progressBar.value = (p * 100).toInt().coerceIn(0, 100)
                progressBar.isVisible = p > 0f && p < 1f
            }
        }
        webView.addURLChangeListener {
            SwingUtilities.invokeLater {
                urlField.text = it
                updateNavButtons()
            }
        }

        urlField.addActionListener {
            progressBar.value = 0
            progressBar.isVisible = false
            webView.loadUrl(urlField.text)
            updateNavButtons()
        }
        backButton.addActionListener {
            runNavigation {
                webView.goBack()
            }
        }
        forwardButton.addActionListener {
            runNavigation {
                webView.goForward()
            }
        }
        refreshButton.addActionListener {
            runRefresh()
        }

        val navPanel = JPanel(BorderLayout(8, 0)).apply {
            add(
                JPanel(FlowLayout(FlowLayout.LEFT, 4, 0)).apply {
                    add(backButton)
                    add(forwardButton)
                    add(refreshButton)
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
        addWebView()
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
        backButton.isEnabled = runCatching { webView.canGoBack() }.getOrDefault(false)
        forwardButton.isEnabled = runCatching { webView.canGoForward() }.getOrDefault(false)
    }

    private fun runNavigation(action: () -> Boolean) {
        val result = runCatching { action() }
        result.exceptionOrNull()?.let {
            JOptionPane.showMessageDialog(this, it.cause?.message ?: it.message ?: "Navigation failed")
            updateNavButtons()
            return
        }
        updateNavButtons()
    }

    private fun runRefresh() {
        val result = runCatching { webView.refresh() }
        result.exceptionOrNull()?.let {
            JOptionPane.showMessageDialog(this, it.cause?.message ?: it.message ?: "Refresh failed")
            updateNavButtons()
            return
        }
        updateNavButtons()
    }
}

fun main() = SwingUtilities.invokeLater {
    val frame = JFrame("WebView Bridge Panel Demo")
    frame.setSize(1200, 600)
    frame.defaultCloseOperation = JFrame.EXIT_ON_CLOSE
    frame.layout = GridLayout(1, 2, 0, 0)

    val initializeUrl = "https://www.baidu.com"
    val leftPane = BrowserPane("左侧 WebView", initializeUrl)
    val rightPane = BrowserPane("右侧 WebView", initializeUrl)

    frame.add(leftPane)
    frame.add(rightPane)

    val menuBar = JMenuBar()
    val menu = JMenu("操作")
    val toggleLeftItem = JMenuItem("删除左侧 WebView")
    val toggleRightItem = JMenuItem("删除右侧 WebView")

    toggleLeftItem.addActionListener {
        val isPresent = leftPane.toggleWebView()
        toggleLeftItem.text = if (isPresent) "删除左侧 WebView" else "显示左侧 WebView"
        frame.revalidate()
        frame.repaint()
    }

    toggleRightItem.addActionListener {
        val isPresent = rightPane.toggleWebView()
        toggleRightItem.text = if (isPresent) "删除右侧 WebView" else "显示右侧 WebView"
        frame.revalidate()
        frame.repaint()
    }

    menu.add(toggleLeftItem)
    menu.add(toggleRightItem)
    menuBar.add(menu)
    frame.jMenuBar = menuBar

    frame.addComponentListener(object : ComponentAdapter() {
        override fun componentResized(e: ComponentEvent?) {
            frame.revalidate()
        }
    })

    frame.isVisible = true
}
