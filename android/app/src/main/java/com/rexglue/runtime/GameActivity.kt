/**
 * GameActivity — Full-screen game rendering and input activity
 *
 * This activity manages:
 * - Immersive fullscreen mode
 * - Vulkan rendering surface (GameSurfaceView)
 * - Virtual touch controller overlay
 * - Physical gamepad input forwarding
 * - Game lifecycle (load module, pause, resume, stop)
 */
package com.rexglue.runtime

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class GameActivity : AppCompatActivity(), GameSurfaceView.SurfaceListener {

    companion object {
        private const val TAG = "ReXGlue.Game"

        // Intent extras
        const val EXTRA_GAME_ID = "game_id"
        const val EXTRA_GAME_TITLE = "game_title"
        const val EXTRA_MODULE_PATH = "module_path"
        const val EXTRA_DATA_PATH = "data_path"
        const val EXTRA_SAVE_PATH = "save_path"
    }

    private lateinit var gameSurface: GameSurfaceView
    private lateinit var controllerOverlay: VirtualControllerView
    private lateinit var fpsCounter: TextView
    private lateinit var rootLayout: FrameLayout

    private var gameId: String = ""
    private var gameTitle: String = ""
    private var modulePath: String = ""
    private var dataPath: String = ""
    private var savePath: String = ""

    private var moduleLoaded = false

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_game)

        // Extract intent extras
        gameId = intent.getStringExtra(EXTRA_GAME_ID) ?: ""
        gameTitle = intent.getStringExtra(EXTRA_GAME_TITLE) ?: ""
        modulePath = intent.getStringExtra(EXTRA_MODULE_PATH) ?: ""
        dataPath = intent.getStringExtra(EXTRA_DATA_PATH) ?: ""
        savePath = intent.getStringExtra(EXTRA_SAVE_PATH) ?: ""

        if (modulePath.isEmpty()) {
            Toast.makeText(this, R.string.error_no_module, Toast.LENGTH_LONG).show()
            finish()
            return
        }

        Log.i(TAG, "Starting game: $gameTitle ($gameId)")
        Log.i(TAG, "  Module: $modulePath")
        Log.i(TAG, "  Data: $dataPath")
        Log.i(TAG, "  Save: $savePath")

        // Setup views
        rootLayout = findViewById(R.id.game_root)
        gameSurface = findViewById(R.id.game_surface)
        controllerOverlay = findViewById(R.id.controller_overlay)
        fpsCounter = findViewById(R.id.fps_counter)

        // Configure surface
        gameSurface.surfaceListener = this

        // Keep screen on
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Enable immersive mode
        enableImmersiveMode()

        // Start FPS counter update loop
        startFpsCounter()
    }

    override fun onResume() {
        super.onResume()
        enableImmersiveMode()

        if (moduleLoaded) {
            NativeBridge.nativeResume()
        }
    }

    override fun onPause() {
        super.onPause()
        if (moduleLoaded) {
            NativeBridge.nativePause()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (moduleLoaded) {
            NativeBridge.nativePause()
        }
        NativeBridge.destroySurface()
        moduleLoaded = false
    }

    // ================================================================
    // Immersive Fullscreen
    // ================================================================

    private fun enableImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.let { controller ->
                controller.hide(
                    WindowInsets.Type.statusBars() or
                    WindowInsets.Type.navigationBars()
                )
                controller.systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            )
        }
    }

    // ================================================================
    // Surface Lifecycle
    // ================================================================

    override fun onSurfaceReady() {
        Log.i(TAG, "Surface ready — loading module")
        if (!moduleLoaded) {
            loadGameModule()
        }
    }

    override fun onSurfaceLost() {
        Log.i(TAG, "Surface lost")
    }

    private fun loadGameModule() {
        val success = NativeBridge.nativeLoadModule(modulePath, dataPath, savePath)
        if (success) {
            moduleLoaded = true
            Log.i(TAG, "Module loaded successfully")
        } else {
            Log.e(TAG, "Failed to load module: $modulePath")
            runOnUiThread {
                Toast.makeText(this, R.string.error_load_failed, Toast.LENGTH_LONG).show()
                finish()
            }
        }
    }

    // ================================================================
    // Physical Gamepad Input
    // ================================================================

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null) return super.onKeyDown(keyCode, event)

        // Check if the event comes from a game controller
        if (isGamepadDevice(event.device)) {
            NativeBridge.dispatchKeyEvent(KeyEvent.ACTION_DOWN, keyCode)
            return true
        }

        // Handle back button to exit game
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            showExitConfirmation()
            return true
        }

        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null) return super.onKeyUp(keyCode, event)

        if (isGamepadDevice(event.device)) {
            NativeBridge.dispatchKeyEvent(KeyEvent.ACTION_UP, keyCode)
            return true
        }

        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent?): Boolean {
        if (event == null) return super.onGenericMotionEvent(event)

        // Analog sticks and triggers from physical gamepads
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            event.source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD
        ) {
            // Left stick
            val lx = event.getAxisValue(MotionEvent.AXIS_X)
            val ly = event.getAxisValue(MotionEvent.AXIS_Y)
            NativeBridge.setVirtualStick(
                false,
                (lx * Short.MAX_VALUE).toInt().toShort(),
                (ly * Short.MAX_VALUE).toInt().toShort()
            )

            // Right stick
            val rx = event.getAxisValue(MotionEvent.AXIS_Z)
            val ry = event.getAxisValue(MotionEvent.AXIS_RZ)
            NativeBridge.setVirtualStick(
                true,
                (rx * Short.MAX_VALUE).toInt().toShort(),
                (ry * Short.MAX_VALUE).toInt().toShort()
            )

            // Triggers
            val lt = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
            val rt = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
            NativeBridge.setVirtualTrigger(false, (lt * 255).toInt())
            NativeBridge.setVirtualTrigger(true, (rt * 255).toInt())

            // D-Pad (hat switch)
            val hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            val hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
            handleDpadFromHat(hatX, hatY)

            return true
        }

        return super.onGenericMotionEvent(event)
    }

    private fun handleDpadFromHat(hatX: Float, hatY: Float) {
        NativeBridge.setVirtualButton(NativeBridge.Buttons.DPAD_LEFT, hatX < -0.5f)
        NativeBridge.setVirtualButton(NativeBridge.Buttons.DPAD_RIGHT, hatX > 0.5f)
        NativeBridge.setVirtualButton(NativeBridge.Buttons.DPAD_UP, hatY < -0.5f)
        NativeBridge.setVirtualButton(NativeBridge.Buttons.DPAD_DOWN, hatY > 0.5f)
    }

    private fun isGamepadDevice(device: InputDevice?): Boolean {
        if (device == null) return false
        val sources = device.sources
        return (sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD) ||
               (sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)
    }

    // ================================================================
    // FPS Counter
    // ================================================================

    private fun startFpsCounter() {
        lifecycleScope.launch(Dispatchers.Main) {
            while (isActive) {
                val fps = NativeBridge.nativeGetFps()
                if (fps > 0f) {
                    fpsCounter.visibility = View.VISIBLE
                    fpsCounter.text = String.format("%.1f FPS", fps)
                }
                delay(500) // Update twice per second
            }
        }
    }

    // ================================================================
    // Exit Confirmation
    // ================================================================

    private fun showExitConfirmation() {
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.exit_game_title)
            .setMessage(getString(R.string.exit_game_message, gameTitle))
            .setPositiveButton(R.string.exit) { _, _ -> finish() }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }
}
