/**
 * NativeBridge — Kotlin-side JNI declarations matching the C++ exports.
 *
 * This class mirrors the JNI functions exported by:
 * - android_bridge.cpp (SDK: surface, input forwarding)
 * - jni_bridge.cpp     (App: lifecycle, module loading, queries)
 *
 * All methods are static (matching the C++ `jclass` JNI convention).
 * The C++ function names follow the pattern:
 *   Java_com_rexglue_runtime_NativeBridge_<methodName>
 */
package com.rexglue.runtime

import android.view.Surface

object NativeBridge {

    // ================================================================
    // Lifecycle (jni_bridge.cpp)
    // ================================================================

    /**
     * Initialize the native runtime.
     * Must be called before any other native method (except getVersion).
     *
     * @param internalDataPath  App-private internal storage path
     *                          (Context.filesDir.absolutePath)
     * @param externalDataPath  App external files path
     *                          (Context.getExternalFilesDir(null).absolutePath)
     * @return true if initialization succeeded
     */
    @JvmStatic
    external fun nativeInit(
        internalDataPath: String,
        externalDataPath: String
    ): Boolean

    /**
     * Shut down the native runtime and release all resources.
     * Blocks until the game thread has exited.
     */
    @JvmStatic
    external fun nativeShutdown()

    /**
     * Pause the native runtime (render + audio threads).
     * Called from Activity.onPause().
     */
    @JvmStatic
    external fun nativePause()

    /**
     * Resume the native runtime.
     * Called from Activity.onResume().
     */
    @JvmStatic
    external fun nativeResume()

    // ================================================================
    // Module loading (jni_bridge.cpp)
    // ================================================================

    /**
     * Load and start a recompiled game module.
     *
     * @param modulePath    Path to the game's .so module file
     * @param gameDataPath  Path to the game's data directory
     * @param saveDataPath  Path to the game's save/user data directory
     * @return true if the module was loaded and the game loop started
     */
    @JvmStatic
    external fun nativeLoadModule(
        modulePath: String,
        gameDataPath: String,
        saveDataPath: String
    ): Boolean

    // ================================================================
    // Runtime queries (jni_bridge.cpp)
    // ================================================================

    /** Check if the game loop is currently running */
    @JvmStatic
    external fun isRunning(): Boolean

    /** Get the native SDK version string */
    @JvmStatic
    external fun nativeGetVersion(): String

    /** Get the current rendering FPS */
    @JvmStatic
    external fun nativeGetFps(): Float

    // ================================================================
    // Surface management (android_bridge.cpp — already in SDK)
    // ================================================================

    /**
     * Set the rendering surface. Called when the SurfaceView's surface
     * is created or changed. Pass null to release the surface.
     *
     * @param surface  The Android Surface to render to, or null
     */
    @JvmStatic
    external fun setSurface(surface: Surface?)

    /**
     * Destroy the rendering surface. Called when the SurfaceView's
     * surface is destroyed.
     */
    @JvmStatic
    external fun destroySurface()

    // ================================================================
    // Input forwarding (android_bridge.cpp — already in SDK)
    // ================================================================

    /**
     * Forward a key event (from hardware gamepad or keyboard).
     *
     * @param action    KeyEvent.ACTION_DOWN or ACTION_UP
     * @param keyCode   KeyEvent key code (e.g., KeyEvent.KEYCODE_BUTTON_A)
     */
    @JvmStatic
    external fun dispatchKeyEvent(action: Int, keyCode: Int)

    /**
     * Forward a motion axis event (analog stick, trigger).
     *
     * @param axis   MotionEvent axis (e.g., MotionEvent.AXIS_X)
     * @param value  Axis value (-1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers)
     */
    @JvmStatic
    external fun dispatchMotionEvent(axis: Int, value: Float)

    /**
     * Set virtual button state (from touch overlay).
     *
     * Xbox 360 button mask bits:
     * - 0x0001: DPAD_UP       - 0x0100: START
     * - 0x0002: DPAD_DOWN     - 0x0200: BACK
     * - 0x0004: DPAD_LEFT     - 0x0400: LEFT_THUMB
     * - 0x0008: DPAD_RIGHT    - 0x0800: RIGHT_THUMB
     * - 0x0010: START (alt)   - 0x1000: A
     * - 0x0020: BACK (alt)    - 0x2000: B
     * - 0x0040: LEFT_SHOULDER - 0x4000: X
     * - 0x0080: RIGHT_SHOULDER- 0x8000: Y
     *
     * @param buttonMask  Bitmask of the button(s)
     * @param pressed     true if pressed, false if released
     */
    @JvmStatic
    external fun setVirtualButton(buttonMask: Int, pressed: Boolean)

    /**
     * Set virtual analog stick position (from touch overlay).
     *
     * @param isRight  false = left stick, true = right stick
     * @param x        X axis value (-32768 to 32767)
     * @param y        Y axis value (-32768 to 32767)
     */
    @JvmStatic
    external fun setVirtualStick(isRight: Boolean, x: Short, y: Short)

    /**
     * Set virtual trigger value (from touch overlay).
     *
     * @param isRight  false = left trigger (LT), true = right trigger (RT)
     * @param value    Trigger value (0-255)
     */
    @JvmStatic
    external fun setVirtualTrigger(isRight: Boolean, value: Int)

    // ================================================================
    // Xbox 360 Button Constants (matching Xenos XINPUT_GAMEPAD_*)
    // ================================================================

    object Buttons {
        const val DPAD_UP        = 0x0001
        const val DPAD_DOWN      = 0x0002
        const val DPAD_LEFT      = 0x0004
        const val DPAD_RIGHT     = 0x0008
        const val START          = 0x0010
        const val BACK           = 0x0020
        const val LEFT_THUMB     = 0x0040
        const val RIGHT_THUMB    = 0x0080
        const val LEFT_SHOULDER  = 0x0100
        const val RIGHT_SHOULDER = 0x0200
        const val GUIDE          = 0x0400
        const val A              = 0x1000
        const val B              = 0x2000
        const val X              = 0x4000
        const val Y              = 0x8000
    }
}
