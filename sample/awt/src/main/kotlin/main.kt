@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

import top.kagg886.wvbridge.cookie.Cookie
import top.kagg886.wvbridge.internal.WebViewBridgePanel
import top.kagg886.wvbridge.util.LoggerReceiver
import java.awt.*
import java.io.File
import java.net.URI
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import javax.swing.*
import top.kagg886.wvbridge.config.internal.NativeLinuxWebViewPlatformSetting
import top.kagg886.wvbridge.config.internal.NativeMacOSWebViewPlatformSetting
import top.kagg886.wvbridge.config.internal.NativeMacOSWebViewWebsiteDataStore
import top.kagg886.wvbridge.config.internal.NativeWindowsWebViewPlatformSetting
import top.kagg886.wvbridge.cookie.path
import top.kagg886.wvbridge.internal.JvmTarget
import top.kagg886.wvbridge.internal.jvmTarget
import top.kagg886.wvbridge.internal.cookie.StructedCookie
import kotlin.io.path.absolutePathString
import javax.swing.table.DefaultTableModel

private class CookieManagerWindow(
    parent: Component,
    private val currentUrl: () -> String,
    private val webView: WebViewBridgePanel,
) : JDialog(
    SwingUtilities.getWindowAncestor(parent),
    "Cookie 管理",
    Dialog.ModalityType.MODELESS,
) {
    private data class CookieField(val key: String, val title: String)

    private val tableModel = object : DefaultTableModel(
        COOKIE_FIELDS.map { it.title }.toTypedArray(),
        0,
    ) {
        override fun isCellEditable(row: Int, column: Int): Boolean = false
    }
    private val table = JTable(tableModel).apply {
        autoCreateRowSorter = true
        setSelectionMode(ListSelectionModel.MULTIPLE_INTERVAL_SELECTION)
    }
    private val urlLabel = JLabel()
    private val statusLabel = JLabel(" ")
    private var cookies: List<Cookie> = emptyList()

    init {
        defaultCloseOperation = DISPOSE_ON_CLOSE
        minimumSize = Dimension(760, 360)
        size = Dimension(960, 480)
        layout = BorderLayout(8, 8)

        val addButton = JButton("增加").apply {
            addActionListener { addCookie() }
        }
        val removeButton = JButton("删除").apply {
            addActionListener { removeSelectedCookies() }
        }
        val clearButton = JButton("清空").apply {
            addActionListener { clearCookies() }
        }
        val reloadButton = JButton("刷新").apply {
            addActionListener { reloadCookies() }
        }

        add(
            JPanel(BorderLayout(8, 0)).apply {
                border = BorderFactory.createEmptyBorder(10, 10, 0, 10)
                add(JLabel("当前 URL："), BorderLayout.WEST)
                add(urlLabel, BorderLayout.CENTER)
            },
            BorderLayout.NORTH,
        )
        add(
            JScrollPane(table).apply {
                border = BorderFactory.createEmptyBorder(0, 10, 0, 10)
            },
            BorderLayout.CENTER,
        )
        add(
            JPanel(BorderLayout()).apply {
                border = BorderFactory.createEmptyBorder(0, 10, 10, 10)
                add(
                    JPanel(FlowLayout(FlowLayout.LEFT, 6, 0)).apply {
                        add(addButton)
                        add(removeButton)
                        add(clearButton)
                        add(reloadButton)
                    },
                    BorderLayout.WEST,
                )
                add(statusLabel, BorderLayout.EAST)
            },
            BorderLayout.SOUTH,
        )

        setLocationRelativeTo(parent)
        reloadCookies()
    }

    private fun reloadCookies() = runCookieAction("读取 Cookie 失败") { uri ->
        cookies = webView.all(uri).sortedWith(COOKIE_NAME_NATURAL_ORDER)
        tableModel.rowCount = 0
        cookies.forEach { cookie ->
            val properties = (cookie as StructedCookie).dict
            tableModel.addRow(
                COOKIE_FIELDS.map { field -> properties[field.key].orEmpty() }.toTypedArray(),
            )
        }
        statusLabel.text = "共 ${cookies.size} 条"
    }

    private fun addCookie() {
        val uri = runCatching(::loadedUrl).getOrElse {
            showError("增加 Cookie 失败", it)
            return
        }
        val nameField = JTextField(24)
        val valueField = JTextField(24)
        val domainField = JTextField(URI(uri).host.orEmpty(), 24)
        val pathField = JTextField(24)
        val expiresField = JTextField(24).apply { isEnabled = false }
        val httpOnlyBox = JCheckBox("HttpOnly")
        val secureBox = JCheckBox("Secure", uri.startsWith("https://", ignoreCase = true))
        val sessionBox = JCheckBox("会话 Cookie", true)
        val sameSiteBox = JComboBox(arrayOf("默认", "LAX", "STRICT", "NONE"))
        sessionBox.addActionListener { expiresField.isEnabled = !sessionBox.isSelected }

        val form = JPanel(GridBagLayout())
        fun addRow(row: Int, label: String, component: Component) {
            form.add(
                JLabel(label),
                GridBagConstraints().apply {
                    gridx = 0
                    gridy = row
                    anchor = GridBagConstraints.LINE_END
                    insets = Insets(4, 4, 4, 8)
                },
            )
            form.add(
                component,
                GridBagConstraints().apply {
                    gridx = 1
                    gridy = row
                    weightx = 1.0
                    fill = GridBagConstraints.HORIZONTAL
                    insets = Insets(4, 0, 4, 4)
                },
            )
        }
        addRow(0, "名称 *", nameField)
        addRow(1, "值", valueField)
        addRow(2, "域", domainField)
        addRow(3, "路径", pathField)
        addRow(4, "过期时间（Unix 毫秒）", expiresField)
        addRow(5, "SameSite", sameSiteBox)
        addRow(
            6,
            "选项",
            JPanel(FlowLayout(FlowLayout.LEFT, 8, 0)).apply {
                add(httpOnlyBox)
                add(secureBox)
                add(sessionBox)
            },
        )

        while (true) {
            val result = JOptionPane.showConfirmDialog(
                this,
                form,
                "增加 Cookie",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE,
            )
            if (result != JOptionPane.OK_OPTION) return

            val name = nameField.text.trim()
            if (name.isEmpty()) {
                JOptionPane.showMessageDialog(this, "Cookie 名称不能为空。")
                continue
            }
            if (!sessionBox.isSelected && expiresField.text.trim().toLongOrNull() == null) {
                JOptionPane.showMessageDialog(this, "非会话 Cookie 需要有效的 Unix 毫秒时间戳。")
                continue
            }

            val properties = linkedMapOf(
                "name" to name,
                "value" to valueField.text,
                "httpOnly" to httpOnlyBox.isSelected.toString(),
                "secure" to secureBox.isSelected.toString(),
                "session" to sessionBox.isSelected.toString(),
            )
            domainField.text.trim().takeIf(String::isNotEmpty)?.let { properties["domain"] = it }
            pathField.text.trim().takeIf(String::isNotEmpty)?.let { properties["path"] = it }
            expiresField.text.trim().takeIf(String::isNotEmpty)?.let { properties["expiresTimeStamp"] = it }
            (sameSiteBox.selectedItem as String)
                .takeUnless { it == "默认" }
                ?.let { properties["sameSite"] = it }

            val succeeded = runCookieAction("增加 Cookie 失败") {
                webView.putCookie(it, StructedCookie(properties))
            }
            if (succeeded) {
                reloadCookies()
                return
            }
        }
    }

    private fun removeSelectedCookies() {
        val selectedCookies = table.selectedRows
            .map(table::convertRowIndexToModel)
            .map(cookies::get)
        if (selectedCookies.isEmpty()) {
            JOptionPane.showMessageDialog(this, "请先选择要删除的 Cookie。")
            return
        }
        if (
            JOptionPane.showConfirmDialog(
                this,
                "确定删除选中的 ${selectedCookies.size} 条 Cookie 吗？",
                "删除 Cookie",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.WARNING_MESSAGE,
            ) != JOptionPane.OK_OPTION
        ) return

        val succeeded = runCookieAction("删除 Cookie 失败") { uri ->
            selectedCookies.forEach { webView.removeCookie(uri, it) }
        }
        if (succeeded) reloadCookies()
    }

    private fun clearCookies() {
        if (
            JOptionPane.showConfirmDialog(
                this,
                "这会清空当前 WebView 数据存储中的全部 Cookie，是否继续？",
                "清空 Cookie",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.WARNING_MESSAGE,
            ) != JOptionPane.OK_OPTION
        ) return

        val succeeded = runCookieAction("清空 Cookie 失败") {
            webView.clearAll()
        }
        if (succeeded) reloadCookies()
    }

    private fun loadedUrl(): String {
        val uri = currentUrl().trim()
        require(
            uri.startsWith("http://", ignoreCase = true) ||
                uri.startsWith("https://", ignoreCase = true),
        ) { "当前页面不是 HTTP/HTTPS URL：$uri" }
        urlLabel.text = uri
        urlLabel.toolTipText = uri
        return uri
    }

    private fun runCookieAction(fallbackError: String, action: (String) -> Unit): Boolean {
        val result = runCatching { action(loadedUrl()) }
        result.exceptionOrNull()?.let {
            statusLabel.text = "操作失败"
            showError(fallbackError, it)
        }
        return result.isSuccess
    }

    private fun showError(fallbackError: String, error: Throwable) {
        JOptionPane.showMessageDialog(
            this,
            error.cause?.message ?: error.message ?: fallbackError,
            fallbackError,
            JOptionPane.ERROR_MESSAGE,
        )
    }

    private companion object {
        private val COOKIE_FIELDS = listOf(
            CookieField("name", "名称"),
            CookieField("value", "值"),
            CookieField("domain", "域"),
            CookieField("path", "路径"),
            CookieField("expiresTimeStamp", "过期时间（Unix 毫秒）"),
            CookieField("httpOnly", "HttpOnly"),
            CookieField("secure", "Secure"),
            CookieField("session", "Session"),
            CookieField("sameSite", "SameSite"),
        )

        private val COOKIE_NAME_NATURAL_ORDER = Comparator<Cookie> { left, right ->
            compareNaturally(left.name, right.name)
        }

        private fun compareNaturally(left: String, right: String): Int {
            var leftIndex = 0
            var rightIndex = 0

            while (leftIndex < left.length && rightIndex < right.length) {
                val leftChar = left[leftIndex]
                val rightChar = right[rightIndex]

                if (leftChar.isDigit() && rightChar.isDigit()) {
                    val result = compareNumberPart(left, leftIndex, right, rightIndex)
                    if (result != 0) return result
                    leftIndex = nextNonDigitIndex(left, leftIndex)
                    rightIndex = nextNonDigitIndex(right, rightIndex)
                    continue
                }

                val caseInsensitive = leftChar.lowercaseChar().compareTo(rightChar.lowercaseChar())
                if (caseInsensitive != 0) return caseInsensitive

                val caseSensitive = leftChar.compareTo(rightChar)
                if (caseSensitive != 0) return caseSensitive


                leftIndex++
                rightIndex++
            }

            return (left.length - leftIndex).compareTo(right.length - rightIndex)
        }

        private fun compareNumberPart(left: String, leftStart: Int, right: String, rightStart: Int): Int {
            val leftEnd = nextNonDigitIndex(left, leftStart)
            val rightEnd = nextNonDigitIndex(right, rightStart)
            val leftSignificantStart = left.indexOfFirstNonZero(leftStart, leftEnd)
            val rightSignificantStart = right.indexOfFirstNonZero(rightStart, rightEnd)
            val leftSignificantLength = leftEnd - leftSignificantStart
            val rightSignificantLength = rightEnd - rightSignificantStart

            if (leftSignificantLength != rightSignificantLength) {
                return leftSignificantLength.compareTo(rightSignificantLength)
            }

            for (offset in 0 until leftSignificantLength) {
                val result = left[leftSignificantStart + offset].compareTo(right[rightSignificantStart + offset])
                if (result != 0) return result
            }

            return (leftEnd - leftStart).compareTo(rightEnd - rightStart)
        }

        private fun nextNonDigitIndex(value: String, start: Int): Int {
            var index = start
            while (index < value.length && value[index].isDigit()) index++
            return index
        }

        private fun String.indexOfFirstNonZero(start: Int, end: Int): Int {
            var index = start
            while (index < end - 1 && this[index] == '0') index++
            return index
        }
    }
}

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
    private val cookiesButton = JButton("Cookies")

    private val path = File("wvbridge").toPath()

    private val webView = WebViewBridgePanel(
        when (jvmTarget) {
            JvmTarget.WINDOWS -> NativeWindowsWebViewPlatformSetting(
                userAgent = "wvbridge",
                dataDir = path.absolutePathString()
            )

            JvmTarget.LINUX -> NativeLinuxWebViewPlatformSetting(
                userAgent = "wvbridge",
                dataDir = path.resolve("data").absolutePathString(),
                cacheDir = path.resolve("cache").absolutePathString(),
            )

            JvmTarget.MACOS -> NativeMacOSWebViewPlatformSetting(
                userAgent = "wvbridge",
                websiteDataStore = NativeMacOSWebViewWebsiteDataStore.DEFAULT
            )
        }
    ) {
        loadUrl(if (urlField.text.isNullOrBlank()) initializeUrl else urlField.text)
    }

    @Volatile
    private var currentLoadedUrl = initializeUrl
    private var isWebViewPresent = webViewInitiallyActive
    private var canGoBack = false
    private var canGoForward = false
    private var isLoading = false

    private fun debug(event: String, value: Any?) =
        LoggerReceiver.log(LoggerReceiver.Level.DEBUG, "BrowserPane - $event", value.toString())

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
            currentLoadedUrl = it
            SwingUtilities.invokeLater {
                isLoading = true
                progressBar.value = 0
                progressBar.isVisible = true
                urlField.text = it
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
            currentLoadedUrl = it
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
        cookiesButton.addActionListener {
            CookieManagerWindow(this, { currentLoadedUrl }, webView).isVisible = true
        }

        val navPanel = JPanel(BorderLayout(8, 0)).apply {
            add(
                JPanel(FlowLayout(FlowLayout.LEFT, 4, 0)).apply {
                    add(backButton)
                    add(forwardButton)
                    add(refreshButton)
                    add(stopButton)
                    add(cookiesButton)
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

internal fun String.truncate(length: Int) = when (this.length) {
    in 0..length -> this
    else -> substring(0, length - 3) + "..."
}

fun main() = SwingUtilities.invokeLater {
    LoggerReceiver.register { level, tag, message ->
        val time = LocalDateTime.now().format(DateTimeFormatter.ofPattern("YYYY-MM-dd HH:mm:ss"))
        val level = level.toString().padEnd(7)
        val tag = tag.truncate(16).padEnd(16)
        println("$time $level: [$tag] - $message")
    }
    val frame = JFrame("WebView Bridge Panel Demo").apply {
        setSize(1200, 600)
        defaultCloseOperation = JFrame.EXIT_ON_CLOSE
        layout = GridLayout(1, 2, 0, 0)
    }

    val initializeUrl =
        "https://app-api.pixiv.net/web/v1/login?code_challenge=qM6bcr4aKuf3-F7QdVO96E2JfeyY3cGyEe3Htu9Pr78&code_challenge_method=S256&client=pixiv-android"
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
