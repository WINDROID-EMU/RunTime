/**
 * SettingsActivity — App and per-game configuration
 */
package com.rexglue.runtime

import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import com.google.android.material.appbar.MaterialToolbar

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        val toolbar: MaterialToolbar = findViewById(R.id.settings_toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = getString(R.string.settings_title)

        toolbar.setNavigationOnClickListener { onBackPressedDispatcher.onBackPressed() }

        if (savedInstanceState == null) {
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings_container, SettingsFragment())
                .commit()
        }
    }

    /**
     * Main settings fragment using AndroidX Preference.
     *
     * Categories:
     * - Graphics: resolution, V-Sync, FidelityFX
     * - Audio: volume, latency mode
     * - Controls: touch overlay visibility, opacity, gamepad mapping
     * - System: game folder, shader cache, logs
     */
    class SettingsFragment : PreferenceFragmentCompat() {

        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            val context = preferenceManager.context
            val screen = preferenceManager.createPreferenceScreen(context)

            // ========================================================
            // Graphics Category
            // ========================================================
            val graphicsCategory = androidx.preference.PreferenceCategory(context).apply {
                key = "cat_graphics"
                title = getString(R.string.settings_cat_graphics)
            }
            screen.addPreference(graphicsCategory)

            graphicsCategory.addPreference(ListPreference(context).apply {
                key = "gfx_resolution"
                title = getString(R.string.settings_resolution)
                summary = getString(R.string.settings_resolution_summary)
                entries = arrayOf("Native (1280x720)", "2x (2560x1440)", "0.75x (960x540)", "0.5x (640x360)")
                entryValues = arrayOf("1.0", "2.0", "0.75", "0.5")
                setDefaultValue("1.0")
            })

            graphicsCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "gfx_vsync"
                title = getString(R.string.settings_vsync)
                summary = getString(R.string.settings_vsync_summary)
                setDefaultValue(true)
            })

            graphicsCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "gfx_fidelityfx"
                title = getString(R.string.settings_fidelityfx)
                summary = getString(R.string.settings_fidelityfx_summary)
                setDefaultValue(false)
            })

            graphicsCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "gfx_fps_counter"
                title = getString(R.string.settings_fps_counter)
                summary = getString(R.string.settings_fps_counter_summary)
                setDefaultValue(true)
            })

            // ========================================================
            // Audio Category
            // ========================================================
            val audioCategory = androidx.preference.PreferenceCategory(context).apply {
                key = "cat_audio"
                title = getString(R.string.settings_cat_audio)
            }
            screen.addPreference(audioCategory)

            audioCategory.addPreference(ListPreference(context).apply {
                key = "audio_latency"
                title = getString(R.string.settings_audio_latency)
                summary = getString(R.string.settings_audio_latency_summary)
                entries = arrayOf("Low Latency", "Normal", "Power Saving")
                entryValues = arrayOf("low", "normal", "power_saving")
                setDefaultValue("normal")
            })

            audioCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "audio_enabled"
                title = getString(R.string.settings_audio_enabled)
                setDefaultValue(true)
            })

            // ========================================================
            // Controls Category
            // ========================================================
            val controlsCategory = androidx.preference.PreferenceCategory(context).apply {
                key = "cat_controls"
                title = getString(R.string.settings_cat_controls)
            }
            screen.addPreference(controlsCategory)

            controlsCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "ctrl_show_touch"
                title = getString(R.string.settings_show_touch)
                summary = getString(R.string.settings_show_touch_summary)
                setDefaultValue(true)
            })

            controlsCategory.addPreference(ListPreference(context).apply {
                key = "ctrl_opacity"
                title = getString(R.string.settings_ctrl_opacity)
                entries = arrayOf("25%", "50%", "75%", "100%")
                entryValues = arrayOf("0.25", "0.50", "0.75", "1.0")
                setDefaultValue("0.50")
            })

            controlsCategory.addPreference(SwitchPreferenceCompat(context).apply {
                key = "ctrl_haptic"
                title = getString(R.string.settings_haptic)
                summary = getString(R.string.settings_haptic_summary)
                setDefaultValue(true)
            })

            // ========================================================
            // System Category
            // ========================================================
            val systemCategory = androidx.preference.PreferenceCategory(context).apply {
                key = "cat_system"
                title = getString(R.string.settings_cat_system)
            }
            screen.addPreference(systemCategory)

            systemCategory.addPreference(Preference(context).apply {
                key = "sys_game_folder"
                title = getString(R.string.settings_game_folder)
                val storageHelper = StorageHelper(context)
                summary = storageHelper.getGameFolderPath()
                    ?: storageHelper.getGameFolderUri()?.toString()
                    ?: getString(R.string.settings_game_folder_not_set)
            })

            systemCategory.addPreference(Preference(context).apply {
                key = "sys_clear_shaders"
                title = getString(R.string.settings_clear_shaders)
                val storageHelper = StorageHelper(context)
                val size = storageHelper.formatSize(storageHelper.getShaderCacheSize())
                summary = getString(R.string.settings_clear_shaders_summary, size)
                setOnPreferenceClickListener {
                    storageHelper.clearShaderCache()
                    this.summary = getString(R.string.settings_clear_shaders_summary,
                        storageHelper.formatSize(0))
                    Toast.makeText(context, R.string.cache_cleared, Toast.LENGTH_SHORT).show()
                    true
                }
            })

            systemCategory.addPreference(Preference(context).apply {
                key = "sys_version"
                title = getString(R.string.settings_version)
                try {
                    summary = "SDK: ${NativeBridge.nativeGetVersion()}"
                } catch (_: Exception) {
                    summary = "SDK: unavailable"
                }
                isSelectable = false
            })

            preferenceScreen = screen
        }
    }
}
