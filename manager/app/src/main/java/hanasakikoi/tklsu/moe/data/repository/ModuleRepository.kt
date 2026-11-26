package hanasakikoi.tklsu.moe.data.repository

import hanasakikoi.tklsu.moe.data.model.Module

interface ModuleRepository {
    suspend fun getModules(): Result<List<Module>>
}
