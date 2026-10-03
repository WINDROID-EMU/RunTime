/**
 * MainActivity — Game Library / Launcher screen
 *
 * This is the entry point of the app. It displays a grid of detected games,
 * handles first-time setup (storage permissions, folder selection), and
 * provides access to settings.
 */
package com.rexglue.runtime

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.floatingactionbutton.FloatingActionButton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "ReXGlue.Main"
    }

    private lateinit var storageHelper: StorageHelper
    private lateinit var adapter: GameListAdapter
    private lateinit var recyclerView: RecyclerView
    private lateinit var emptyView: View
    private lateinit var swipeRefresh: SwipeRefreshLayout

    // SAF folder picker
    private val folderPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) {
            storageHelper.setGameFolderUri(uri)
            refreshGameList()
        }
    }

    // Storage permission request (for MANAGE_EXTERNAL_STORAGE)
    private val storagePermissionLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (storageHelper.hasStoragePermission()) {
            showFolderPicker()
        } else {
            Toast.makeText(this, R.string.storage_permission_denied, Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Verify native libraries loaded
        if (!RexGlueApplication.nativeLibrariesLoaded) {
            showFatalError(getString(R.string.error_native_libs))
            return
        }

        storageHelper = StorageHelper(this)

        // Setup toolbar
        val toolbar: MaterialToolbar = findViewById(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.title = getString(R.string.app_name)

        // Setup game list
        recyclerView = findViewById(R.id.game_list)
        emptyView = findViewById(R.id.empty_view)
        swipeRefresh = findViewById(R.id.swipe_refresh)

        adapter = GameListAdapter(
            onGameClick = { game -> launchGame(game) },
            onGameLongClick = { game -> showGameOptions(game); true }
        )

        recyclerView.layoutManager = GridLayoutManager(this, 3)
        recyclerView.adapter = adapter

        swipeRefresh.setOnRefreshListener { refreshGameList() }

        // Setup FAB for adding games folder
        val fab: FloatingActionButton = findViewById(R.id.fab_add_folder)
        fab.setOnClickListener { handleAddFolder() }

        // Show version in subtitle
        try {
            val version = NativeBridge.nativeGetVersion()
            supportActionBar?.subtitle = "SDK v$version"
        } catch (e: Exception) {
            Log.w(TAG, "Could not get native version: ${e.message}")
        }

        // Initialize native runtime
        initializeNativeRuntime()

        // First-time setup or scan games
        if (!storageHelper.hasGameFolder()) {
            showFirstTimeSetup()
        } else {
            refreshGameList()
        }
    }

    override fun onResume() {
        super.onResume()
        // Re-scan in case games were added while app was in background
        if (storageHelper.hasGameFolder()) {
            refreshGameList()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_settings -> {
                startActivity(Intent(this, SettingsActivity::class.java))
                true
            }
            R.id.action_refresh -> {
                refreshGameList()
                true
            }
            R.id.action_change_folder -> {
                handleAddFolder()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    // ================================================================
    // Initialization
    // ================================================================

    private fun initializeNativeRuntime() {
        try {
            val internalPath = filesDir.absolutePath
            val externalPath = getExternalFilesDir(null)?.absolutePath ?: internalPath
            val success = NativeBridge.nativeInit(internalPath, externalPath)
            if (!success) {
                Log.e(TAG, "Native runtime initialization failed")
                Toast.makeText(this, R.string.error_init_failed, Toast.LENGTH_LONG).show()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception during native init: ${e.message}", e)
        }
    }

    // ================================================================
    // Game List
    // ================================================================

    private fun refreshGameList() {
        swipeRefresh.isRefreshing = true
        lifecycleScope.launch {
            val games = withContext(Dispatchers.IO) {
                storageHelper.scanForGames()
            }
            adapter.submitList(games)
            emptyView.visibility = if (games.isEmpty()) View.VISIBLE else View.GONE
            recyclerView.visibility = if (games.isEmpty()) View.GONE else View.VISIBLE
            swipeRefresh.isRefreshing = false
        }
    }

    // ================================================================
    // Game Launch
    // ================================================================

    private fun launchGame(game: GameEntry) {
        Log.i(TAG, "Launching game: ${game.title} (${game.id})")

        val intent = Intent(this, GameActivity::class.java).apply {
            putExtra(GameActivity.EXTRA_GAME_ID, game.id)
            putExtra(GameActivity.EXTRA_GAME_TITLE, game.title)
            putExtra(GameActivity.EXTRA_MODULE_PATH, game.modulePath)
            putExtra(GameActivity.EXTRA_DATA_PATH, game.dataPath)
            putExtra(GameActivity.EXTRA_SAVE_PATH, storageHelper.getGameSaveDir(game.id).absolutePath)
        }
        startActivity(intent)
    }

    private fun showGameOptions(game: GameEntry) {
        AlertDialog.Builder(this)
            .setTitle(game.title)
            .setItems(arrayOf(
                getString(R.string.game_option_play),
                getString(R.string.game_option_settings),
                getString(R.string.game_option_delete_cache)
            )) { _, which ->
                when (which) {
                    0 -> launchGame(game)
                    1 -> { /* TODO: Per-game settings */ }
                    2 -> {
                        // Delete game-specific shader cache
                        val cacheDir = java.io.File(
                            storageHelper.getShaderCacheDir(), game.id
                        )
                        cacheDir.deleteRecursively()
                        Toast.makeText(this, R.string.cache_cleared, Toast.LENGTH_SHORT).show()
                    }
                }
            }
            .show()
    }

    // ================================================================
    // Folder Selection
    // ================================================================

    private fun handleAddFolder() {
        if (!storageHelper.hasStoragePermission()) {
            val intent = storageHelper.requestStoragePermission()
            if (intent != null) {
                storagePermissionLauncher.launch(intent)
            } else {
                showFolderPicker()
            }
        } else {
            showFolderPicker()
        }
    }

    private fun showFolderPicker() {
        folderPickerLauncher.launch(null)
    }

    private fun showFirstTimeSetup() {
        AlertDialog.Builder(this)
            .setTitle(R.string.welcome_title)
            .setMessage(R.string.welcome_message)
            .setPositiveButton(R.string.select_folder) { _, _ ->
                handleAddFolder()
            }
            .setCancelable(false)
            .show()
    }

    private fun showFatalError(message: String) {
        AlertDialog.Builder(this)
            .setTitle(R.string.error_title)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok) { _, _ -> finish() }
            .setCancelable(false)
            .show()
    }

    override fun onDestroy() {
        super.onDestroy()
        // Don't shutdown native here — it persists across activity transitions
    }
}
