@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE")

package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.currentCompositeKeyHashCode
import androidx.compose.runtime.remember
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.awt.InteropFocusSwitcher
import androidx.compose.ui.awt.NoOpUpdate
import androidx.compose.ui.awt.SwingInteropViewGroup
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.IntrinsicMeasurable
import androidx.compose.ui.layout.IntrinsicMeasureScope
import androidx.compose.ui.layout.Measurable
import androidx.compose.ui.layout.MeasurePolicy
import androidx.compose.ui.layout.MeasureResult
import androidx.compose.ui.layout.MeasureScope
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.InteropView
import androidx.compose.ui.viewinterop.LocalInteropContainer
import androidx.compose.ui.viewinterop.SwingInteropViewHolder
import java.awt.Component

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/8/12 12:01
 * ================================================
 */


/**
 * fix swing-panel can't use in higher compose version bug.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
internal fun <T : Component> SupportSwingPanel(
    background: Color = Color.White,
    factory: () -> T,
    modifier: Modifier = Modifier,
    update: (T) -> Unit = NoOpUpdate,
) {
    val interopContainer = LocalInteropContainer.current
    val compositeKeyHashCode = currentCompositeKeyHashCode
    val focusManager = LocalFocusManager.current

    // TODO: entire interop context must be inside SwingInteropViewHolder in order to
    //  expose a version of this API with `onReset` callback and integrated with ReusableComposeNode
    //  https://youtrack.jetbrains.com/issue/CMP-5897/Desktop-self-contained-InteropViewHolder

    val group = remember {
        SwingInteropViewGroup(
            key = compositeKeyHashCode,
            focusComponent = interopContainer.root
        )
    }

    // TODO(https://youtrack.jetbrains.com/issue/CMP-7557/SwingInteropViewHolder.-Commonization-of-focus-logic-with-different-targets)
    //  we probably can commonize this logic across different targets (including Android)
    val focusSwitcher = remember { InteropFocusSwitcher(group, focusManager) }

    val interopViewHolder = remember {
        SwingInteropViewHolder(
            factory = factory,
            container = interopContainer,
            group = group,
            focusSwitcher = focusSwitcher,
            compositeKeyHashCode = compositeKeyHashCode,
            measurePolicy = AwtContentMeasurePolicy(group)
        )
    }

    InteropView(
        factory = { interopViewHolder },
        modifier = modifier.then(focusSwitcher.modifier),
        update = {
            it.background = background.toAwtColor()
            update(it)
        }
    )
}

private class AwtContentMeasurePolicy(
    val component: Component,
) : MeasurePolicy {

    private fun Density.awtToPx(awtPx: Int): Int = awtPx.dp.roundToPx()

    override fun MeasureScope.measure(
        measurables: List<Measurable>,
        constraints: Constraints
    ): MeasureResult {
        val prefSize = component.preferredSize
        val width = awtToPx(prefSize.width).coerceIn(constraints.minWidth, constraints.maxWidth)
        val height = awtToPx(prefSize.height).coerceIn(constraints.minHeight, constraints.maxHeight)
        return layout(width, height) {}
    }

    override fun IntrinsicMeasureScope.minIntrinsicWidth(
        measurables: List<IntrinsicMeasurable>,
        height: Int
    ): Int {
        return awtToPx(component.minimumSize.width)
    }

    override fun IntrinsicMeasureScope.minIntrinsicHeight(
        measurables: List<IntrinsicMeasurable>,
        width: Int
    ): Int {
        return awtToPx(component.minimumSize.height)
    }

    override fun IntrinsicMeasureScope.maxIntrinsicWidth(
        measurables: List<IntrinsicMeasurable>,
        height: Int
    ): Int {
        return awtToPx(component.maximumSize.width)
    }

    override fun IntrinsicMeasureScope.maxIntrinsicHeight(
        measurables: List<IntrinsicMeasurable>,
        width: Int
    ): Int {
        return awtToPx(component.maximumSize.height)
    }
}

internal fun Color.toAwtColor() = java.awt.Color(red, green, blue, alpha)
