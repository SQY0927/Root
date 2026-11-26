package hanasakikoi.tklsu.moe.ui.component.dialog

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import hanasakikoi.tklsu.moe.R
import hanasakikoi.tklsu.moe.ui.theme.isInDarkTheme

/**
 * 切换到宽容（Permissive）模式前从底部弹出的红色警告窗口。
 *
 * 只有点击“确认”才会真正切换；点“取消”只关闭窗口，不做任何修改。
 * 遮罩与返回键被禁用，避免误触关闭后误以为已经切换。
 */
@Composable
fun PermissiveWarningDialog(
    onConfirm: () -> Unit,
    onCancel: () -> Unit,
) {
    val dark = isInDarkTheme()
    val cardColor = if (dark) Color(0xFF1F1F1F) else Color(0xFFFFFFFF)
    val warningColor = if (dark) Color(0xFFFF6B6B) else Color(0xFFD32F2F)
    val secondaryColor = if (dark) Color(0xFFBDBDBD) else Color(0xFF616161)

    Dialog(
        onDismissRequest = { },
        properties = DialogProperties(
            dismissOnBackPress = false,
            dismissOnClickOutside = false,
            usePlatformDefaultWidth = false,
        ),
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0x66000000)),
            contentAlignment = Alignment.BottomCenter,
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
                    .background(cardColor)
                    .navigationBarsPadding()
                    .padding(horizontal = 24.dp, vertical = 20.dp),
            ) {
                Text(
                    text = stringResource(R.string.settings_selinux_permissive_warning_title),
                    color = warningColor,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(10.dp))
                Text(
                    text = stringResource(R.string.settings_selinux_permissive_warning_message),
                    color = warningColor,
                    fontSize = 14.sp,
                    lineHeight = 20.sp,
                )
                Spacer(Modifier.height(20.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = stringResource(android.R.string.cancel),
                        color = secondaryColor,
                        fontSize = 15.sp,
                        modifier = Modifier
                            .clip(RoundedCornerShape(50))
                            .clickable(onClick = onCancel)
                            .padding(horizontal = 18.dp, vertical = 10.dp),
                    )
                    Spacer(Modifier.width(12.dp))
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(50))
                            .background(warningColor)
                            .clickable(onClick = onConfirm)
                            .padding(horizontal = 22.dp, vertical = 10.dp),
                    ) {
                        Text(
                            text = stringResource(R.string.settings_selinux_permissive_confirm),
                            color = Color.White,
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Medium,
                        )
                    }
                }
            }
        }
    }
}
