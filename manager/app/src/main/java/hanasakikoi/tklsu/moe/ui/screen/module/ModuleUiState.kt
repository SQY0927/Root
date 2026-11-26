package hanasakikoi.tklsu.moe.ui.screen.module

import android.net.Uri
import androidx.compose.runtime.Immutable
import hanasakikoi.tklsu.moe.data.model.Module
import hanasakikoi.tklsu.moe.ui.component.SearchStatus

sealed interface ModuleConfirmRequest {
    data class Uninstall(
        val module: Module,
    ) : ModuleConfirmRequest
}

@Immutable
data class ModuleConfirmDialogState(
    val request: ModuleConfirmRequest,
    val title: String,
    val content: String? = null,
    val markdown: Boolean = false,
    val html: Boolean = false,
    /** Registry-hosted content: render with JavaScript off and network locked to the registry. */
    val restricted: Boolean = false,
    val confirm: String? = null,
    val dismiss: String? = null,
)

sealed interface ModuleEffect {
    data class Toast(
        val message: String,
    ) : ModuleEffect

    data class SnackBar(
        val message: String,
    ) : ModuleEffect
}

@Immutable
data class ModuleUiState(
    val isRefreshing: Boolean = false,
    val hasLoaded: Boolean = false,
    val modules: List<Module> = emptyList(),
    val moduleList: List<Module> = emptyList(),
    val searchStatus: SearchStatus = SearchStatus(""),
    val searchResults: List<Module> = emptyList(),
    val sortEnabledFirst: Boolean = false,
    val sortActionFirst: Boolean = false,
    val isSafeMode: Boolean = false,
    val magiskInstalled: Boolean = false,
    val confirmDialogState: ModuleConfirmDialogState? = null,
) {
    val installButtonVisible: Boolean
        get() = !(isSafeMode || magiskInstalled)
}

@Immutable
data class ModuleActions(
    val onRefresh: () -> Unit,
    val onSearchStatusChange: (SearchStatus) -> Unit,
    val onSearchTextChange: (String) -> Unit,
    val onClearSearch: () -> Unit,
    val onRequestUninstallConfirmation: (Module) -> Unit,
    val onDismissConfirmRequest: () -> Unit,
    val onOpenRepo: () -> Unit,
    val onToggleSortActionFirst: () -> Unit,
    val onToggleSortEnabledFirst: () -> Unit,
    val onOpenWebUi: (Module) -> Unit,
    val onToggleModule: (Module) -> Unit,
    val onUninstallModule: (Module) -> Unit,
    val onUndoUninstallModule: (Module) -> Unit,
    val onOpenFlash: (List<Uri>) -> Unit,
    val onExecuteModuleAction: (Module) -> Unit,
)
