// NFSMW Recompiled - APK de Android para Qualcomm Snapdragon (arm64-v8a)
//
// Antes de compilar hace falta el SDK preparado:
//     python tools/android/preparar_sdk.py
//
// Sin codigo generado (app/generated-android) sale un APK que solo lleva la
// sonda de Vulkan: se puede publicar y sirve para probar movil y driver.
// CON codigo generado, el APK lleva dentro el juego traducido: NO SE PUBLICA.

plugins {
    id("com.android.application")
}

val repo: File = rootProject.projectDir.parentFile
val sdkAndroid: File = (findProperty("nfsmw.sdk") as String?)?.let { file(it) }
    ?: File(repo.parentFile, "rexglue-sdk-android")
// El codigo generado. Con otra carpeta se pueden tener dos ediciones del juego
// a la vez:  gradlew assembleRelease -Pnfsmw.generado=<repo>/app/generated-android/usa
val generadoPropio: File? = (findProperty("nfsmw.generado") as String?)?.let { file(it) }
val generado: File = generadoPropio ?: File(repo, "app/generated-android")

// El motor del juego (ver docs/motor-nativo.md):
//   xenos   el de siempre: el SDK imita la GPU de la Xbox 360.
//   nativo  el renderizador nativo de nfsmw-android, que dibuja con Vulkan sin
//           imitarla:  gradlew assembleRelease -Pnfsmw.motor=nativo
// El nativo es OTRO arbol nativo entero (su SDK, su app y su codigo generado),
// preparado al lado de este repositorio por tools/android/preparar_nativo.py.
val motor: String = (findProperty("nfsmw.motor") as String?) ?: "xenos"
val esNativo = motor == "nativo"
require(motor == "xenos" || esNativo) { "nfsmw.motor tiene que ser xenos o nativo (es '$motor')" }
val nativo: File = (findProperty("nfsmw.nativo") as String?)?.let { file(it) }
    ?: File(repo.parentFile, "nfsmw-android")
// La edicion del juego con el motor nativo. Su app esta escrita para PAL Espana
// (app/ de su arbol); otra edicion es otra copia de la app con las direcciones
// traducidas, app_<edicion>, con su biblioteca de shaders en out/<edicion>.
// Las prepara preparar_nativo.py --iso <iso de esa edicion>:
//   gradlew assembleRelease -Pnfsmw.motor=nativo -Pnfsmw.edicion=usa
val edicionNativa: String = (findProperty("nfsmw.edicion") as String?) ?: "pal_es"
val appNativa: File = File(nativo, if (edicionNativa == "pal_es") "app" else "app_$edicionNativa")
val outNativo: File = File(nativo, if (edicionNativa == "pal_es") "out" else "out/$edicionNativa")

val conJuego = if (esNativo) File(appNativa, "generated/default/sources.cmake").isFile
    else File(generado, "default/sources.cmake").isFile

// Para que edicion del juego es ese codigo. Lo apunta generar_codigo.py (o
// preparar_nativo.py); sin el fichero, es codigo generado antes de que hubiera
// ediciones: PAL Espana.
val edicion: Map<*, *> = (if (esNativo) File(outNativo, "edicion.json") else File(generado, "edicion.json")).let {
    if (it.isFile) groovy.json.JsonSlurper().parse(it) as Map<*, *> else emptyMap<String, Any>()
}

// Motor nativo: sus ajustes (nfsmw.toml) y su biblioteca de shaders, que sale de
// TU copia del juego, viajan en el APK. MotorNativo.java los instala al jugar.
val assetsNativo = layout.buildDirectory.dir("generated/nfsmw-nativo-assets")
val copiarAssetsNativo = tasks.register<Copy>("copiarAssetsNativo") {
    from(File(nativo, "android/app/src/main/assets/nfsmw.toml"))
    from(File(outNativo, "nfsmw_shaders.nfsp"))
    into(assetsNativo.map { it.dir("nativo") })
    doFirst {
        require(File(outNativo, "nfsmw_shaders.nfsp").isFile) {
            "Falta la biblioteca de shaders del motor nativo. Lanza: python tools/android/preparar_nativo.py"
        }
    }
}

android {
    namespace = "io.github.nfsmwrecomp"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "io.github.nfsmwrecomp"
        // 29: memfd para adrenotools y un cargador de librerias con los espacios
        // de nombres que necesita. Por debajo no hay Snapdragon que merezca la pena.
        minSdk = 29
        targetSdk = 35
        versionCode = 5
        versionName = "0.4.0"

        buildConfigField("boolean", "CON_JUEGO", conJuego.toString())
        buildConfigField("String", "MOTOR", "\"$motor\"")
        // Un APK lleva el codigo de UNA edicion: su nombre, el SHA-256 de su
        // default.xex (para avisar si se elige la ISO de otra) y el idioma y el
        // pais de Xbox 360 que se le pasan al juego.
        buildConfigField("String", "EDICION", "\"${edicion["nombre"] ?: "PAL España"}\"")
        buildConfigField("String", "EDICION_XEX", "\"${edicion["sha256"] ?: ""}\"")
        buildConfigField("int", "JUEGO_IDIOMA", "${edicion["idioma"] ?: 5}")
        buildConfigField("int", "JUEGO_PAIS", "${edicion["pais"] ?: 31}")
        // El log del juego tambien en release, para diagnosticar un movil:
        //     gradlew assembleRelease -Pnfsmw.registro=true
        buildConfigField("boolean", "REGISTRO", ((findProperty("nfsmw.registro") as String?) == "true").toString())

        ndk {
            // Solo ARM64: el codigo generado, las rutas NEON y adrenotools.
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    // Release tambien en el APK de depuracion: el codigo
                    // generado a -O0 no se puede jugar.
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DNFSMW_REPO=${repo.absolutePath}",
                    "-DNFSMW_SDK=${sdkAndroid.absolutePath}",
                    "-DREX_BUILD_DIAGNOSTICS=OFF",
                )
                // Solo si se pide otra carpeta: AGP da a cada juego de argumentos
                // su propia carpeta de compilacion, y cambiarlos en el caso
                // normal obligaria a recompilarlo todo.
                if (generadoPropio != null) {
                    arguments += "-DNFSMW_GEN_DIR=${File(generadoPropio, "default").absolutePath}"
                }
                if (esNativo) {
                    arguments += listOf("-DNFSMW_MOTOR=nativo", "-DNFSMW_NATIVO=${nativo.absolutePath}")
                    // Igual que NFSMW_GEN_DIR: solo con otra edicion, para no
                    // cambiar la carpeta de compilacion de la PAL.
                    if (edicionNativa != "pal_es") {
                        arguments += "-DNFSMW_NATIVO_APP=${appNativa.name}"
                    }
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }

    buildFeatures {
        buildConfig = true
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Firmado con la clave de depuracion local: el APK es para instalarlo
            // en TUS dispositivos, no para una tienda.
            signingConfig = signingConfigs.getByName("debug")
            // Para sacar ficheros de la app con "adb shell run-as" en una prueba
            // (capturas, caches). El codigo nativo se compila igual:
            //     gradlew assembleRelease -Pnfsmw.depurable=true
            isDebuggable = (findProperty("nfsmw.depurable") as String?) == "true"
        }
    }

    packaging {
        jniLibs {
            // Obligatorio: adrenotools busca sus hooks en nativeLibraryDir y el
            // SDK carga librexgpu-xenos.so de ahi con dlopen. Sin esto las .so
            // se quedan dentro del APK y no existen como ficheros.
            useLegacyPackaging = true
        }
    }

    sourceSets {
        getByName("main") {
            // Las clases Java de SDL3 (SDLActivity y compania) se usan
            // directamente desde el submodulo del SDK, sin copiarlas.
            // Con el motor nativo, las de su SDK: es el mismo SDL.
            java.directories.add(
                File(if (esNativo) File(nativo, "sdk") else sdkAndroid,
                    "thirdparty/sdl3/android-project/app/src/main/java").absolutePath
            )
            if (esNativo) {
                assets.directories.add(assetsNativo.get().asFile.absolutePath)
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    dependencies {
        implementation("androidx.appcompat:appcompat:1.6.1")
        implementation("com.google.android.material:material:1.11.0")
    }
}

if (esNativo) {
    tasks.named("preBuild") { dependsOn(copiarAssetsNativo) }
}
