package hanasakikoi.tklsu.moe.data.repository

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import hanasakikoi.tklsu.moe.data.model.Author
import hanasakikoi.tklsu.moe.data.model.RepoModule
import hanasakikoi.tklsu.moe.data.model.RepoModuleDetail
import hanasakikoi.tklsu.moe.data.model.RepoModuleList
import hanasakikoi.tklsu.moe.data.model.RepoRelease
import hanasakikoi.tklsu.moe.data.registry.ApiAuthor
import hanasakikoi.tklsu.moe.data.registry.ApiModuleDetail
import hanasakikoi.tklsu.moe.data.registry.ApiModuleSummary
import hanasakikoi.tklsu.moe.data.registry.ApiVersionRef
import hanasakikoi.tklsu.moe.data.registry.FileRegistryCache
import hanasakikoi.tklsu.moe.data.registry.RegistryClient
import hanasakikoi.tklsu.moe.data.registry.RegistryFetcher
import hanasakikoi.tklsu.moe.data.registry.RegistryKind
import hanasakikoi.tklsu.moe.data.registry.RegistryLocale
import hanasakikoi.tklsu.moe.data.registry.RegistryUrls
import hanasakikoi.tklsu.moe.ksuApp
import okhttp3.CacheControl
import okhttp3.Request
import java.io.File
import java.io.IOException
import java.time.OffsetDateTime
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle
import java.util.Locale

class ModuleRepoRepositoryImpl : ModuleRepoRepository {

    companion object {
        private const val TAG = "ModuleRepoRepository"

        /** Site root of KernelSU Modules; every API path is resolved against it. */
        const val REGISTRY_BASE_URL = RegistryUrls.DEFAULT_BASE_URL

        /** Upper bound for a single JSON / HTML document. */
        private const val MAX_DOCUMENT_BYTES = 32L * 1024 * 1024

        // One client per process so the list and detail screens share the same catalog.
        private val client: RegistryClient by lazy {
            RegistryClient(
                baseUrl = REGISTRY_BASE_URL,
                fetcher = OkHttpRegistryFetcher,
                cache = FileRegistryCache(File(ksuApp.filesDir, "module_registry")),
            )
        }

        private fun requestedLocale(): String = Locale.getDefault().toLanguageTag()
    }

    private object OkHttpRegistryFetcher : RegistryFetcher {
        override fun fetch(url: String): String {
            val request = Request.Builder()
                .url(url)
                // Revalidate so index, list and detail are never served from different cache ages.
                .cacheControl(CacheControl.Builder().noCache().build())
                .build()
            ksuApp.okhttpClient.newCall(request).execute().use { response ->
                if (!response.isSuccessful) throw IOException("HTTP ${response.code} for $url")
                val body = response.body
                if (body.contentLength() > MAX_DOCUMENT_BYTES) throw IOException("Document too large: $url")
                val source = body.source()
                if (source.request(MAX_DOCUMENT_BYTES + 1)) throw IOException("Document too large: $url")
                return source.readUtf8()
            }
        }
    }

    override suspend fun fetchModules(): Result<RepoModuleList> = withContext(Dispatchers.IO) {
        runCatching {
            val load = client.loadCatalog()
            load.error?.let { Log.w(TAG, "registry fetch failed, using last good copy", it) }
            RepoModuleList(
                modules = load.catalog.modules.map { it.toRepoModule() },
                buildId = load.catalog.buildId,
                stale = load.error != null || load.catalog.fromCache,
                error = load.error,
            )
        }
    }

    override suspend fun fetchDetail(moduleId: String): Result<RepoModuleDetail> = withContext(Dispatchers.IO) {
        runCatching {
            val load = client.loadDetail(moduleId)
            load.error?.let { Log.w(TAG, "detail fetch failed, using last good copy", it) }
            val readmeResult = runCatching { client.loadReadme(load.detail) }
                .onFailure { Log.w(TAG, "readme fetch failed", it) }
            val readme = readmeResult.getOrNull()
            load.detail.toRepoModuleDetail(
                siteLocale = RegistryLocale.siteLocale(requestedLocale()),
                readmeHtml = readme?.html,
                readmeFailed = readmeResult.isFailure,
                fromCache = load.fromCache,
            )
        }
    }

    private fun formatDate(iso: String): Pair<String, Long> = runCatching {
        val time = OffsetDateTime.parse(iso)
        val text = time.atZoneSameInstant(ZoneId.systemDefault())
            .format(DateTimeFormatter.ofLocalizedDate(FormatStyle.MEDIUM))
        text to time.toInstant().toEpochMilli()
    }.getOrDefault(iso to 0L)

    private fun ApiModuleSummary.toRepoModule(): RepoModule =
        buildRepoModule(id, name, summary, authors, tags, metamodule, kind, icon, stable, stars, starsRepository)

    private fun buildRepoModule(
        id: String,
        name: String,
        summary: String,
        authors: List<ApiAuthor>,
        tags: List<String>,
        metamodule: Boolean,
        kind: String,
        icon: String?,
        stable: ApiVersionRef,
        stars: Int?,
        starsRepository: String?,
    ): RepoModule {
        val authorList = authors
            .filter { it.name.isNotBlank() }
            .map { Author(name = it.name.trim(), link = it.url.orEmpty()) }
        val (date, epoch) = formatDate(stable.publishedAt)
        return RepoModule(
            moduleId = id,
            moduleName = name.trim().ifEmpty { id },
            authors = authorList.joinToString(", ") { it.name },
            authorList = authorList,
            summary = summary.trim(),
            tags = tags,
            metamodule = metamodule,
            isScript = kind == RegistryKind.SCRIPT,
            iconUrl = icon?.let { client.resolve(it) },
            latestVersion = stable.version,
            latestVersionCode = stable.versionCode,
            latestReleaseTime = date,
            latestReleaseEpoch = epoch,
            stars = stars,
            starsRepository = starsRepository,
        )
    }

    private fun ApiModuleDetail.toRepoModuleDetail(
        siteLocale: String,
        readmeHtml: String?,
        readmeFailed: Boolean,
        fromCache: Boolean,
    ): RepoModuleDetail {
        val module = buildRepoModule(id, name, summary, authors, tags, metamodule, kind, icon, stable, stars, starsRepository)
        return RepoModuleDetail(
            module = module,
            license = license,
            homepage = homepage,
            support = support,
            sourceFullName = source.fullName,
            sourceUrl = source.url,
            sourceCommit = source.commit,
            readmeHtml = readmeHtml,
            readmeFailed = readmeFailed,
            readmeBaseUrl = client.baseUrl,
            // Prereleases are recorded by the registry but never offered for installation.
            releases = releases.filter { !it.prerelease }.map { r ->
                RepoRelease(
                    tag = r.tag,
                    name = r.name?.takeIf { it.isNotBlank() } ?: r.tag,
                    version = r.version,
                    versionCode = r.versionCode,
                    publishedAt = formatDate(r.publishedAt).first,
                    changelogHtml = r.changelogHtml?.takeIf { it.isNotBlank() },
                    zipUrl = r.zip.url,
                    zipName = r.zip.name.substringAfterLast('/').ifBlank { "$id-${r.versionCode}.zip" },
                    zipSize = r.zip.size,
                    sha256 = r.zip.sha256,
                    tagCommit = r.tagCommit,
                    releaseUrl = r.releaseUrl,
                    attestationUrl = r.attestationUrl,
                    immutable = r.immutable,
                )
            },
            healthStale = health?.status == "stale",
            // The public site only has UI locales, not the module's content locales.
            webUrl = "${client.baseUrl}$siteLocale/modules/$id/",
            fromCache = fromCache,
        )
    }
}
