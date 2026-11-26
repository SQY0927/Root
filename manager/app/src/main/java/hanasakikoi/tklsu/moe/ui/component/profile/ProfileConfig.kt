package hanasakikoi.tklsu.moe.ui.component.profile

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import hanasakikoi.tklsu.moe.Natives
import hanasakikoi.tklsu.moe.ui.LocalUiMode
import hanasakikoi.tklsu.moe.ui.UiMode

@Composable
fun AppProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    enabled: Boolean,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppProfileConfigMiuix(
            modifier = modifier,
            fixedName = fixedName,
            enabled = enabled,
            profile = profile,
            onProfileChange = onProfileChange
        )

        UiMode.Material -> AppProfileConfigMaterial(
            modifier = modifier,
            fixedName = fixedName,
            enabled = enabled,
            profile = profile,
            onProfileChange = onProfileChange
        )
    }
}

@Composable
fun RootProfileConfig(
    modifier: Modifier = Modifier,
    fixedName: Boolean,
    enabled: Boolean = true,
    profile: Natives.Profile,
    onProfileChange: (Natives.Profile) -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> RootProfileConfigMiuix(
            modifier = modifier,
            fixedName = fixedName,
            enabled = enabled,
            profile = profile,
            onProfileChange = onProfileChange
        )

        UiMode.Material -> RootProfileConfigMaterial(
            modifier = modifier,
            enabled = enabled,
            profile = profile,
            onProfileChange = onProfileChange
        )
    }
}

