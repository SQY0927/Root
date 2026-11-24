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

// Version: mirror the upstream KernelSU release. Upstream derives versionCode
// as 30000 + git commit count; v3.3.1 is 30000 + 2680 = 32680. Keep it fixed so
// builds without git metadata (CI, exported source trees) report the same
// number instead of falling back to 0/30000.
fun getVersionCode(): Int = 32680

fun getVersionName(): String = "Alpha1.0.0"
