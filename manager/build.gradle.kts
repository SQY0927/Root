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
// Version: read from the repository-root Version.txt (line 1 = versionCode,
// line 2 = versionName). The Gradle root project is manager/, so the repo root
// is its parent; a Version.txt next to the Gradle root is accepted too. Both
// lines are required (line 1 positive integer, line 2 non-empty); anything
// malformed invalidates the whole file and falls back to the fixed values.
// A UTF-8 BOM and CR line endings are stripped before parsing.
val versionFromFile: Pair<Int, String>? = run {
    val candidates = listOfNotNull(
        rootProject.projectDir.parentFile?.resolve("Version.txt"),
        rootProject.projectDir.resolve("Version.txt"),
    )
    val file = candidates.firstOrNull { it.isFile }
    if (file == null) {
        logger.warn("TKernelSU: Version.txt not found; falling back to 32680110 / Alpha1.1.0")
        return@run null
    }
    val content = runCatching { file.readText() }.getOrElse { error ->
        logger.warn(
            "TKernelSU: cannot read ${file.absolutePath}: ${error.message}; " +
                "falling back to 32680110 / Alpha1.1.0"
        )
        return@run null
    }.removePrefix("\uFEFF")
    val lines = content.lines()
    val code = lines.getOrNull(0)?.trim()?.toIntOrNull()
    val name = lines.getOrNull(1)?.trim()
    if (code == null || code <= 0 || name.isNullOrEmpty()) {
        logger.warn(
            "TKernelSU: Version.txt is invalid (line 1 must be a positive integer and " +
                "line 2 non-empty); falling back to 32680110 / Alpha1.1.0"
        )
        null
    } else {
        code to name
    }
}

extra["managerVersionCode"] = versionFromFile?.first ?: 32680110
extra["managerVersionName"] = versionFromFile?.second ?: "Alpha1.1.0"
