package com.automatika.autopilot.data

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.content.FileProvider
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * Сеть и установка обновления приложения из GitHub Releases.
 * Скачивание идёт по HTTPS, файл проверяется по SHA-256 из SHA256SUMS-apk.txt
 * того же релиза, и только после этого открывается системный установщик.
 */
class AppUpdater(private val context: Context) {

    private val latestUrl = "https://api.github.com/repos/topsecret08/automatika_v2/releases/latest"

    fun fetchLatest(): ReleaseInfo = UpdateLogic.parseRelease(getText(latestUrl))

    /** Скачивает APK в кэш и возвращает файл, только если SHA-256 совпал. */
    fun downloadVerified(info: ReleaseInfo, onProgress: (Int) -> Unit): File {
        val expected = UpdateLogic.expectedSha256(getText(info.sums.url), info.apk.name)
            ?: throw SecurityException("В ${UpdateLogic.SUMS_NAME} нет строки для ${info.apk.name}")

        val dir = File(context.cacheDir, "update").apply { mkdirs() }
        val part = File(dir, info.apk.name + ".part")
        val target = File(dir, info.apk.name)
        try {
            download(info.apk.url, part, onProgress)
            val actual = UpdateLogic.sha256Hex(part.inputStream())
            if (!actual.equals(expected, ignoreCase = true)) {
                throw SecurityException("SHA-256 не совпал, установка отменена")
            }
            if (target.exists()) target.delete()
            if (!part.renameTo(target)) throw IOException("Не удалось сохранить APK")
            return target
        } finally {
            part.delete()
        }
    }

    /** true, если системе разрешена установка из этого приложения. */
    fun canInstall(): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.O || context.packageManager.canRequestPackageInstalls()

    /** Открывает экран разрешения «установка из неизвестных источников» для этого приложения. */
    fun openInstallPermission() {
        val intent = Intent(
            Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
            Uri.parse("package:${context.packageName}"),
        ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(intent)
    }

    /** Передаёт APK системному установщику. Подтверждение пользователя обязательно. */
    fun install(file: File) {
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", file)
        val intent = Intent(Intent.ACTION_INSTALL_PACKAGE).apply {
            data = uri
            flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK
        }
        context.startActivity(intent)
    }

    private fun getText(url: String): String {
        requireHttps(url)
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = 10_000
        connection.readTimeout = 15_000
        connection.setRequestProperty("Accept", "application/vnd.github+json")
        connection.setRequestProperty("User-Agent", "Automatika-Autopilot")
        try {
            val code = connection.responseCode
            check(code in 200..299) { "GitHub HTTP $code" }
            requireHttps(connection.url.toString())
            return connection.inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }
        } finally {
            connection.disconnect()
        }
    }

    private fun download(url: String, dest: File, onProgress: (Int) -> Unit) {
        requireHttps(url)
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = 10_000
        connection.readTimeout = 30_000
        connection.instanceFollowRedirects = true
        connection.setRequestProperty("User-Agent", "Automatika-Autopilot")
        try {
            val code = connection.responseCode
            check(code in 200..299) { "APK HTTP $code" }
            requireHttps(connection.url.toString())

            val total = connection.contentLengthLong
            connection.inputStream.use { input ->
                FileOutputStream(dest).use { out ->
                    val buf = ByteArray(64 * 1024)
                    var done = 0L
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        done += n
                        if (total > 0) onProgress((done * 100 / total).toInt())
                    }
                }
            }
            onProgress(100)
        } finally {
            connection.disconnect()
        }
    }

    private fun requireHttps(url: String) {
        require(url.startsWith("https://", ignoreCase = true)) { "Разрешён только HTTPS" }
    }
}
