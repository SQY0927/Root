package me.weishu.kernelsu.ui.screen.settings

import android.widget.Toast
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Article
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.automirrored.filled.Rule
import androidx.compose.material.icons.filled.Adb
import androidx.compose.material.icons.filled.AdminPanelSettings
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.DeveloperMode
import androidx.compose.material.icons.filled.DisplaySettings
import androidx.compose.material.icons.filled.FindInPage
import androidx.compose.material.icons.filled.FlashOn
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.LayersClear
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.RestartAlt
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.VerifiedUser
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.Icon
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.material.ExpressiveScaffold
import me.weishu.kernelsu.ui.component.material.SegmentedColumn
import me.weishu.kernelsu.ui.component.material.SegmentedDropdownItem
import me.weishu.kernelsu.ui.component.material.SegmentedListItem
import me.weishu.kernelsu.ui.component.material.SegmentedSwitchItem
import me.weishu.kernelsu.ui.component.material.SendLogBottomSheet
import me.weishu.kernelsu.ui.component.material.SnackBarHost
import me.weishu.kernelsu.ui.component.material.expressiveTopAppBarColors
import me.weishu.kernelsu.ui.component.uninstalldialog.UninstallDialog

/**
 * @author weishu
 * @date 2023/1/1.
 */
@Composable
fun SettingPagerMaterial(
    uiState: SettingsUiState,
    actions: SettingsScreenActions,
    bottomInnerPadding: Dp,
) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val snackBarHost = remember { SnackbarHostState() }
    val showUninstallDialog = rememberSaveable { mutableStateOf(false) }
    var showBottomSheet by remember { mutableStateOf(false) }
    val context = LocalContext.current

    UninstallDialog(
        show = showUninstallDialog.value,
        onDismissRequest = { showUninstallDialog.value = false }
    )

    ExpressiveScaffold(
        topBar = {
            TopBar(scrollBehavior = scrollBehavior)
        },
        snackbarHost = { SnackBarHost(hostState = snackBarHost, modifier = Modifier.padding(bottom = bottomInnerPadding)) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .padding(paddingValues)
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(rememberScrollState())
        ) {
            SegmentedColumn(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                content = buildList {
                    add {
                        SegmentedDropdownItem(
                            icon = Icons.Filled.DisplaySettings,
                            title = stringResource(id = R.string.settings_ui_mode),
                            summary = stringResource(id = R.string.settings_ui_mode_summary),
                            items = UiMode.entries.map { it.name },
                            selectedIndex = if (uiState.uiMode == UiMode.Material.value) 1 else 0,
                            onItemSelected = actions.onSetUiModeIndex
                        )
                    }
                    add {
                        SegmentedListItem(
                            onClick = actions.onOpenTheme,
                            headlineContent = { Text(stringResource(id = R.string.settings_theme)) },
                            supportingContent = { Text(stringResource(id = R.string.settings_theme_summary)) },
                            leadingContent = { Icon(Icons.Filled.Palette, stringResource(id = R.string.settings_theme)) },
                            trailingContent = {
                                Icon(
                                    Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                    null
                                )
                            }
                        )
                    }
                }
            )

            val profileTemplate = stringResource(id = R.string.settings_profile_template)
            KsuIsValid {
                SegmentedColumn(
                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                    content = listOf {
                        SegmentedListItem(
                            onClick = actions.onOpenProfileTemplate,
                            headlineContent = { Text(profileTemplate) },
                            supportingContent = { Text(stringResource(id = R.string.settings_profile_template_summary)) },
                            leadingContent = { Icon(Icons.Filled.Description, profileTemplate) },
                            trailingContent = {
                                Icon(
                                    Icons.AutoMirrored.Filled.KeyboardArrowRight,
                                    null
                                )
                            }
                        )
                    }
                )
            }

            KsuIsValid {
                SegmentedColumn(
                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                    content = listOf(
                        {
                            val kernelUmountSummary = when (uiState.kernelUmountStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_kernel_umount_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.LayersClear,
                                title = stringResource(id = R.string.settings_kernel_umount),
                                summary = kernelUmountSummary,
                                checked = uiState.isKernelUmountEnabled,
                                enabled = uiState.kernelUmountStatus == "supported",
                                onCheckedChange = actions.onSetKernelUmountEnabled
                            )
                        },
                        {
                            val selinuxHideSummary = when (uiState.selinuxHideStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_selinux_hide_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.Security,
                                title = stringResource(id = R.string.settings_selinux_hide),
                                summary = selinuxHideSummary,
                                checked = uiState.isSelinuxHideEnabled,
                                enabled = uiState.selinuxHideStatus == "supported",
                                onCheckedChange = actions.onSetSelinuxHideEnabled
                            )
                        },
                        {
                            val sulogSummary = when (uiState.sulogStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sulog_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.AutoMirrored.Filled.Article,
                                title = stringResource(id = R.string.settings_sulog),
                                summary = sulogSummary,
                                checked = uiState.isSulogEnabled,
                                enabled = uiState.sulogStatus == "supported",
                                onCheckedChange = actions.onSetSulogEnabled
                            )
                        },
                        {
                            val adbRootSummary = when (uiState.adbRootStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_adb_root_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.Adb,
                                title = stringResource(id = R.string.settings_adb_root),
                                summary = adbRootSummary,
                                checked = uiState.isAdbRootEnabled,
                                enabled = uiState.adbRootStatus == "supported",
                                onCheckedChange = actions.onSetAdbRootEnabled
                            )
                        },
                        {
                            val shellSuAdbOnlySummary = when (uiState.shellSuAdbOnlyStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_shell_su_adb_only_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.VerifiedUser,
                                title = stringResource(id = R.string.settings_shell_su_adb_only),
                                summary = shellSuAdbOnlySummary,
                                checked = uiState.isShellSuAdbOnlyEnabled,
                                enabled = uiState.shellSuAdbOnlyStatus == "supported",
                                onCheckedChange = actions.onSetShellSuAdbOnlyEnabled
                            )
                        },
                        {
                            val hideKallsymsSummary = when (uiState.hideKallsymsStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_hide_kallsyms_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.VisibilityOff,
                                title = stringResource(id = R.string.settings_hide_kallsyms),
                                summary = hideKallsymsSummary,
                                checked = uiState.isHideKallsymsEnabled,
                                onCheckedChange = {
                                    if (uiState.hideKallsymsStatus == "supported") {
                                        actions.onSetHideKallsymsEnabled(it)
                                    } else {
                                        Toast.makeText(context, hideKallsymsSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )
                        },
                        {
                            val suCompatSummary = when (uiState.suCompatStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sucompat_execve_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.AdminPanelSettings,
                                title = stringResource(id = R.string.settings_sucompat_execve),
                                summary = suCompatSummary,
                                checked = uiState.isSuEnabled,
                                onCheckedChange = {
                                    if (uiState.suCompatStatus == "supported") {
                                        actions.onSetSuCompatEnabled(it)
                                    } else {
                                        Toast.makeText(context, suCompatSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )
                        },
                        {
                            val suCompatPathSummary = when (uiState.suCompatPathStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sucompat_path_summary)
                            }
                            SegmentedSwitchItem(
                                icon = Icons.Filled.FindInPage,
                                title = stringResource(id = R.string.settings_sucompat_path),
                                summary = suCompatPathSummary,
                                checked = uiState.isSuCompatPathEnabled,
                                onCheckedChange = {
                                    if (uiState.suCompatPathStatus == "supported") {
                                        actions.onSetSuCompatPathEnabled(it)
                                    } else {
                                        Toast.makeText(context, suCompatPathSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )
                        },
                        {
                            SegmentedSwitchItem(
                                icon = Icons.Filled.RestartAlt,
                                title = stringResource(id = R.string.settings_soft_reboot),
                                summary = stringResource(id = R.string.settings_soft_reboot_summary),
                                enabled = !uiState.isLateLoadMode,
                                checked = uiState.isLateLoadMode || uiState.useSoftReboot,
                                onCheckedChange = actions.onSetUseSoftReboot
                            )
                        },
                    )
                )

                SegmentedColumn(
                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                    content = listOf(
                        {
                            SegmentedSwitchItem(
                                icon = Icons.AutoMirrored.Filled.Rule,
                                title = stringResource(id = R.string.settings_umount_modules_default),
                                summary = stringResource(id = R.string.settings_umount_modules_default_summary),
                                checked = uiState.isDefaultUmountModules,
                                onCheckedChange = actions.onSetDefaultUmountModules
                            )
                        },
                        {
                            SegmentedSwitchItem(
                                icon = Icons.Filled.DeveloperMode,
                                title = stringResource(id = R.string.enable_web_debugging),
                                summary = stringResource(id = R.string.enable_web_debugging_summary),
                                checked = uiState.enableWebDebugging,
                                onCheckedChange = actions.onSetEnableWebDebugging
                            )
                        },
                        {
                            SegmentedSwitchItem(
                                icon = Icons.Filled.FlashOn,
                                title = stringResource(id = R.string.settings_auto_jailbreak),
                                summary = stringResource(id = R.string.settings_auto_jailbreak_summary),
                                enabled = uiState.isLateLoadMode,
                                checked = uiState.autoJailbreak,
                                onCheckedChange = actions.onSetAutoJailbreak
                            )
                        }
                    )
                )
            }

            if (uiState.isLkmMode) {
                SegmentedColumn(
                    modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                    content = listOf(
                        {
                            val uninstall = stringResource(id = R.string.settings_uninstall)
                            SegmentedListItem(
                                onClick = { showUninstallDialog.value = true },
                                enabled = !uiState.isLateLoadMode,
                                headlineContent = { Text(uninstall) },
                                leadingContent = { Icon(Icons.Filled.Delete, uninstall) }
                            )
                        }
                    )
                )
            }

            SegmentedColumn(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp, bottom = 13.dp),
                content = listOf(
                    {
                        SegmentedListItem(
                            onClick = { showBottomSheet = true },
                            headlineContent = { Text(stringResource(id = R.string.send_log)) },
                            leadingContent = {
                                Icon(
                                    Icons.Filled.BugReport,
                                    stringResource(id = R.string.send_log)
                                )
                            },
                        )
                    },
                    {
                        SegmentedListItem(
                            onClick = actions.onOpenAbout,
                            headlineContent = { Text(stringResource(id = R.string.about)) },
                            leadingContent = {
                                Icon(
                                    Icons.Filled.Info,
                                    stringResource(id = R.string.about)
                                )
                            },
                        )
                    }
                )
            )

            Spacer(modifier = Modifier.height(8.dp))

            if (showBottomSheet) {
                SendLogBottomSheet(
                    onDismiss = { showBottomSheet = false },
                    snackbarHostState = snackBarHost,
                )
            }
            Spacer(modifier = Modifier.height(bottomInnerPadding))
        }
    }
}

@Composable
private fun TopBar(
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    LargeFlexibleTopAppBar(
        title = { Text(stringResource(R.string.settings)) },
        colors = expressiveTopAppBarColors(),
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}
