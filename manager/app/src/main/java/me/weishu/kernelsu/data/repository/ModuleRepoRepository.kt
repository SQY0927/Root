package me.weishu.kernelsu.data.repository

import me.weishu.kernelsu.data.model.RepoModuleDetail
import me.weishu.kernelsu.data.model.RepoModuleList

interface ModuleRepoRepository {
    /** Index + module list; falls back to the last complete cached copy on failure. */
    suspend fun fetchModules(): Result<RepoModuleList>

    /**
     * Detail (and readme) of a module in the current index. Fails with
     * [me.weishu.kernelsu.data.registry.ModuleNotInIndexException] if it is not listed.
     */
    suspend fun fetchDetail(moduleId: String): Result<RepoModuleDetail>
}
