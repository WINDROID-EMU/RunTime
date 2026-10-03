# ProGuard / R8 rules for ReXGlue Android
# ==========================================

# Keep JNI native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep NativeBridge — called from native code via JNI
-keep class com.rexglue.runtime.NativeBridge { *; }

# Keep Application class
-keep class com.rexglue.runtime.RexGlueApplication { *; }

# Keep all activities (referenced in manifest)
-keep class com.rexglue.runtime.MainActivity { *; }
-keep class com.rexglue.runtime.GameActivity { *; }
-keep class com.rexglue.runtime.SettingsActivity { *; }

# Keep Parcelable implementations
-keepclassmembers class * implements android.os.Parcelable {
    public static final android.os.Parcelable$Creator CREATOR;
}

# Keep Serializable
-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    !static !transient <fields>;
    !private <fields>;
    !private <methods>;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}

# Keep enums
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# Don't warn about missing annotations
-dontwarn javax.annotation.**
-dontwarn kotlin.jvm.internal.**
