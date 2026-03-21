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

    private val webView = WebViewBridgePanel {
        loadUrl(initializeUrl)
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
            }
        }

        urlField.addActionListener {
            progressBar.value = 0
            progressBar.isVisible = false
            webView.loadUrl(urlField.text)
        }

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
            urlField,
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
}

fun main() = SwingUtilities.invokeLater {
    val frame = JFrame("WebView Bridge Panel Demo")
    frame.setSize(1200, 600)
    frame.defaultCloseOperation = JFrame.EXIT_ON_CLOSE
    frame.layout = GridBagLayout()

    val initializeUrl = "https://www.baidu.com"
    val leftPane = BrowserPane("左侧 WebView", initializeUrl)
    val rightPane = BrowserPane("右侧 WebView", initializeUrl)

    frame.add(
        leftPane,
        GridBagConstraints().apply {
            gridx = 0
            gridy = 0
            weightx = 0.5
            weighty = 1.0
            fill = GridBagConstraints.BOTH
            insets = Insets(10, 10, 10, 5)
        }
    )
    frame.add(
        rightPane,
        GridBagConstraints().apply {
            gridx = 1
            gridy = 0
            weightx = 0.5
            weighty = 1.0
            fill = GridBagConstraints.BOTH
            insets = Insets(10, 5, 10, 10)
        }
    )

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
