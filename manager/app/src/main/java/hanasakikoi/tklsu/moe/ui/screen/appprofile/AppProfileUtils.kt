package hanasakikoi.tklsu.moe.ui.screen.appprofile

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import hanasakikoi.tklsu.moe.R

enum class Mode(@field:StringRes private val res: Int) {
    Default(R.string.profile_default),
    Custom(R.string.profile_custom);

    val text: String
        @Composable get() = stringResource(res)
}
