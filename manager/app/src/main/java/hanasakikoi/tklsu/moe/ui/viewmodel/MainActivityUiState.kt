package hanasakikoi.tklsu.moe.ui.viewmodel

import androidx.compose.runtime.Immutable
import hanasakikoi.tklsu.moe.ui.UiMode
import hanasakikoi.tklsu.moe.ui.theme.AppSettings

@Immutable
data class MainActivityUiState(
    val appSettings: AppSettings,
    val pageScale: Float,
    val enableBlur: Boolean,
    val enableFloatingBottomBar: Boolean,
    val enableFloatingBottomBarBlur: Boolean,
    val enableNavigationBadge: Boolean,
    val enableSwipeDismiss: Boolean,
    val pagerInterceptionMode: Int,
    val moduleDescriptionMaxLines: Int = 4,
    val uiMode: UiMode,
)
