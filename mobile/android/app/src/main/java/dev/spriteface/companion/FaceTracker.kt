package dev.spriteface.companion

import android.content.Context
import android.graphics.Bitmap
import android.os.SystemClock
import com.google.mediapipe.framework.image.BitmapImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import kotlin.math.max

/** Called and closed on the CameraX analysis executor, never concurrently. */
class FaceTracker(context: Context) : AutoCloseable {
    private val landmarker: FaceLandmarker
    private var previousTimestamp = -1L

    init {
        require(context.assets.list("")?.contains("face_landmarker.task") == true) {
            "顔モデルがありません。READMEのモデル取得手順でAPKをビルドしてください"
        }
        val options = FaceLandmarker.FaceLandmarkerOptions.builder()
            .setBaseOptions(BaseOptions.builder().setModelAssetPath("face_landmarker.task").build())
            .setRunningMode(RunningMode.VIDEO)
            .setNumFaces(1)
            .setMinFaceDetectionConfidence(0.5f)
            .setMinFacePresenceConfidence(0.5f)
            .setMinTrackingConfidence(0.5f)
            .setOutputFaceBlendshapes(true)
            .setOutputFacialTransformationMatrixes(true)
            .build()
        landmarker = FaceLandmarker.createFromOptions(context, options)
    }

    fun detect(bitmap: Bitmap): FaceFrame {
        val capturedAt = SystemClock.elapsedRealtime()
        val timestamp = max(capturedAt, previousTimestamp + 1)
        previousTimestamp = timestamp
        val image = BitmapImageBuilder(bitmap).build()
        try {
            val result = landmarker.detectForVideo(image, timestamp)
            if (result.faceLandmarks().isEmpty()) return FaceFrame(capturedAtMs = capturedAt)
            val matrices = result.facialTransformationMatrixes().orElse(emptyList())
            val blendshapes = result.faceBlendshapes().orElse(emptyList()).firstOrNull()
                ?.associate { it.categoryName() to it.score().toDouble().coerceIn(0.0, 1.0) }
                ?: return FaceFrame(capturedAtMs = capturedAt)
            if (matrices.isEmpty()) return FaceFrame(capturedAtMs = capturedAt)
            fun score(key: String) = blendshapes[key] ?: 0.0
            return FaceFrame(
                tracking = true,
                // Tasks does not expose a face-presence score; this is binary availability.
                confidence = 1.0,
                head = Pose.fromColumnMajor(matrices[0]),
                eyeLeft = 1.0 - score("eyeBlinkLeft"),
                eyeRight = 1.0 - score("eyeBlinkRight"),
                mouthOpen = score("jawOpen"),
                mouthSmile = (score("mouthSmileLeft") + score("mouthSmileRight")) * 0.5,
                capturedAtMs = capturedAt
            )
        } finally {
            image.close()
        }
    }

    override fun close() { landmarker.close() }
}
