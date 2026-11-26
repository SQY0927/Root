package hanasakikoi.tklsu.moe.data.repository

import hanasakikoi.tklsu.moe.data.model.RepoModuleDetail
import hanasakikoi.tklsu.moe.data.model.RepoModuleList

interface ModuleRepoRepository {
    /** Index + module list; falls back to the last complete cached copy on failure. */
    suspend fun fetchModules(): Result<RepoModuleList>

    /**
     * Detail (and readme) of a module in the current index. Fails with
     * [hanasakikoi.tklsu.moe.data.registry.ModuleNotInIndexException] if it is not listed.
     */
    suspend fun fetchDetail(moduleId: String): Result<RepoModuleDetail>
}
