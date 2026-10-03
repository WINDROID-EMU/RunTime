/**
 * VirtualControllerView — Touch-based Xbox 360 gamepad overlay
 *
 * Renders a semi-transparent gamepad overlay on top of the game surface
 * and translates touch events into NativeBridge button/stick/trigger calls.
 *
 * Layout:
 *   Left side:  D-Pad (top), Left Stick (bottom)
 *   Right side: Face buttons (A/B/X/Y) (top), Right Stick (bottom)
 *   Top:        LB, LT, Back, Start, RB, RT
 */
package com.rexglue.runtime

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Build
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.min

/**
 * Custom View that draws and handles a virtual Xbox 360 controller.
 *
 * The controller is drawn using Canvas primitives (no bitmaps needed)
 * with a clean, semi-transparent design.
 */
class VirtualControllerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : View(context, attrs, defStyle) {

    // ================================================================
    // Paint and Style
    // ================================================================

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(60, 255, 255, 255)
        style = Paint.Style.FILL
    }

    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }

    private val pressedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(140, 100, 200, 255)
        style = Paint.Style.FILL
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 255, 255, 255)
        textAlign = Paint.Align.CENTER
        textSize = 36f
        isFakeBoldText = true
    }

    private val stickPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(100, 200, 200, 200)
        style = Paint.Style.FILL
    }

    private val stickKnobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(180, 150, 200, 255)
        style = Paint.Style.FILL
    }

    // ================================================================
    // Button / Zone definitions
    // ================================================================

    /** A touchable zone on the controller */
    data class TouchZone(
        val id: String,
        val bounds: RectF = RectF(),
        val label: String,
        val buttonMask: Int = 0,
        var isPressed: Boolean = false,
        var pointerId: Int = -1
    )

    /** An analog stick zone */
    data class StickZone(
        val id: String,
        val isRight: Boolean,
        var centerX: Float = 0f,
        var centerY: Float = 0f,
        var radius: Float = 0f,
        var knobX: Float = 0f,
        var knobY: Float = 0f,
        var pointerId: Int = -1,
        var active: Boolean = false
    )

    // Face buttons (A, B, X, Y)
    private val buttonA = TouchZone("a", label = "A", buttonMask = NativeBridge.Buttons.A)
    private val buttonB = TouchZone("b", label = "B", buttonMask = NativeBridge.Buttons.B)
    private val buttonX = TouchZone("x", label = "X", buttonMask = NativeBridge.Buttons.X)
    private val buttonY = TouchZone("y", label = "Y", buttonMask = NativeBridge.Buttons.Y)

    // D-Pad
    private val dpadUp = TouchZone("dup", label = "▲", buttonMask = NativeBridge.Buttons.DPAD_UP)
    private val dpadDown = TouchZone("ddn", label = "▼", buttonMask = NativeBridge.Buttons.DPAD_DOWN)
    private val dpadLeft = TouchZone("dlt", label = "◄", buttonMask = NativeBridge.Buttons.DPAD_LEFT)
    private val dpadRight = TouchZone("drt", label = "►", buttonMask = NativeBridge.Buttons.DPAD_RIGHT)

    // Shoulder buttons and triggers
    private val btnLB = TouchZone("lb", label = "LB", buttonMask = NativeBridge.Buttons.LEFT_SHOULDER)
    private val btnRB = TouchZone("rb", label = "RB", buttonMask = NativeBridge.Buttons.RIGHT_SHOULDER)
    private val btnLT = TouchZone("lt", label = "LT")  // Trigger — uses setVirtualTrigger
    private val btnRT = TouchZone("rt", label = "RT")  // Trigger — uses setVirtualTrigger

    // Menu buttons
    private val btnStart = TouchZone("start", label = "≡", buttonMask = NativeBridge.Buttons.START)
    private val btnBack = TouchZone("back", label = "⊞", buttonMask = NativeBridge.Buttons.BACK)

    // Sticks
    private val leftStick = StickZone("ls", isRight = false)
    private val rightStick = StickZone("rs", isRight = true)

    // All buttons for iteration
    private val allButtons = listOf(
        buttonA, buttonB, buttonX, buttonY,
        dpadUp, dpadDown, dpadLeft, dpadRight,
        btnLB, btnRB, btnLT, btnRT,
        btnStart, btnBack
    )

    // ================================================================
    // Layout
    // ================================================================

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        layoutControls(w.toFloat(), h.toFloat())
    }

    private fun layoutControls(w: Float, h: Float) {
        val unit = min(w, h) / 20f  // Base unit for proportional sizing
        val btnSize = unit * 2.2f
        val margin = unit * 0.6f

        // ---- D-Pad (lower-left area) ----
        val dpadCx = unit * 4.5f
        val dpadCy = h - unit * 6f
        val dpadSpread = btnSize * 1.1f

        dpadUp.bounds.set(dpadCx - btnSize/2, dpadCy - dpadSpread - btnSize/2,
            dpadCx + btnSize/2, dpadCy - dpadSpread + btnSize/2)
        dpadDown.bounds.set(dpadCx - btnSize/2, dpadCy + dpadSpread - btnSize/2,
            dpadCx + btnSize/2, dpadCy + dpadSpread + btnSize/2)
        dpadLeft.bounds.set(dpadCx - dpadSpread - btnSize/2, dpadCy - btnSize/2,
            dpadCx - dpadSpread + btnSize/2, dpadCy + btnSize/2)
        dpadRight.bounds.set(dpadCx + dpadSpread - btnSize/2, dpadCy - btnSize/2,
            dpadCx + dpadSpread + btnSize/2, dpadCy + btnSize/2)

        // ---- Face Buttons (lower-right area) ----
        val faceCx = w - unit * 4.5f
        val faceCy = h - unit * 6f
        val faceSpread = btnSize * 1.1f

        buttonY.bounds.set(faceCx - btnSize/2, faceCy - faceSpread - btnSize/2,
            faceCx + btnSize/2, faceCy - faceSpread + btnSize/2)
        buttonA.bounds.set(faceCx - btnSize/2, faceCy + faceSpread - btnSize/2,
            faceCx + btnSize/2, faceCy + faceSpread + btnSize/2)
        buttonX.bounds.set(faceCx - faceSpread - btnSize/2, faceCy - btnSize/2,
            faceCx - faceSpread + btnSize/2, faceCy + btnSize/2)
        buttonB.bounds.set(faceCx + faceSpread - btnSize/2, faceCy - btnSize/2,
            faceCx + faceSpread + btnSize/2, faceCy + btnSize/2)

        // ---- Left Stick (bottom-left) ----
        leftStick.radius = unit * 3f
        leftStick.centerX = unit * 4.5f
        leftStick.centerY = h - unit * 2.5f
        leftStick.knobX = leftStick.centerX
        leftStick.knobY = leftStick.centerY

        // ---- Right Stick (bottom-right) ----
        rightStick.radius = unit * 3f
        rightStick.centerX = w - unit * 4.5f
        rightStick.centerY = h - unit * 2.5f
        rightStick.knobX = rightStick.centerX
        rightStick.knobY = rightStick.centerY

        // ---- Shoulder Buttons (top) ----
        val topY = margin
        val shoulderW = unit * 3f
        val shoulderH = unit * 1.8f

        btnLT.bounds.set(margin, topY, margin + shoulderW, topY + shoulderH)
        btnLB.bounds.set(margin + shoulderW + margin, topY,
            margin + shoulderW * 2 + margin, topY + shoulderH)

        btnRT.bounds.set(w - margin - shoulderW, topY, w - margin, topY + shoulderH)
        btnRB.bounds.set(w - margin - shoulderW * 2 - margin, topY,
            w - margin - shoulderW, topY + shoulderH)

        // ---- Menu Buttons (top center) ----
        val menuW = unit * 2.5f
        val menuH = unit * 1.5f
        val menuCx = w / 2f

        btnBack.bounds.set(menuCx - menuW - margin/2, topY,
            menuCx - margin/2, topY + menuH)
        btnStart.bounds.set(menuCx + margin/2, topY,
            menuCx + menuW + margin/2, topY + menuH)

        // Scale text
        textPaint.textSize = btnSize * 0.45f
    }

    // ================================================================
    // Drawing
    // ================================================================

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Draw buttons
        for (btn in allButtons) {
            val paint = if (btn.isPressed) pressedPaint else bgPaint
            val r = min(btn.bounds.width(), btn.bounds.height()) / 2f
            canvas.drawRoundRect(btn.bounds, r, r, paint)
            canvas.drawRoundRect(btn.bounds, r, r, borderPaint)
            canvas.drawText(
                btn.label,
                btn.bounds.centerX(),
                btn.bounds.centerY() + textPaint.textSize / 3f,
                textPaint
            )
        }

        // Draw sticks
        for (stick in listOf(leftStick, rightStick)) {
            // Base circle
            canvas.drawCircle(stick.centerX, stick.centerY, stick.radius, stickPaint)
            canvas.drawCircle(stick.centerX, stick.centerY, stick.radius, borderPaint)
            // Knob
            val knobRadius = stick.radius * 0.45f
            canvas.drawCircle(stick.knobX, stick.knobY, knobRadius, stickKnobPaint)
            canvas.drawCircle(stick.knobX, stick.knobY, knobRadius, borderPaint)
        }
    }

    // ================================================================
    // Touch Handling
    // ================================================================

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val x = event.getX(idx)
                val y = event.getY(idx)
                val pid = event.getPointerId(idx)
                handleTouchDown(x, y, pid)
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    handleTouchMove(event.getX(i), event.getY(i), event.getPointerId(i))
                }
            }
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                handleTouchUp(pid)
            }
            MotionEvent.ACTION_CANCEL -> {
                releaseAll()
            }
        }
        invalidate()
        return true
    }

    private fun handleTouchDown(x: Float, y: Float, pointerId: Int) {
        // Check sticks first
        for (stick in listOf(leftStick, rightStick)) {
            val dx = x - stick.centerX
            val dy = y - stick.centerY
            if (hypot(dx, dy) <= stick.radius * 1.5f) {
                stick.pointerId = pointerId
                stick.active = true
                updateStick(stick, x, y)
                return
            }
        }

        // Check buttons
        for (btn in allButtons) {
            if (btn.bounds.contains(x, y)) {
                btn.isPressed = true
                btn.pointerId = pointerId
                onButtonPressed(btn)
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                return
            }
        }
    }

    private fun handleTouchMove(x: Float, y: Float, pointerId: Int) {
        // Update sticks
        for (stick in listOf(leftStick, rightStick)) {
            if (stick.pointerId == pointerId && stick.active) {
                updateStick(stick, x, y)
                return
            }
        }
    }

    private fun handleTouchUp(pointerId: Int) {
        // Release sticks
        for (stick in listOf(leftStick, rightStick)) {
            if (stick.pointerId == pointerId) {
                stick.active = false
                stick.pointerId = -1
                stick.knobX = stick.centerX
                stick.knobY = stick.centerY
                NativeBridge.setVirtualStick(stick.isRight, 0, 0)
                return
            }
        }

        // Release buttons
        for (btn in allButtons) {
            if (btn.pointerId == pointerId && btn.isPressed) {
                btn.isPressed = false
                btn.pointerId = -1
                onButtonReleased(btn)
                return
            }
        }
    }

    private fun releaseAll() {
        for (btn in allButtons) {
            if (btn.isPressed) {
                btn.isPressed = false
                btn.pointerId = -1
                onButtonReleased(btn)
            }
        }
        for (stick in listOf(leftStick, rightStick)) {
            if (stick.active) {
                stick.active = false
                stick.pointerId = -1
                stick.knobX = stick.centerX
                stick.knobY = stick.centerY
                NativeBridge.setVirtualStick(stick.isRight, 0, 0)
            }
        }
        invalidate()
    }

    // ================================================================
    // Stick Processing
    // ================================================================

    private fun updateStick(stick: StickZone, touchX: Float, touchY: Float) {
        var dx = touchX - stick.centerX
        var dy = touchY - stick.centerY
        val dist = hypot(dx, dy)

        // Clamp to radius
        if (dist > stick.radius) {
            dx = dx / dist * stick.radius
            dy = dy / dist * stick.radius
        }

        stick.knobX = stick.centerX + dx
        stick.knobY = stick.centerY + dy

        // Normalize to -32768..32767
        val nx = (dx / stick.radius * Short.MAX_VALUE).toInt().toShort()
        val ny = (dy / stick.radius * Short.MAX_VALUE).toInt().toShort()

        NativeBridge.setVirtualStick(stick.isRight, nx, ny)
    }

    // ================================================================
    // Button Events
    // ================================================================

    private fun onButtonPressed(btn: TouchZone) {
        when (btn.id) {
            "lt" -> NativeBridge.setVirtualTrigger(false, 255)
            "rt" -> NativeBridge.setVirtualTrigger(true, 255)
            else -> {
                if (btn.buttonMask != 0) {
                    NativeBridge.setVirtualButton(btn.buttonMask, true)
                }
            }
        }
    }

    private fun onButtonReleased(btn: TouchZone) {
        when (btn.id) {
            "lt" -> NativeBridge.setVirtualTrigger(false, 0)
            "rt" -> NativeBridge.setVirtualTrigger(true, 0)
            else -> {
                if (btn.buttonMask != 0) {
                    NativeBridge.setVirtualButton(btn.buttonMask, false)
                }
            }
        }
    }

    // ================================================================
    // Visibility Toggle
    // ================================================================

    /** Toggle controller visibility (used when physical gamepad is connected) */
    fun setControllerVisible(visible: Boolean) {
        visibility = if (visible) VISIBLE else GONE
        if (!visible) {
            releaseAll()
        }
    }
}
