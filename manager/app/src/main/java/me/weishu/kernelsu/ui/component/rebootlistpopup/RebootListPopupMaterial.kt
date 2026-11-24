package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.util.reboot

@Composable
fun RebootListPopupMaterial() {
    KsuIsValid {
        IconButton(onClick = { reboot("soft_reboot") }) {
            Icon(
                imageVector = Icons.Filled.PowerSettingsNew,
                contentDescription = stringResource(id = R.string.reboot_soft)
            )
        }
    }
}
