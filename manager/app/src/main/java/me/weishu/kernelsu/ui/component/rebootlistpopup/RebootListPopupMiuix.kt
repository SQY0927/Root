package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.util.reboot
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun RebootListPopupMiuix(
    modifier: Modifier = Modifier,
) {
    KsuIsValid {
        IconButton(
            modifier = modifier,
            onClick = { reboot("soft_reboot") },
        ) {
            Icon(
                imageVector = Icons.Rounded.PowerSettingsNew,
                contentDescription = stringResource(id = R.string.reboot_soft),
                tint = colorScheme.onBackground
            )
        }
    }
}
