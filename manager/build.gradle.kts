plugins {
    alias(libs.plugins.agp.app) apply false
    alias(libs.plugins.kotlin) apply false
    alias(libs.plugins.compose.compiler) apply false
}

extra["androidMinSdkVersion"] = 31
extra["androidTargetSdkVersion"] = 37
extra["androidCompileSdkVersion"] = 37
extra["androidCompileSdkVersionMinor"] = 0
extra["androidBuildToolsVersion"] = "37.0.0"
extra["androidCompileNdkVersion"] = libs.versions.ndk.get()
extra["androidSourceCompatibility"] = JavaVersion.VERSION_21
extra["androidTargetCompatibility"] = JavaVersion.VERSION_21
extra["managerVersionCode"] = getVersionCode()
extra["managerVersionName"] = getVersionName()

fun gitOutput(vararg args: String): String? {
    return try {
        val process = ProcessBuilder(listOf("git") + args)
            .redirectError(ProcessBuilder.Redirect.DISCARD)
            .start()
        val text = process.inputStream.bufferedReader().use { it.readText() }.trim()
        if (process.waitFor() == 0 && text.isNotEmpty()) text else null
    } catch (_: Exception) {
        null
    }
}

fun getVersionCode(): Int {
    val envVersion = System.getenv("KSU_VERSION")?.trim()?.toIntOrNull()
    if (envVersion != null) {
        return envVersion
    }

    val commitCount = gitOutput("rev-list", "--count", "HEAD")?.toIntOrNull()
    if (commitCount != null) {
        return 30000 + commitCount
    }

    return 30000
}

fun getVersionName(): String {
    return gitOutput("describe", "--tags", "--always")
        ?: System.getenv("KSU_VERSION")?.trim()?.takeIf { it.isNotEmpty() }
        ?: "30000"
}
