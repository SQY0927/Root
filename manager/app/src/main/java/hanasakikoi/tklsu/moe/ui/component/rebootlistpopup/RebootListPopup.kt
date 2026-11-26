package hanasakikoi.tklsu.moe.ui.component.rebootlistpopup

import androidx.compose.runtime.Composable
import hanasakikoi.tklsu.moe.ui.LocalUiMode
import hanasakikoi.tklsu.moe.ui.UiMode

@Composable
fun RebootListPopup() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> RebootListPopupMiuix()
        UiMode.Material -> RebootListPopupMaterial()
    }
}
