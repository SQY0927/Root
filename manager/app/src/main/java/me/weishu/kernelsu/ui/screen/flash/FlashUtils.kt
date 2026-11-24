package me.weishu.kernelsu.ui.screen.flash

import android.net.Uri
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.Parcelable
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Adb
import androidx.compose.material.icons.rounded.DeleteForever
import androidx.compose.material.icons.rounded.RemoveModerator
import androidx.compose.material.icons.rounded.RestartAlt
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.graphics.vector.ImageVector
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.FlashResult
import me.weishu.kernelsu.ui.util.flashModule
import me.weishu.kernelsu.ui.util.restoreBoot
import me.weishu.kernelsu.ui.util.uninstallPermanently
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

enum class FlashingStatus {
    FLASHING,
    SUCCESS,
    FAILED
}

enum class UninstallType(val icon: ImageVector, val title: Int, val message: Int) {
    TEMPORARY(
        Icons.Rounded.RemoveModerator,
        R.string.settings_uninstall_temporary,
        R.string.settings_uninstall_temporary_message
    ),
    PERMANENT(
        Icons.Rounded.DeleteForever,
        R.string.settings_uninstall_permanent,
        R.string.settings_uninstall_permanent_message
    ),
    RESTORE_STOCK_IMAGE(
        Icons.Rounded.RestartAlt,
        R.string.settings_restore_stock_image,
        R.string.settings_restore_stock_image_message
    ),
    NONE(Icons.Rounded.Adb, 0, 0)
}

@Parcelize
sealed class FlashIt : Parcelable {
    @Parcelize
    data class FlashModules(val uris: List<Uri>) : FlashIt()

    @Parcelize
    data object FlashRestore : FlashIt()

    @Parcelize
    data object FlashUninstall : FlashIt()
}

fun flashModulesSequentially(
    uris: List<Uri>,
    onStdout: (String) -> Unit,
    onStderr: (String) -> Unit
): FlashResult {
    var needsReboot = false
    for (uri in uris) {
        flashModule(uri, onStdout, onStderr).apply {
            needsReboot = needsReboot || showReboot
            if (code != 0) {
                return FlashResult(code, err, needsReboot)
            }
        }
    }
    return FlashResult(0, "", needsReboot)
}

fun flashIt(
    flashIt: FlashIt,
    onStdout: (String) -> Unit,
    onStderr: (String) -> Unit
): FlashResult {
    return when (flashIt) {
        is FlashIt.FlashModules -> {
            flashModulesSequentially(flashIt.uris, onStdout, onStderr)
        }

        FlashIt.FlashRestore -> restoreBoot(onStdout, onStderr)
        FlashIt.FlashUninstall -> uninstallPermanently(onStdout, onStderr)
    }
}

@Composable
fun FlashEffect(
    flashIt: FlashIt,
    text: String,
    logContent: StringBuilder,
    onTextUpdate: (String) -> Unit,
    onShowRebootChange: (Boolean) -> Unit,
    onFlashingStatusChange: (FlashingStatus) -> Unit,
) {
    LaunchedEffect(Unit) {
        if (text.isNotEmpty()) {
            return@LaunchedEffect
        }
        var currentText = text
        val mainHandler = Handler(Looper.getMainLooper())
        withContext(Dispatchers.IO) {
            flashIt(flashIt, onStdout = {
                val tempText = "$it\n"
                if (tempText.startsWith("[H[J")) { // clear command
                    currentText = tempText.substring(6)
                } else {
                    currentText += tempText
                }
                mainHandler.post {
                    onTextUpdate(currentText)
                }
                logContent.append(it).append("\n")
            }, onStderr = {
                logContent.append(it).append("\n")
            }).apply {
                if (code != 0) {
                    currentText += "Error code: $code.\n $err Please save and check the log.\n"
                    mainHandler.post {
                        onTextUpdate(currentText)
                    }
                }
                if (showReboot) {
                    currentText += "\n\n\n"
                    mainHandler.post {
                        onTextUpdate(currentText)
                        onShowRebootChange(true)
                    }
                }
                mainHandler.post {
                    onFlashingStatusChange(if (code == 0) FlashingStatus.SUCCESS else FlashingStatus.FAILED)
                }
            }
        }
    }
}

fun saveLog(
    logContent: StringBuilder,
    scope: CoroutineScope,
    showMessage: (String) -> Unit
): () -> Unit {
    return {
        scope.launch {
            val format = SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.getDefault())
            val date = format.format(Date())
            val file = File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                "KernelSU_install_log_${date}.log"
            )
            file.writeText(logContent.toString())
            showMessage("Log saved to ${file.absolutePath}")
        }
    }
}
