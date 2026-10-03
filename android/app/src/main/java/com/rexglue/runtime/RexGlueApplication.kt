/**
 * RexGlueApplication — Application class for global initialization
 *
 * Responsibilities:
 * - Load native libraries in the correct order
 * - Initialize crash reporting
 * - Set up global app state
 */
package com.rexglue.runtime

import android.app.Application
import android.util.Log

class RexGlueApplication : Application() {

    companion object {
        private const val TAG = "ReXGlue"

        /** Whether native libraries loaded successfully */
        var nativeLibrariesLoaded = false
            private set

        /** Global application instance */
        lateinit var instance: RexGlueApplication
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        loadNativeLibraries()
    }

    /**
     * Load native .so libraries in dependency order.
     *
     * Load order matters:
     * 1. c++_shared  — C++ standard library (STL)
     * 2. rexruntime  — Core runtime (contains rexcore, rexui, rexaudio, etc.)
     * 3. rexglue_jni — App-specific JNI bridge
     *
     * Note: librexgpu-xenos.so is NOT loaded here. It is loaded at runtime
     * by the native code via dlopen() when a game starts (see gpu_plugin_loader.cpp).
     */
    private fun loadNativeLibraries() {
        try {
            System.loadLibrary("c++_shared")
            Log.i(TAG, "Loaded: libc++_shared.so")

            System.loadLibrary("rexruntime")
            Log.i(TAG, "Loaded: librexruntime.so")

            System.loadLibrary("rexglue_jni")
            Log.i(TAG, "Loaded: librexglue_jni.so")

            nativeLibrariesLoaded = true
            Log.i(TAG, "All native libraries loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native libraries: ${e.message}", e)
            nativeLibrariesLoaded = false
        }
    }
}
