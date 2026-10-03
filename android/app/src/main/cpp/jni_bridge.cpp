#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "rexruntime.h"

#define LOG_TAG "ReXGlue.JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::string JstringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return {};
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

} // namespace

extern "C" {

// ============================================================
// Surface Management
// ============================================================

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_setSurface(
        JNIEnv* env, jclass /*clazz*/, jobject surface) {
    if (!surface) {
        rex::AndroidRuntime::Get().DestroySurface();
        return;
    }
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    rex::AndroidRuntime::Get().SetSurface(window);
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_destroySurface(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    rex::AndroidRuntime::Get().DestroySurface();
}

// ============================================================
// Input Dispatching
// ============================================================

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_dispatchKeyEvent(
        JNIEnv* /*env*/, jclass /*clazz*/, jint action, jint key_code) {
    rex::AndroidRuntime::Get().DispatchKeyEvent(action, key_code);
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_dispatchMotionEvent(
        JNIEnv* /*env*/, jclass /*clazz*/, jint axis, jfloat value) {
    rex::AndroidRuntime::Get().DispatchMotionEvent(axis, value);
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_setVirtualButton(
        JNIEnv* /*env*/, jclass /*clazz*/, jint button_mask, jboolean pressed) {
    rex::AndroidRuntime::Get().DispatchVirtualButton(
        static_cast<uint16_t>(button_mask), pressed == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_setVirtualStick(
        JNIEnv* /*env*/, jclass /*clazz*/, jboolean is_right, jshort x, jshort y) {
    rex::AndroidRuntime::Get().DispatchVirtualStick(is_right == JNI_TRUE, x, y);
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_setVirtualTrigger(
        JNIEnv* /*env*/, jclass /*clazz*/, jboolean is_right, jint value) {
    rex::AndroidRuntime::Get().DispatchVirtualTrigger(
        is_right == JNI_TRUE, static_cast<uint8_t>(value));
}

// ============================================================
// Lifecycle Management
// ============================================================

JNIEXPORT jboolean JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeInit(
        JNIEnv* env, jclass /*clazz*/,
        jstring internalDataPath,
        jstring externalDataPath) {
    std::string internal_path = JstringToString(env, internalDataPath);
    std::string external_path = JstringToString(env, externalDataPath);
    return rex::AndroidRuntime::Get().Initialize(internal_path, external_path) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeShutdown(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    rex::AndroidRuntime::Get().Shutdown();
}

JNIEXPORT jboolean JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeLoadModule(
        JNIEnv* env, jclass /*clazz*/,
        jstring modulePath,
        jstring gameDataPath,
        jstring saveDataPath) {
    std::string module = JstringToString(env, modulePath);
    std::string game_data = JstringToString(env, gameDataPath);
    std::string save_data = JstringToString(env, saveDataPath);
    return rex::AndroidRuntime::Get().LoadModule(module, game_data, save_data) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_nativePause(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    rex::AndroidRuntime::Get().Pause();
}

JNIEXPORT void JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeResume(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    rex::AndroidRuntime::Get().Resume();
}

JNIEXPORT jboolean JNICALL
Java_com_rexglue_runtime_NativeBridge_isRunning(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    return rex::AndroidRuntime::Get().IsRunning() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeGetVersion(
        JNIEnv* env, jclass /*clazz*/) {
    return env->NewStringUTF(rex::AndroidRuntime::Get().GetVersion().c_str());
}

JNIEXPORT jfloat JNICALL
Java_com_rexglue_runtime_NativeBridge_nativeGetFps(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    return rex::AndroidRuntime::Get().GetFps();
}

} // extern "C"
