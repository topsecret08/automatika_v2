package com.automatika.autopilot.data

import org.json.JSONObject
import java.io.InputStream
import java.security.MessageDigest

/** Файл релиза GitHub: имя, прямая ссылка (только HTTPS) и размер. */
data class ReleaseAsset(val name: String, val url: String, val size: Long)

/** Нужные для обновления файлы последнего релиза. */
data class ReleaseInfo(val tag: String, val apk: ReleaseAsset, val sums: ReleaseAsset)

/**
 * Чистая логика обновления приложения из GitHub Releases: разбор ответа API,
 * сравнение версий и проверка SHA-256. Без сети и Android, поэтому покрыта юнит-тестами.
 */
object UpdateLogic {
    const val APK_PREFIX = "Autopilot-"
    const val APK_SUFFIX = ".apk"
    const val SUMS_NAME = "SHA256SUMS-apk.txt"

    fun parseRelease(json: String): ReleaseInfo {
        val root = JSONObject(json)
        val tag = root.optString("tag_name")
        require(tag.isNotBlank()) { "В ответе нет tag_name" }
        val assets = root.optJSONArray("assets") ?: throw IllegalStateException("В релизе нет файлов")

        var apk: ReleaseAsset? = null
        var sums: ReleaseAsset? = null
        for (i in 0 until assets.length()) {
            val item = assets.getJSONObject(i)
            val name = item.optString("name")
            val asset = ReleaseAsset(name, item.optString("browser_download_url"), item.optLong("size"))
            when {
                name == SUMS_NAME -> sums = asset
                name.startsWith(APK_PREFIX) && name.endsWith(APK_SUFFIX) -> apk = asset
            }
        }
        val apkAsset = requireNotNull(apk) { "В релизе нет $APK_PREFIX*$APK_SUFFIX" }
        val sumsAsset = requireNotNull(sums) { "В релизе нет $SUMS_NAME" }
        require(apkAsset.url.startsWith("https://") && sumsAsset.url.startsWith("https://")) {
            "Ссылки на файлы релиза должны быть HTTPS"
        }
        return ReleaseInfo(tag, apkAsset, sumsAsset)
    }

    /** Новее ли тег релиза (v0.9.7) установленной версии (0.9.6). Суффиксы вроде -beta игнорируются. */
    fun isNewer(tag: String, current: String): Boolean {
        val a = numbers(tag)
        val b = numbers(current)
        for (i in 0 until maxOf(a.size, b.size)) {
            val x = a.getOrElse(i) { 0 }
            val y = b.getOrElse(i) { 0 }
            if (x != y) return x > y
        }
        return false
    }

    private fun numbers(version: String): List<Int> = version.trim()
        .removePrefix("v")
        .substringBefore('-')
        .split('.')
        .map { it.toIntOrNull() ?: 0 }

    /** SHA-256 файла name из SHA256SUMS (формат sha256sum: «<hex>  <имя>»), в нижнем регистре. */
    fun expectedSha256(sums: String, name: String): String? = sums.lineSequence()
        .map { it.trim().split(Regex("\\s+"), limit = 2) }
        .firstOrNull { it.size == 2 && it[1].removePrefix("*") == name }
        ?.get(0)
        ?.lowercase()

    fun sha256Hex(input: InputStream): String {
        val digest = MessageDigest.getInstance("SHA-256")
        input.use { stream ->
            val buf = ByteArray(64 * 1024)
            while (true) {
                val n = stream.read(buf)
                if (n < 0) break
                digest.update(buf, 0, n)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}
