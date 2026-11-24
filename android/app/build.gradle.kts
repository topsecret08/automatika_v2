plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.automatika.autopilot"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.automatika.autopilot"
        minSdk = 26
        targetSdk = 35
        versionCode = 8
        versionName = "0.9.7"
    }

    buildFeatures {
        compose = true
        // Версия приложения нужна для проверки обновлений (BuildConfig.VERSION_NAME).
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    // Подпись release-APK: только если CI передал keystore через переменные окружения.
    // Без них release собирается без подписи, а CI публикует debug-APK, как раньше.
    signingConfigs {
        create("release") {
            System.getenv("ANDROID_KEYSTORE_PATH")?.let { path ->
                storeFile = file(path)
                storePassword = System.getenv("ANDROID_STORE_PASSWORD")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (System.getenv("ANDROID_KEYSTORE_PATH") != null) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.00")
    implementation(composeBom)
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.6")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}
