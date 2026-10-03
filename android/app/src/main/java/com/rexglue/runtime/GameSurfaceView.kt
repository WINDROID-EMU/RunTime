/**
 * GameSurfaceView — SurfaceView for Vulkan rendering
 *
 * This custom SurfaceView provides the ANativeWindow that the native
 * Vulkan renderer uses for presentation. It handles surface lifecycle
 * callbacks and forwards them to the NativeBridge.
 */
package com.rexglue.runtime

import android.content.Context
import android.util.AttributeSet
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView

class GameSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : SurfaceView(context, attrs, defStyle), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "ReXGlue.Surface"
    }

    /** Listener for surface lifecycle events */
    interface SurfaceListener {
        fun onSurfaceReady()
        fun onSurfaceLost()
    }

    var surfaceListener: SurfaceListener? = null
    private var surfaceReady = false

    init {
        holder.addCallback(this)
        // Request a fixed-size buffer for Vulkan
        // The actual size will be set when the surface is created
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceCreated")
        surfaceReady = true

        // Forward the Surface to native code
        // This creates an ANativeWindow and initializes the Vulkan swapchain
        NativeBridge.setSurface(holder.surface)
        surfaceListener?.onSurfaceReady()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged: ${width}x${height} format=$format")

        // Notify native code of the new surface dimensions
        // The Vulkan swapchain will be recreated
        NativeBridge.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceDestroyed")
        surfaceReady = false

        // Tell native code to release the Vulkan swapchain and ANativeWindow
        NativeBridge.destroySurface()
        surfaceListener?.onSurfaceLost()
    }

    /** Whether the surface is currently available for rendering */
    fun isSurfaceReady(): Boolean = surfaceReady
}
