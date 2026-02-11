@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

import top.kagg886.wvbridge.NavigationHandler
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


fun main() = SwingUtilities.invokeLater {
    val frame = JFrame("WebView Bridge Panel Demo")
    frame.setSize(800, 600)
    frame.defaultCloseOperation = JFrame.EXIT_ON_CLOSE
    frame.layout = GridBagLayout()

    val initializeUrl = "https://www.baidu.com"

    val webView = WebViewBridgePanel {
        loadUrl(initializeUrl)
    }

    webView.addProgressListener(::println)
    webView.addNavigationHandler {
        println(it)
        NavigationHandler.NavigationResult.ALLOWED
    }

    // --- 新增：进度条（位于输入框上方） ---
    val progressBar = JProgressBar(0, 100).apply {
        value = 0
        isStringPainted = false
        isVisible = false
        preferredSize = Dimension(600, 6)
    }

    webView.addProgressListener { progress ->
        SwingUtilities.invokeLater {
            val p = progress.coerceIn(0f, 1f)
            progressBar.value = (p * 100).toInt().coerceIn(0, 100)
            progressBar.isVisible = p > 0f && p < 1f
        }
    }

    // --- 新增：地址栏输入框 ---
    val urlField = JTextField(initializeUrl).apply {
        preferredSize = Dimension(600, 30)
    }
    webView.addNavigationHandler {
        urlField.text = it
        NavigationHandler.NavigationResult.ALLOWED
    }

    // 监听回车键
    urlField.addActionListener {
        // 0.0 时隐藏，等待 WebView 报告真实进度后再显示
        progressBar.value = 0
        progressBar.isVisible = false

        val url = urlField.text
        webView.loadUrl(url)
    }

    // --- 布局配置 ---
    val gbc = GridBagConstraints().apply {
        fill = GridBagConstraints.HORIZONTAL
        insets = Insets(10, 10, 10, 10) // 设置边距
    }

    // 添加进度条 (第 0 行)
    gbc.gridy = 0
    gbc.weightx = 1.0
    gbc.weighty = 0.0
    gbc.fill = GridBagConstraints.HORIZONTAL
    gbc.insets = Insets(10, 10, 2, 10)
    frame.add(progressBar, gbc)

    // 添加输入框 (第 1 行)
    gbc.gridy = 1
    gbc.fill = GridBagConstraints.HORIZONTAL
    gbc.insets = Insets(2, 10, 10, 10)
    frame.add(urlField, gbc)

    // 添加 WebView (第 2 行)
    gbc.gridy = 2
    gbc.weighty = 1.0 // 占据剩余垂直空间
    gbc.fill = GridBagConstraints.BOTH
    gbc.insets = Insets(0, 10, 10, 10)
    frame.add(webView, gbc)

    // --- 菜单逻辑 (保持不变) ---
    val menuBar = JMenuBar()
    val menu = JMenu("操作")
    val toggleItem = JMenuItem("删除 WebView")
    var isPresent = true

    toggleItem.addActionListener {
        if (isPresent) {
            frame.remove(webView)
            toggleItem.text = "显示 WebView"
        } else {
            gbc.gridy = 2 // 确保重新添加时位置正确
            gbc.weightx = 1.0
            gbc.weighty = 1.0
            gbc.fill = GridBagConstraints.BOTH
            gbc.insets = Insets(0, 10, 10, 10)
            frame.add(webView, gbc)
            toggleItem.text = "删除 WebView"
        }
        isPresent = !isPresent
        frame.revalidate()
        frame.repaint()
    }

    menu.add(toggleItem)
    menuBar.add(menu)
    frame.jMenuBar = menuBar

    // 窗口缩放监听 (可选：如果你希望 WebView 比例固定)
    frame.addComponentListener(object : ComponentAdapter() {
        override fun componentResized(e: ComponentEvent?) {
            // GridBagLayout 会自动处理基础缩放，这里可以根据需要调整
            frame.revalidate()
        }
    })

    frame.isVisible = true
}
