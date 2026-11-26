package hanasakikoi.tklsu.moe.data.repository

interface SettingsRepository {
    var uiMode: String
    var themeMode: Int
    var miuixMonet: Boolean
    var keyColor: Int
    var colorStyle: String
    var colorSpec: String
    var enablePredictiveBack: Boolean
    var enableSwipeDismiss: Boolean
    var pagerInterceptionMode: Int
    var enableBlur: Boolean
    var enableFloatingBottomBar: Boolean
    var enableFloatingBottomBarBlur: Boolean
    var enableNavigationBadge: Boolean
    var navigationRailExpanded: Boolean
    var pageScale: Float
    var moduleDescriptionMaxLines: Int
    var enableWebDebugging: Boolean
    var moduleSortEnabledFirst: Boolean
    var moduleSortActionFirst: Boolean
    /** Name of the [hanasakikoi.tklsu.moe.ui.screen.modulerepo.RepoSort] entry. */
    var moduleRepoSortOrder: String
    var superuserShowSystemApps: Boolean
    var superuserShowOnlyPrimaryUserApps: Boolean
    var superuserSortOption: Int
    var suLogFilters: Set<String>?
    var autoJailbreak: Boolean
    var useSoftReboot: Boolean
    val intentToken: String

    suspend fun getSuCompatStatus(): String
    suspend fun getSuCompatPersistValue(): Long?
    fun isSuEnabled(): Boolean
    fun setSuEnabled(enabled: Boolean): Boolean

    suspend fun getSuCompatPathStatus(): String
    suspend fun getSuCompatPathPersistValue(): Long?
    suspend fun getSuCompatPathValue(): Long?
    fun setSuCompatPathEnabled(enabled: Boolean): Boolean

    suspend fun getKernelUmountStatus(): String
    fun isKernelUmountEnabled(): Boolean
    fun setKernelUmountEnabled(enabled: Boolean): Boolean

    suspend fun getSelinuxHideStatus(): String
    fun isSelinuxHideEnabled(): Boolean
    fun setSelinuxHideEnabled(enabled: Boolean): Int

    /** Global SELinux mode: Enforcing / Permissive / Disabled / Unknown. */
    suspend fun getGlobalSelinuxMode(): String
    /** Writes /sys/fs/selinux/enforce through the root shell. */
    suspend fun setGlobalSelinuxEnforcing(enforcing: Boolean): Boolean

    suspend fun getSulogStatus(): String
    suspend fun getSulogPersistValue(): Long?
    suspend fun getSulogValue(): Long?
    fun setSulogEnabled(enabled: Boolean): Boolean

    suspend fun getAdbRootStatus(): String
    suspend fun getAdbRootPersistValue(): Long?
    suspend fun getAdbRootValue(): Long?
    fun setAdbRootEnabled(enabled: Boolean): Boolean
    suspend fun getShellSuAdbOnlyStatus(): String
    suspend fun getShellSuAdbOnlyPersistValue(): Long?
    suspend fun getShellSuAdbOnlyValue(): Long?
    fun setShellSuAdbOnlyEnabled(enabled: Boolean): Boolean

    suspend fun getHideKallsymsStatus(): String
    suspend fun getHideKallsymsPersistValue(): Long?
    suspend fun getHideKallsymsValue(): Long?
    fun setHideKallsymsEnabled(enabled: Boolean): Boolean

    fun isDefaultUmountModules(): Boolean
    fun setDefaultUmountModules(enabled: Boolean): Boolean

    fun execKsudFeatureSave()
}
