package hanasakikoi.tklsu.moe.ui.screen.settings

import android.widget.Toast
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.Article
import androidx.compose.material.icons.automirrored.rounded.Rule
import androidx.compose.material.icons.rounded.Adb
import androidx.compose.material.icons.rounded.AdminPanelSettings
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.DeveloperMode
import androidx.compose.material.icons.rounded.DisplaySettings
import androidx.compose.material.icons.rounded.FindInPage
import androidx.compose.material.icons.rounded.FlashOn
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.LayersClear
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.RestartAlt
import androidx.compose.material.icons.rounded.Security
import androidx.compose.material.icons.rounded.VerifiedUser
import androidx.compose.material.icons.rounded.VisibilityOff
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import hanasakikoi.tklsu.moe.R
import hanasakikoi.tklsu.moe.ui.UiMode
import hanasakikoi.tklsu.moe.ui.component.KsuIsValid
import hanasakikoi.tklsu.moe.ui.component.dialog.rememberLoadingDialog
import hanasakikoi.tklsu.moe.ui.component.miuix.SendLogDialog
import hanasakikoi.tklsu.moe.ui.theme.LocalEnableBlur
import hanasakikoi.tklsu.moe.ui.util.BlurredBar
import hanasakikoi.tklsu.moe.ui.util.rememberBlurBackdrop
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.blur.layerBackdrop
import top.yukonga.miuix.kmp.preference.ArrowPreference
import top.yukonga.miuix.kmp.preference.OverlayDropdownPreference
import top.yukonga.miuix.kmp.preference.SwitchPreference
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

/**
 * @author weishu
 * @date 2023/1/1.
 */
@Composable
fun SettingPagerMiuix(
    uiState: SettingsUiState,
    actions: SettingsScreenActions,
    bottomInnerPadding: Dp,
) {
    val scrollBehavior = MiuixScrollBehavior()
    val enableBlur = LocalEnableBlur.current
    val backdrop = rememberBlurBackdrop(enableBlur)
    val blurActive = backdrop != null
    val barColor = if (blurActive) Color.Transparent else colorScheme.surface
    val loadingDialog = rememberLoadingDialog()
    val showSendLogDialog = rememberSaveable { mutableStateOf(false) }
    val context = LocalContext.current

    Scaffold(
        topBar = {
            BlurredBar(backdrop) {
                TopAppBar(
                    color = barColor,
                    title = stringResource(R.string.settings),
                    scrollBehavior = scrollBehavior
                )
            }
        },
        popupHost = { },
        contentWindowInsets = WindowInsets.systemBars.add(WindowInsets.displayCutout).only(WindowInsetsSides.Horizontal),
    ) { innerPadding ->
        Box(modifier = if (backdrop != null) Modifier.layerBackdrop(backdrop) else Modifier) {
            LazyColumn(
                modifier = Modifier
                    .fillMaxHeight()
                    .scrollEndHaptic()
                    .overScrollVertical()
                    .nestedScroll(scrollBehavior.nestedScrollConnection)
                    .padding(horizontal = 12.dp),
                contentPadding = innerPadding,
                overscrollEffect = null,
            ) {
                item {
                    Card(
                        modifier = Modifier
                            .padding(top = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        OverlayDropdownPreference(
                            title = stringResource(id = R.string.settings_ui_mode),
                            summary = stringResource(id = R.string.settings_ui_mode_summary),
                            items = UiMode.entries.map { it.name },
                            startAction = {
                                Icon(
                                    Icons.Rounded.DisplaySettings,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_ui_mode),
                                    tint = colorScheme.onBackground
                                )
                            },
                            selectedIndex = if (uiState.uiMode == UiMode.Material.value) 1 else 0,
                            onSelectedIndexChange = actions.onSetUiModeIndex
                        )
                        ArrowPreference(
                            title = stringResource(id = R.string.settings_theme),
                            summary = stringResource(id = R.string.settings_theme_summary),
                            startAction = {
                                Icon(
                                    Icons.Rounded.Palette,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.settings_theme),
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = actions.onOpenTheme
                        )
                    }

                    KsuIsValid {
                        Card(
                            modifier = Modifier
                                .padding(top = 12.dp)
                                .fillMaxWidth(),
                        ) {
                            val kernelUmountSummary = when (uiState.kernelUmountStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_kernel_umount_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_kernel_umount),
                                summary = kernelUmountSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.LayersClear,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_kernel_umount),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isKernelUmountEnabled,
                                enabled = uiState.kernelUmountStatus == "supported",
                                onCheckedChange = actions.onSetKernelUmountEnabled
                            )

                            val selinuxHideSummary = when (uiState.selinuxHideStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_selinux_hide_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_selinux_hide),
                                summary = selinuxHideSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.Security,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_selinux_hide),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isSelinuxHideEnabled,
                                enabled = uiState.selinuxHideStatus == "supported",
                                onCheckedChange = actions.onSetSelinuxHideEnabled
                            )

                            val selinuxModeText = when (uiState.globalSelinuxMode) {
                                "Enforcing" -> stringResource(id = R.string.selinux_status_enforcing)
                                "Permissive" -> stringResource(id = R.string.selinux_status_permissive)
                                "Disabled" -> stringResource(id = R.string.selinux_status_disabled)
                                else -> stringResource(id = R.string.selinux_status_unknown)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_selinux_mode),
                                summary = stringResource(id = R.string.settings_selinux_mode_summary, selinuxModeText),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.Lock,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_selinux_mode),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.globalSelinuxMode == "Enforcing",
                                enabled = (uiState.globalSelinuxMode == "Enforcing" || uiState.globalSelinuxMode == "Permissive") &&
                                    !uiState.isGlobalSelinuxUpdating,
                                onCheckedChange = actions.onSetGlobalSelinuxEnforcing
                            )

                            val sulogSummary = when (uiState.sulogStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sulog_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_sulog),
                                summary = sulogSummary,
                                startAction = {
                                    Icon(
                                        Icons.AutoMirrored.Rounded.Article,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_sulog),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isSulogEnabled,
                                enabled = uiState.sulogStatus == "supported",
                                onCheckedChange = actions.onSetSulogEnabled
                            )

                            val adbRootSummary = when (uiState.adbRootStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_adb_root_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_adb_root),
                                summary = adbRootSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.Adb,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_adb_root),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isAdbRootEnabled,
                                enabled = uiState.adbRootStatus == "supported",
                                onCheckedChange = actions.onSetAdbRootEnabled
                            )

                            val shellSuAdbOnlySummary = when (uiState.shellSuAdbOnlyStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_shell_su_adb_only_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_shell_su_adb_only),
                                summary = shellSuAdbOnlySummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.VerifiedUser,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_shell_su_adb_only),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isShellSuAdbOnlyEnabled,
                                enabled = uiState.shellSuAdbOnlyStatus == "supported",
                                onCheckedChange = actions.onSetShellSuAdbOnlyEnabled
                            )

                            val hideKallsymsSummary = when (uiState.hideKallsymsStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_hide_kallsyms_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_hide_kallsyms),
                                summary = hideKallsymsSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.VisibilityOff,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_hide_kallsyms),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isHideKallsymsEnabled,
                                onCheckedChange = {
                                    if (uiState.hideKallsymsStatus == "supported") {
                                        actions.onSetHideKallsymsEnabled(it)
                                    } else {
                                        Toast.makeText(context, hideKallsymsSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )

                            val avcSilentObserveSummary = when (uiState.avcSilentStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_avc_silent_observe_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_avc_silent_observe),
                                summary = avcSilentObserveSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.FindInPage,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_avc_silent_observe),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.avcSilentMode == 1,
                                enabled = uiState.avcSilentStatus == "supported",
                                onCheckedChange = actions.onSetAvcSilentObserveEnabled
                            )

                            val avcSilentEnforceSummary = when (uiState.avcSilentStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_avc_silent_enforce_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_avc_silent_enforce),
                                summary = avcSilentEnforceSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.Security,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_avc_silent_enforce),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.avcSilentMode == 2,
                                enabled = uiState.avcSilentStatus == "supported",
                                onCheckedChange = actions.onSetAvcSilentEnforceEnabled
                            )

                            val suCompatSummary = when (uiState.suCompatStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sucompat_execve_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_sucompat_execve),
                                summary = suCompatSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.AdminPanelSettings,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_sucompat_execve),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isSuEnabled,
                                onCheckedChange = {
                                    if (uiState.suCompatStatus == "supported") {
                                        actions.onSetSuCompatEnabled(it)
                                    } else {
                                        Toast.makeText(context, suCompatSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )

                            val suCompatPathSummary = when (uiState.suCompatPathStatus) {
                                "unsupported" -> stringResource(id = R.string.feature_status_unsupported_summary)
                                "managed" -> stringResource(id = R.string.feature_status_managed_summary)
                                "forced" -> stringResource(id = R.string.feature_status_forced_summary)
                                else -> stringResource(id = R.string.settings_sucompat_path_summary)
                            }
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_sucompat_path),
                                summary = suCompatPathSummary,
                                startAction = {
                                    Icon(
                                        Icons.Rounded.FindInPage,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_sucompat_path),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isSuCompatPathEnabled,
                                onCheckedChange = {
                                    if (uiState.suCompatPathStatus == "supported") {
                                        actions.onSetSuCompatPathEnabled(it)
                                    } else {
                                        Toast.makeText(context, suCompatPathSummary, Toast.LENGTH_SHORT).show()
                                    }
                                }
                            )

                            SwitchPreference(
                                title = stringResource(id = R.string.settings_soft_reboot),
                                summary = stringResource(id = R.string.settings_soft_reboot_summary),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.RestartAlt,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_soft_reboot),
                                        tint = if (uiState.isLateLoadMode) colorScheme.disabledOnSecondaryVariant else colorScheme.onBackground
                                    )
                                },
                                enabled = !uiState.isLateLoadMode,
                                checked = uiState.isLateLoadMode || uiState.useSoftReboot,
                                onCheckedChange = actions.onSetUseSoftReboot
                            )
                        }

                        Card(
                            modifier = Modifier
                                .padding(top = 12.dp)
                                .fillMaxWidth(),
                        ) {
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_umount_modules_default),
                                summary = stringResource(id = R.string.settings_umount_modules_default_summary),
                                startAction = {
                                    Icon(
                                        Icons.AutoMirrored.Rounded.Rule,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_umount_modules_default),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.isDefaultUmountModules,
                                onCheckedChange = actions.onSetDefaultUmountModules
                            )

                            SwitchPreference(
                                title = stringResource(id = R.string.enable_web_debugging),
                                summary = stringResource(id = R.string.enable_web_debugging_summary),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.DeveloperMode,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.enable_web_debugging),
                                        tint = colorScheme.onBackground
                                    )
                                },
                                checked = uiState.enableWebDebugging,
                                onCheckedChange = actions.onSetEnableWebDebugging
                            )
                            SwitchPreference(
                                title = stringResource(id = R.string.settings_auto_jailbreak),
                                summary = stringResource(id = R.string.settings_auto_jailbreak_summary),
                                startAction = {
                                    Icon(
                                        Icons.Rounded.FlashOn,
                                        modifier = Modifier.padding(end = 6.dp),
                                        contentDescription = stringResource(id = R.string.settings_auto_jailbreak),
                                        tint = if (uiState.isLateLoadMode) colorScheme.onBackground else colorScheme.disabledOnSecondaryVariant
                                    )
                                },
                                enabled = uiState.isLateLoadMode,
                                checked = uiState.autoJailbreak,
                                onCheckedChange = actions.onSetAutoJailbreak
                            )
                        }
                    }

                    Card(
                        modifier = Modifier
                            .padding(vertical = 12.dp)
                            .fillMaxWidth(),
                    ) {
                        ArrowPreference(
                            title = stringResource(id = R.string.send_log),
                            startAction = {
                                Icon(
                                    Icons.Rounded.BugReport,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = stringResource(id = R.string.send_log),
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = { showSendLogDialog.value = true },
                        )
                        SendLogDialog(
                            show = showSendLogDialog.value,
                            onDismissRequest = { showSendLogDialog.value = false },
                            loadingDialog = loadingDialog
                        )
                        val about = stringResource(id = R.string.about)
                        ArrowPreference(
                            title = about,
                            startAction = {
                                Icon(
                                    Icons.Rounded.Info,
                                    modifier = Modifier.padding(end = 6.dp),
                                    contentDescription = about,
                                    tint = colorScheme.onBackground
                                )
                            },
                            onClick = actions.onOpenAbout,
                        )
                    }
                    Spacer(Modifier.height(bottomInnerPadding))
                }
            }
        }
    }
}
