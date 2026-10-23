// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "io.github.resonancefx"
    compileSdk = 37

    defaultConfig {
        applicationId = "io.github.resonancefx"
        // Compose requires API 23; the root module itself supports API 21+.
        minSdk = 23
        targetSdk = 36
        versionCode = 20100
        versionName = "2.1.0"
    }

    // Release key from the environment (CI secrets or your shell): RFX_KEYSTORE (path),
    // RFX_KEYSTORE_PASSWORD, RFX_KEY_ALIAS, RFX_KEY_PASSWORD. Without it, release builds use the
    // debug key so they still install.
    val keystore = System.getenv("RFX_KEYSTORE")?.let { file(it) }?.takeIf { it.exists() }
    signingConfigs {
        if (keystore != null) {
            create("release") {
                storeFile = keystore
                storePassword = System.getenv("RFX_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("RFX_KEY_ALIAS")
                keyPassword = System.getenv("RFX_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.getByName(if (keystore != null) "release" else "debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    testOptions {
        unitTests.isReturnDefaultValues = true
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.09.00")
    implementation(composeBom)
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.11.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.4")
    implementation("androidx.lifecycle:lifecycle-service:2.9.4")
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20250517")
}
