package me.weishu.kernelsu.data.repository

import me.weishu.kernelsu.data.model.Module

interface ModuleRepository {
    suspend fun getModules(): Result<List<Module>>
}
