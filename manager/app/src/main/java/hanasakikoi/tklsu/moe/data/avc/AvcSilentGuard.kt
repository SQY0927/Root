package hanasakikoi.tklsu.moe.data.avc

import android.util.Log
import hanasakikoi.tklsu.moe.data.repository.SettingsRepositoryImpl
import hanasakikoi.tklsu.moe.ksuApp
import hanasakikoi.tklsu.moe.ui.util.withNewRootShell
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

const val AVC_SILENT_MODE_OFF = 0
const val AVC_SILENT_MODE_OBSERVE = 1
const val AVC_SILENT_MODE_ENFORCE = 2

/**
 * 「拦截」档的守护。
 *
 * 管理器进程本身是普通应用 uid，只负责定时校验；每次校验用一次性 root shell
 * 跑一条命令，执行完立即关闭，不会留下常驻的 uid 0 进程。命令内部先读当前
 * 档位，只有不一致时才写，所以正常情况只是一次读。
 */
object AvcSilentGuard {
    private const val TAG = "AvcSilentGuard"
    private const val INTERVAL_MS = 60_000L

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    @Volatile
    private var started = false

    /** 进程启动时调用，幂等。 */
    fun start() {
        if (started) return
        started = true
        scope.launch {
            while (isActive) {
                runCatching { ensureOnce() }.onFailure { Log.w(TAG, "check failed", it) }
                delay(INTERVAL_MS)
            }
        }
    }

    /**
     * 校验一次：期望档位不是拦截就直接返回；内核不支持（没有 perf HAL）也返回；
     * 只有档位被改回其它值时才补写一次。
     */
    fun ensureOnce() {
        val desired = SettingsRepositoryImpl().avcSilentDesiredMode
        if (desired != AVC_SILENT_MODE_ENFORCE) return

        val ksud = shellQuote(File(ksuApp.applicationInfo.nativeLibraryDir, "libksud.so").absolutePath)
        // 一次往返里完成“读 + 必要时写”，不常驻
        val script = "k=$ksud; " +
            "o=\$(\"\$k\" feature get avc_silent 2>&1); " +
            "case \"\$o\" in *not\\ supported*) exit 0;; esac; " +
            "echo \"\$o\" | grep -q '^Value: 2\$' || " +
            "\"\$k\" feature set avc_silent 2 >/dev/null 2>&1"

        withNewRootShell {
            val result = newJob().add(script).to(ArrayList<String>(), null).exec()
            if (!result.isSuccess) {
                Log.w(TAG, "guard command failed: ${result.err.joinToString("\n")}")
            }
        }
    }

    private fun shellQuote(value: String): String = "'" + value.replace("'", "'\\''") + "'"
}
