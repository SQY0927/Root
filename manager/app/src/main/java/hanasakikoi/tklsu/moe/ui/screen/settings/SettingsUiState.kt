package hanasakikoi.tklsu.moe.ui.screen.settings

import androidx.compose.runtime.Immutable
import com.materialkolor.PaletteStyle
import com.materialkolor.dynamiccolor.ColorSpec
import hanasakikoi.tklsu.moe.ui.UiMode

@Immutable
data class SettingsUiState(
    val uiMode: String = UiMode.DEFAULT_VALUE,
    val themeMode: Int = 0,
    val miuixMonet: Boolean = false,
    val keyColor: Int = 0,
    val colorStyle: String = PaletteStyle.TonalSpot.name,
    val colorSpec: String = ColorSpec.SpecVersion.SPEC_2025.name,
    val enablePredictiveBack: Boolean = false,
    val enableSwipeDismiss: Boolean = true,
    val pagerInterceptionMode: Int = 1,
    val enableBlur: Boolean = true,
    val enableFloatingBottomBar: Boolean = false,
    val enableFloatingBottomBarBlur: Boolean = false,
    val enableNavigationBadge: Boolean = true,
    val pageScale: Float = 1.0f,
    val moduleDescriptionMaxLines: Int = 4,
    val enableWebDebugging: Boolean = false,

    // Su Compat
    val suCompatStatus: String = "",
    val isSuEnabled: Boolean = false,

    // Su Compat Path (stat/access masquerade)
    val suCompatPathStatus: String = "",
    val isSuCompatPathEnabled: Boolean = false,

    // Kernel Umount
    val kernelUmountStatus: String = "",
    val isKernelUmountEnabled: Boolean = false,

    // SELinux Hide
    val selinuxHideStatus: String = "",
    val isSelinuxHideEnabled: Boolean = false,

    // Global SELinux mode: Enforcing / Permissive / Disabled / Unknown
    val globalSelinuxMode: String = "",
    // 写入 enforce 并回读校准期间置灰，防止连点重入
    val isGlobalSelinuxUpdating: Boolean = false,

    // SU Log
    val sulogStatus: String = "",
    val isSulogEnabled: Boolean = false,

    // Umount Modules
    val isDefaultUmountModules: Boolean = false,

    // ADB Root
    val adbRootStatus: String = "",
    val isAdbRootEnabled: Boolean = false,

    // Shell SU ADB Only
    val shellSuAdbOnlyStatus: String = "",
    val isShellSuAdbOnlyEnabled: Boolean = false,

    // Hide Kallsyms
    val hideKallsymsStatus: String = "",
    val isHideKallsymsEnabled: Boolean = false,

    // AVC Silent: 0 = 关闭, 1 = 只记录, 2 = 拦截
    val avcSilentStatus: String = "",
    val avcSilentMode: Int = 0,

    val isLateLoadMode: Boolean = false,

    // Auto Jailbreak
    val autoJailbreak: Boolean = false,

    // Soft Reboot
    val useSoftReboot: Boolean = false
)

@Immutable
data class SettingsScreenActions(
    val onOpenTheme: () -> Unit,
    val onSetUiModeIndex: (Int) -> Unit,
    val onSetSuCompatEnabled: (Boolean) -> Unit,
    val onSetSuCompatPathEnabled: (Boolean) -> Unit,
    val onSetKernelUmountEnabled: (Boolean) -> Unit,
    val onSetSelinuxHideEnabled: (Boolean) -> Unit,
    val onSetGlobalSelinuxEnforcing: (Boolean) -> Unit,
    val onSetSulogEnabled: (Boolean) -> Unit,
    val onSetAdbRootEnabled: (Boolean) -> Unit,
    val onSetShellSuAdbOnlyEnabled: (Boolean) -> Unit,
    val onSetHideKallsymsEnabled: (Boolean) -> Unit,
    val onSetAvcSilentObserveEnabled: (Boolean) -> Unit,
    val onSetAvcSilentEnforceEnabled: (Boolean) -> Unit,
    val onSetDefaultUmountModules: (Boolean) -> Unit,
    val onSetEnableWebDebugging: (Boolean) -> Unit,
    val onSetAutoJailbreak: (Boolean) -> Unit,
    val onSetUseSoftReboot: (Boolean) -> Unit,
    val onOpenAbout: () -> Unit,
)
