/**
 * StorageHelper — File management and game scanner for Android Scoped Storage
 */
package com.rexglue.runtime

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import android.util.Log
import androidx.activity.result.ActivityResultLauncher
import androidx.documentfile.provider.DocumentFile
import java.io.File

/**
 * Handles file system access, storage permission management, and game scanning.
 *
 * On Android 11+ (API 30+), uses Scoped Storage with the Storage Access Framework (SAF)
 * or MANAGE_EXTERNAL_STORAGE permission for broad file access.
 */
class StorageHelper(private val context: Context) {

    companion object {
        private const val TAG = "ReXGlue.Storage"
        private const val PREFS_NAME = "rexglue_storage"
        private const val KEY_GAME_FOLDER_URI = "game_folder_uri"
        private const val KEY_GAME_FOLDER_PATH = "game_folder_path"

        /** Standard subdirectories inside the app's internal storage */
        const val DIR_SAVES = "saves"
        const val DIR_SHADER_CACHE = "cache/shaders"
        const val DIR_CONFIG = "config"
        const val DIR_LOGS = "logs"
    }

    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    // ================================================================
    // Storage Permission
    // ================================================================

    /**
     * Check if we have sufficient storage access to read game files.
     */
    fun hasStoragePermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            // On API 28-29, READ_EXTERNAL_STORAGE is sufficient
            true // Assume granted; check at runtime via ActivityCompat
        }
    }

    /**
     * Request broad storage access (MANAGE_EXTERNAL_STORAGE).
     * On Android 11+, this opens the system settings page.
     */
    fun requestStoragePermission(): Intent? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                data = Uri.parse("package:${context.packageName}")
            }
        } else {
            null // Use runtime permission request for older APIs
        }
    }

    // ================================================================
    // Game Folder Management
    // ================================================================

    /**
     * Save the selected game folder URI (from SAF document tree picker).
     */
    fun setGameFolderUri(uri: Uri) {
        // Persist permission across reboots
        val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        context.contentResolver.takePersistableUriPermission(uri, flags)

        prefs.edit()
            .putString(KEY_GAME_FOLDER_URI, uri.toString())
            .apply()
        Log.i(TAG, "Game folder set: $uri")
    }

    /**
     * Save a direct filesystem path as game folder (used with MANAGE_EXTERNAL_STORAGE).
     */
    fun setGameFolderPath(path: String) {
        prefs.edit()
            .putString(KEY_GAME_FOLDER_PATH, path)
            .apply()
        Log.i(TAG, "Game folder path set: $path")
    }

    /** Get the stored game folder URI, or null if not set */
    fun getGameFolderUri(): Uri? {
        return prefs.getString(KEY_GAME_FOLDER_URI, null)?.let { Uri.parse(it) }
    }

    /** Get the stored game folder path, or null if not set */
    fun getGameFolderPath(): String? {
        return prefs.getString(KEY_GAME_FOLDER_PATH, null)
    }

    /** Check if a game folder has been selected */
    fun hasGameFolder(): Boolean {
        return getGameFolderUri() != null || getGameFolderPath() != null
    }

    // ================================================================
    // Internal App Directories
    // ================================================================

    /** Get or create app-internal save data directory */
    fun getSavesDir(): File {
        return File(context.filesDir, DIR_SAVES).also { it.mkdirs() }
    }

    /** Get or create per-game save directory */
    fun getGameSaveDir(gameId: String): File {
        return File(getSavesDir(), gameId).also { it.mkdirs() }
    }

    /** Get or create shader cache directory */
    fun getShaderCacheDir(): File {
        val cacheBase = context.externalCacheDir ?: context.cacheDir
        return File(cacheBase, DIR_SHADER_CACHE).also { it.mkdirs() }
    }

    /** Get or create config directory */
    fun getConfigDir(): File {
        return File(context.filesDir, DIR_CONFIG).also { it.mkdirs() }
    }

    /** Get or create per-game config file path */
    fun getGameConfigFile(gameId: String): File {
        return File(getConfigDir(), "$gameId.toml")
    }

    /** Get or create logs directory */
    fun getLogsDir(): File {
        return File(context.filesDir, DIR_LOGS).also { it.mkdirs() }
    }

    // ================================================================
    // Game Scanner
    // ================================================================

    /**
     * Scan the game folder for recompiled game modules.
     *
     * Each game is expected to be in a subdirectory containing:
     * - A .so module file (the recompiled executable)
     * - Optional: manifest.toml with game metadata
     * - Optional: cover.png or cover.jpg
     *
     * Expected structure:
     *   games/
     *   ├── my_game/
     *   │   ├── libmy_game.so
     *   │   ├── manifest.toml
     *   │   ├── cover.png
     *   │   └── data/
     *   │       └── (game data files)
     *   └── another_game/
     *       └── ...
     */
    fun scanForGames(): List<GameEntry> {
        val games = mutableListOf<GameEntry>()

        // Try direct path first (MANAGE_EXTERNAL_STORAGE)
        val directPath = getGameFolderPath()
        if (directPath != null) {
            val folder = File(directPath)
            if (folder.isDirectory) {
                games.addAll(scanDirectory(folder))
            }
        }

        // Try SAF URI
        val uri = getGameFolderUri()
        if (uri != null && games.isEmpty()) {
            val docFile = DocumentFile.fromTreeUri(context, uri)
            if (docFile != null && docFile.isDirectory) {
                games.addAll(scanDocumentTree(docFile))
            }
        }

        Log.i(TAG, "Found ${games.size} game(s)")
        return games.sortedBy { it.title.lowercase() }
    }

    /**
     * Scan a regular directory for game modules.
     */
    private fun scanDirectory(dir: File): List<GameEntry> {
        val games = mutableListOf<GameEntry>()

        dir.listFiles()?.filter { it.isDirectory }?.forEach { gameDir ->
            val soFiles = gameDir.listFiles()?.filter { f ->
                f.isFile && f.name.endsWith(".so") && f.name.startsWith("lib")
            } ?: emptyList()

            val moduleSo = soFiles.firstOrNull() ?: return@forEach
            val gameId = gameDir.name
            val title = parseGameTitle(gameDir, gameId)
            val coverArt = findCoverArt(gameDir)

            games.add(
                GameEntry(
                    id = gameId,
                    title = title,
                    modulePath = moduleSo.absolutePath,
                    dataPath = gameDir.absolutePath,
                    coverArtUri = coverArt
                )
            )
        }

        return games
    }

    /**
     * Scan a SAF document tree for game modules.
     */
    private fun scanDocumentTree(root: DocumentFile): List<GameEntry> {
        val games = mutableListOf<GameEntry>()

        root.listFiles().filter { it.isDirectory }.forEach { gameDir ->
            val soFile = gameDir.listFiles().firstOrNull { f ->
                f.isFile && (f.name?.endsWith(".so") == true) && (f.name?.startsWith("lib") == true)
            } ?: return@forEach

            val gameId = gameDir.name ?: return@forEach
            val title = gameId.replace("_", " ")
                .replaceFirstChar { c -> c.uppercase() }

            games.add(
                GameEntry(
                    id = gameId,
                    title = title,
                    modulePath = soFile.uri.toString(),
                    dataPath = gameDir.uri.toString()
                )
            )
        }

        return games
    }

    /**
     * Try to read game title from manifest.toml, fall back to directory name.
     */
    private fun parseGameTitle(gameDir: File, fallback: String): String {
        val manifest = File(gameDir, "manifest.toml")
        if (manifest.exists()) {
            try {
                manifest.readLines().forEach { line ->
                    val match = Regex("""^title\s*=\s*"(.+)"\s*$""").find(line.trim())
                    if (match != null) {
                        return match.groupValues[1]
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to parse manifest for $fallback: ${e.message}")
            }
        }
        return fallback.replace("_", " ").replaceFirstChar { it.uppercase() }
    }

    /**
     * Find cover art image in game directory.
     */
    private fun findCoverArt(gameDir: File): Uri? {
        val candidates = listOf("cover.png", "cover.jpg", "cover.webp", "icon.png")
        for (name in candidates) {
            val file = File(gameDir, name)
            if (file.exists()) {
                return Uri.fromFile(file)
            }
        }
        return null
    }

    // ================================================================
    // Cache Management
    // ================================================================

    /** Get the total size of the shader cache in bytes */
    fun getShaderCacheSize(): Long {
        return getShaderCacheDir().walkTopDown()
            .filter { it.isFile }
            .sumOf { it.length() }
    }

    /** Clear the shader cache */
    fun clearShaderCache() {
        getShaderCacheDir().deleteRecursively()
        getShaderCacheDir().mkdirs()
        Log.i(TAG, "Shader cache cleared")
    }

    /** Format bytes into a human-readable string */
    fun formatSize(bytes: Long): String {
        return when {
            bytes < 1024 -> "$bytes B"
            bytes < 1024 * 1024 -> "${bytes / 1024} KB"
            bytes < 1024 * 1024 * 1024 -> "${"%.1f".format(bytes / (1024.0 * 1024.0))} MB"
            else -> "${"%.2f".format(bytes / (1024.0 * 1024.0 * 1024.0))} GB"
        }
    }
}
