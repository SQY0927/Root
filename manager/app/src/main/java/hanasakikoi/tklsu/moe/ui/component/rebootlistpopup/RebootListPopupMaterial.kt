package hanasakikoi.tklsu.moe.ui.component.rebootlistpopup

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import hanasakikoi.tklsu.moe.R
import hanasakikoi.tklsu.moe.ui.component.KsuIsValid
import hanasakikoi.tklsu.moe.ui.util.reboot

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
