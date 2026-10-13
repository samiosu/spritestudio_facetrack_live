import com.android.build.api.instrumentation.AsmClassVisitorFactory
import com.android.build.api.instrumentation.ClassContext
import com.android.build.api.instrumentation.ClassData
import com.android.build.api.instrumentation.FramesComputationMode
import com.android.build.api.instrumentation.InstrumentationParameters
import com.android.build.api.instrumentation.InstrumentationScope
import org.objectweb.asm.ClassVisitor
import org.objectweb.asm.ClassReader
import org.objectweb.asm.MethodVisitor
import org.objectweb.asm.Opcodes
import java.io.ByteArrayInputStream
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

/**
 * Local modification to MediaPipe tasks-core 0.10.32 (Apache-2.0).
 * Replace the bundled remote stats factory with MediaPipe's bundled no-op logger.
 * No inference or camera implementation changes. See docs/mediapipe-telemetry.md.
 */
abstract class DisableMediaPipeTelemetryFactory : AsmClassVisitorFactory<InstrumentationParameters.None> {
    override fun isInstrumentable(classData: ClassData) =
        classData.className == "com.google.mediapipe.tasks.core.logging.TasksStatsLoggerFactory"

    override fun createClassVisitor(classContext: ClassContext, nextClassVisitor: ClassVisitor): ClassVisitor =
        object : ClassVisitor(Opcodes.ASM9, nextClassVisitor) {
            private var replaced = false
            private val expectedDescriptor =
                "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Lcom/google/mediapipe/tasks/core/logging/TasksStatsLogger;"

            override fun visitMethod(access: Int, name: String, descriptor: String, signature: String?, exceptions: Array<out String>?): MethodVisitor? {
                val target = super.visitMethod(access, name, descriptor, signature, exceptions)
                if (name != "create") return target
                check(descriptor == expectedDescriptor && access and Opcodes.ACC_STATIC != 0 && !replaced) {
                    "MediaPipe stats factory changed; review the telemetry replacement before upgrading"
                }
                replaced = true
                requireNotNull(target)
                // Consume the original method body without forwarding it.
                return object : MethodVisitor(Opcodes.ASM9) {
                    override fun visitCode() {
                        target.visitCode()
                        target.visitVarInsn(Opcodes.ALOAD, 0)
                        target.visitVarInsn(Opcodes.ALOAD, 1)
                        target.visitVarInsn(Opcodes.ALOAD, 2)
                        target.visitMethodInsn(Opcodes.INVOKESTATIC,
                            "com/google/mediapipe/tasks/core/logging/TasksStatsDummyLogger",
                            "create", expectedDescriptor.replace("TasksStatsLogger;", "TasksStatsDummyLogger;"), false)
                        target.visitInsn(Opcodes.ARETURN)
                        target.visitMaxs(3, 3)
                    }
                    override fun visitEnd() { target.visitEnd() }
                }
            }

            override fun visitEnd() {
                check(replaced) { "MediaPipe stats factory entry point missing; audit the dependency" }
                super.visitEnd()
            }
        }
}

androidComponents {
    onVariants(selector().all()) { variant ->
        variant.instrumentation.transformClassesWith(DisableMediaPipeTelemetryFactory::class.java, InstrumentationScope.ALL) {}
        variant.instrumentation.setAsmFramesComputationMode(FramesComputationMode.COMPUTE_FRAMES_FOR_INSTRUMENTED_METHODS)
    }
}

android {
    namespace = "dev.spriteface.companion"
    compileSdk = 35
    defaultConfig {
        applicationId = "dev.spriteface.companion"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildTypes {
        release { isMinifyEnabled = false }
    }
    sourceSets.getByName("main").assets.srcDir(layout.buildDirectory.dir("generated/licenseAssets"))
}

val syncThirdPartyLicenses by tasks.registering(Sync::class) {
    from(rootProject.file("../../LICENSE"))
    from(rootProject.file("../../THIRD_PARTY_NOTICES.md"))
    from(rootProject.file("../../licenses")) { into("licenses") }
    into(layout.buildDirectory.dir("generated/licenseAssets/third_party_licenses"))
}

val verifyMediaPipeLogger by tasks.registering {
    group = "verification"
    description = "Fail closed if the pinned MediaPipe stats factory or no-op logger changes"
    doLast {
        val artifact = configurations.getByName("debugRuntimeClasspath").resolvedConfiguration.resolvedArtifacts
            .single { it.moduleVersion.id.group == "com.google.mediapipe" && it.name == "tasks-core" }
        check(artifact.moduleVersion.id.version == "0.10.32") { "Audit the new MediaPipe version before upgrading" }
        val classes = ZipFile(artifact.file).use { archive ->
            archive.getInputStream(requireNotNull(archive.getEntry("classes.jar"))).readBytes()
        }
        val entries = mutableMapOf<String, ByteArray>()
        ZipInputStream(ByteArrayInputStream(classes)).use { jar ->
            while (true) {
                val entry = jar.nextEntry ?: break
                if (entry.name.endsWith("/TasksStatsLoggerFactory.class") || entry.name.endsWith("/TasksStatsDummyLogger.class")) {
                    entries[entry.name.substringAfterLast('/')] = jar.readBytes()
                }
            }
        }
        check(entries.keys == setOf("TasksStatsLoggerFactory.class", "TasksStatsDummyLogger.class")) {
            "MediaPipe stats classes missing; audit before packaging"
        }
        var factorySeen = false
        ClassReader(entries.getValue("TasksStatsLoggerFactory.class")).accept(object : ClassVisitor(Opcodes.ASM9) {
            override fun visitMethod(access: Int, name: String, descriptor: String, signature: String?, exceptions: Array<out String>?): MethodVisitor? {
                if (name == "create") {
                    check(!factorySeen && access and Opcodes.ACC_STATIC != 0 && descriptor ==
                        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Lcom/google/mediapipe/tasks/core/logging/TasksStatsLogger;") {
                        "MediaPipe factory signature changed; audit before packaging"
                    }
                    factorySeen = true
                }
                return null
            }
        }, ClassReader.SKIP_DEBUG or ClassReader.SKIP_FRAMES)
        check(factorySeen) { "MediaPipe factory entry point missing" }
        ClassReader(entries.getValue("TasksStatsDummyLogger.class")).accept(object : ClassVisitor(Opcodes.ASM9) {
            override fun visitMethod(access: Int, name: String, descriptor: String, signature: String?, exceptions: Array<out String>?): MethodVisitor? {
                if (name == "<init>" || name == "create") return null
                return object : MethodVisitor(Opcodes.ASM9) {
                    override fun visitInsn(opcode: Int) { check(opcode == Opcodes.RETURN) { "MediaPipe dummy logger is no longer a no-op" } }
                    override fun visitMethodInsn(opcode: Int, owner: String, name: String, descriptor: String, isInterface: Boolean) {
                        error("MediaPipe dummy logger makes an unexpected call")
                    }
                    override fun visitFieldInsn(opcode: Int, owner: String, name: String, descriptor: String) {
                        error("MediaPipe dummy logger accesses an unexpected field")
                    }
                }
            }
        }, ClassReader.SKIP_DEBUG or ClassReader.SKIP_FRAMES)
    }
}
tasks.named("preBuild") { dependsOn(syncThirdPartyLicenses, verifyMediaPipeLogger) }

dependencies {
    implementation("androidx.activity:activity-ktx:1.10.1")
    implementation("androidx.camera:camera-core:1.4.2")
    implementation("androidx.camera:camera-camera2:1.4.2")
    implementation("androidx.camera:camera-lifecycle:1.4.2")
    implementation("androidx.camera:camera-view:1.4.2")
    implementation("com.google.mediapipe:tasks-vision:0.10.32")
    implementation("com.google.zxing:core:3.5.3")
    testImplementation("junit:junit:4.13.2")
}
