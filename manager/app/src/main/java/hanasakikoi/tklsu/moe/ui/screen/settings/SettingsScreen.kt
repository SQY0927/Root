package hanasakikoi.tklsu.moe.ui.screen.settings

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.unit.Dp
import androidx.lifecycle.compose.LifecycleResumeEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import hanasakikoi.tklsu.moe.ui.LocalUiMode
import hanasakikoi.tklsu.moe.ui.UiMode
import hanasakikoi.tklsu.moe.ui.component.dialog.PermissiveWarningDialog
import hanasakikoi.tklsu.moe.ui.navigation3.Navigator
import hanasakikoi.tklsu.moe.ui.navigation3.Route
import hanasakikoi.tklsu.moe.ui.viewmodel.SettingsViewModel

@Composable
fun SettingPager(
    navigator: Navigator,
    bottomInnerPadding: Dp,
    isCurrentPage: Boolean = true,
) {
    val viewModel = viewModel<SettingsViewModel>()
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val latestIsCurrentPage by rememberUpdatedState(isCurrentPage)
    val initialResumeHandled = rememberSaveable { mutableStateOf(false) }

    LaunchedEffect(isCurrentPage) {
        if (isCurrentPage) {
            viewModel.refresh()
        }
    }

    LifecycleResumeEffect(Unit) {
        if (initialResumeHandled.value && latestIsCurrentPage) {
            viewModel.refresh()
        }
        initialResumeHandled.value = true
        onPauseOrDispose { }
    }

    var showPermissiveWarning by remember { mutableStateOf(false) }

    val actions = SettingsScreenActions(
        onOpenTheme = { navigator.push(Route.ColorPalette) },
        onSetUiModeIndex = { index ->
            viewModel.setUiMode(if (index == 0) UiMode.Miuix.value else UiMode.Material.value)
        },
        onSetSuCompatEnabled = viewModel::setSuCompatEnabled,
        onSetSuCompatPathEnabled = viewModel::setSuCompatPathEnabled,
        onSetKernelUmountEnabled = viewModel::setKernelUmountEnabled,
        onSetSelinuxHideEnabled = viewModel::setSelinuxHideEnabled,
        onSetGlobalSelinuxEnforcing = { enforcing ->
            if (enforcing) {
                viewModel.setGlobalSelinuxEnforcing(true)
            } else {
                // 切到宽松模式必须先确认，弹窗本身不做任何切换
                showPermissiveWarning = true
            }
        },
        onSetSulogEnabled = viewModel::setSulogEnabled,
        onSetAdbRootEnabled = viewModel::setAdbRootEnabled,
        onSetShellSuAdbOnlyEnabled = viewModel::setShellSuAdbOnlyEnabled,
        onSetHideKallsymsEnabled = viewModel::setHideKallsymsEnabled,
        onSetAvcSilentObserveEnabled = viewModel::setAvcSilentObserveEnabled,
        onSetAvcSilentEnforceEnabled = viewModel::setAvcSilentEnforceEnabled,
        onSetDefaultUmountModules = viewModel::setDefaultUmountModules,
        onSetEnableWebDebugging = viewModel::setEnableWebDebugging,
        onSetAutoJailbreak = viewModel::setAutoJailbreak,
        onSetUseSoftReboot = viewModel::setUseSoftReboot,
        onOpenAbout = { navigator.push(Route.About) },
    )

    if (showPermissiveWarning) {
        PermissiveWarningDialog(
            onConfirm = {
                showPermissiveWarning = false
                viewModel.setGlobalSelinuxEnforcing(false)
            },
            onCancel = { showPermissiveWarning = false },
        )
    }

    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingPagerMiuix(uiState, actions, bottomInnerPadding)
        UiMode.Material -> SettingPagerMaterial(uiState, actions, bottomInnerPadding)
    }
}
