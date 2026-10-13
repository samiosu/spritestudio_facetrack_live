package dev.spriteface.companion

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.Bundle
import android.os.SystemClock
import android.text.InputType
import android.util.Size
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.common.HybridBinarizer
import java.nio.ByteBuffer
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : ComponentActivity() {
    private lateinit var preview: PreviewView
    private lateinit var host: EditText
    private lateinit var port: EditText
    private lateinit var token: EditText
    private lateinit var info: TextView
    private lateinit var scanButton: Button
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private val analysisExecutor = Executors.newSingleThreadExecutor()
    private val streaming = AtomicBoolean(false)
    private val scanning = AtomicBoolean(false)
    @Volatile private var client: UdpFaceClient? = null
    private var tracker: FaceTracker? = null // analysisExecutor only
    private var cameraProvider: ProcessCameraProvider? = null
    private var resumed = false
    private var cameraError = false
    private var lastAnalysisAt = 0L
    private val qrReader = MultiFormatReader().apply {
        setHints(mapOf(DecodeHintType.POSSIBLE_FORMATS to listOf(com.google.zxing.BarcodeFormat.QR_CODE)))
    }
    private val requestCamera = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) loadCamera() else info.text = "カメラ権限が必要です。設定から許可してください"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        makeUi()
        acceptPairIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        acceptPairIntent(intent)
    }

    private fun makeUi() {
        fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(12), dp(12), dp(12), dp(12))
            ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
                val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.ime())
                view.setPadding(dp(12) + bars.left, dp(12) + bars.top, dp(12) + bars.right, dp(12) + bars.bottom)
                insets
            }
        }
        preview = PreviewView(this).apply {
            implementationMode = PreviewView.ImplementationMode.COMPATIBLE
        }
        root.addView(preview, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        val controls = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        fun label(value: String) { controls.addView(TextView(this).apply { text = value }) }
        fun field(hintText: String, initial: String = "", input: Int = InputType.TYPE_CLASS_TEXT): EditText {
            return EditText(this).apply {
                hint = hintText
                setText(initial)
                inputType = input
                setSingleLine()
                controls.addView(this)
            }
        }
        label("SpriteFace · スマホで顔を取得")
        host = field("PCのIPv4アドレス（例 192.168.1.10）", input = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI)
        port = field("UDPポート", "11574", InputType.TYPE_CLASS_NUMBER)
        token = field("PCに表示された32桁トークン", input = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD)
        token.importantForAutofill = EditText.IMPORTANT_FOR_AUTOFILL_NO
        scanButton = Button(this).apply {
            text = "接続QRを撮影（背面カメラ）"
            setOnClickListener {
                if (streaming.get()) stopStreaming()
                val scan = !scanning.get()
                scanning.set(scan)
                preview.keepScreenOn = scan
                text = if (scan) "QR撮影をやめる" else "接続QRを撮影（背面カメラ）"
                info.text = if (scan) "PCの接続QRをカメラに向けてください" else "接続先を入力して開始してください"
                bindCamera()
            }
        }
        controls.addView(scanButton)
        val buttons = LinearLayout(this)
        startButton = Button(this).apply {
            text = "顔の送信を開始"
            setOnClickListener { startStreaming() }
        }
        stopButton = Button(this).apply {
            text = "停止"
            isEnabled = false
            setOnClickListener { stopStreaming() }
        }
        buttons.addView(startButton, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        buttons.addView(stopButton, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        controls.addView(buttons)
        info = TextView(this).apply { text = "PCと同じLANに接続してください。画像はスマホ内で処理します" }
        controls.addView(info)
        root.addView(ScrollView(this).apply { addView(controls) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(340)))
        setContentView(root)
    }

    private fun acceptPairIntent(value: Intent) {
        val uri = value.data?.toString() ?: return
        try {
            fillPair(PairConfig.parse(uri))
        } catch (error: Exception) {
            info.text = error.message ?: "接続情報が不正です"
        }
    }

    private fun fillPair(pair: PairConfig) {
        if (streaming.get()) stopStreaming()
        host.setText(pair.host)
        port.setText(pair.port.toString())
        token.setText(pair.token)
        scanning.set(false)
        preview.keepScreenOn = false
        scanButton.text = "接続QRを撮影（背面カメラ）"
        info.text = "接続情報を読み込みました。開始すると顔の数値をPCへ送信します"
        bindCamera()
    }

    private fun startStreaming() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestCamera.launch(Manifest.permission.CAMERA)
            return
        }
        val pair = try {
            PairConfig(host.text.toString().trim(), port.text.toString().trim().toInt(), token.text.toString().trim().lowercase())
        } catch (error: Exception) {
            info.text = error.message ?: "接続先を確認してください"
            return
        }
        if (cameraError || cameraProvider == null) {
            info.text = "カメラの準備ができていません"
            return
        }
        if (assets.list("")?.contains("face_landmarker.task") != true) {
            info.text = "顔モデルがありません。READMEのモデル取得手順でAPKをビルドしてください"
            return
        }
        stopStreaming()
        scanning.set(false)
        streaming.set(true)
        preview.keepScreenOn = true
        host.isEnabled = false
        port.isEnabled = false
        token.isEnabled = false
        startButton.isEnabled = false
        stopButton.isEnabled = true
        scanButton.text = "接続QRを撮影（背面カメラ）"
        val udp = UdpFaceClient(pair) { message ->
            runOnUiThread { if (streaming.get()) info.text = message }
        }
        client = udp
        udp.start()
        bindCamera()
    }

    private fun stopStreaming() {
        streaming.set(false)
        preview.keepScreenOn = false
        host.isEnabled = true
        port.isEnabled = true
        token.isEnabled = true
        client?.close()
        client = null
        analysisExecutor.execute {
            tracker?.close()
            tracker = null
        }
        startButton.isEnabled = true
        stopButton.isEnabled = false
        info.text = "停止しました"
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            loadCamera()
        } else requestCamera.launch(Manifest.permission.CAMERA)
    }

    override fun onStop() {
        resumed = false
        scanning.set(false)
        stopStreaming()
        cameraProvider?.unbindAll()
        super.onStop()
    }

    override fun onDestroy() {
        analysisExecutor.shutdown()
        super.onDestroy()
    }

    private fun loadCamera() {
        val future = ProcessCameraProvider.getInstance(this)
        future.addListener({
            try {
                cameraProvider = future.get()
                bindCamera()
            } catch (_: Exception) {
                cameraError = true
                info.text = "カメラを開けません。別のアプリが使用中か確認してください"
            }
        }, ContextCompat.getMainExecutor(this))
    }

    @Suppress("DEPRECATION")
    private fun bindCamera() {
        val provider = cameraProvider ?: return
        if (!resumed) return
        try {
            val preferred = if (scanning.get()) CameraSelector.DEFAULT_BACK_CAMERA else CameraSelector.DEFAULT_FRONT_CAMERA
            val fallback = if (scanning.get()) CameraSelector.DEFAULT_FRONT_CAMERA else CameraSelector.DEFAULT_BACK_CAMERA
            val selector = if (provider.hasCamera(preferred)) preferred else fallback
            require(provider.hasCamera(selector)) { "カメラが見つかりません" }
            provider.unbindAll()
            val cameraPreview = Preview.Builder().build().also { it.surfaceProvider = preview.surfaceProvider }
            val analysis = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
                .setTargetResolution(Size(640, 480))
                .build()
            analysis.setAnalyzer(analysisExecutor) { image -> analyze(image) }
            provider.bindToLifecycle(this, selector, cameraPreview, analysis)
            cameraError = false
        } catch (_: Exception) {
            cameraError = true
            if (streaming.get()) stopStreaming()
            info.text = "カメラを開けません。権限とカメラの使用状態を確認してください"
        }
    }

    private fun analyze(image: ImageProxy) {
        var bitmap: Bitmap? = null
        try {
            val now = SystemClock.elapsedRealtime()
            if ((!scanning.get() && !streaming.get()) || now - lastAnalysisAt < 33) return
            lastAnalysisAt = now
            bitmap = rgbaBitmap(image)
            if (scanning.get()) {
                val pixels = IntArray(bitmap.width * bitmap.height)
                bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
                try {
                    val source = RGBLuminanceSource(bitmap.width, bitmap.height, pixels)
                    val value = qrReader.decodeWithState(BinaryBitmap(HybridBinarizer(source))).text
                    val pair = PairConfig.parse(value)
                    if (scanning.compareAndSet(true, false)) runOnUiThread { fillPair(pair) }
                } catch (_: Exception) {
                    // Continue scanning until a valid SpriteFace QR is seen.
                } finally { qrReader.reset() }
            } else if (streaming.get()) {
                if (tracker == null) tracker = FaceTracker(applicationContext)
                val frame = tracker!!.detect(bitmap)
                client?.update(frame)
            }
        } catch (error: Exception) {
            client?.update(FaceFrame(capturedAtMs = SystemClock.elapsedRealtime()))
            runOnUiThread {
                if (streaming.get()) {
                    stopStreaming()
                    info.text = "顔の解析を停止しました: ${error.message ?: error.javaClass.simpleName}"
                }
            }
        } finally {
            bitmap?.let { if (!it.isRecycled) it.recycle() }
            image.close()
        }
    }

    private fun rgbaBitmap(image: ImageProxy): Bitmap {
        val plane = image.planes[0]
        require(plane.pixelStride == 4) { "Unexpected RGBA camera format" }
        val raw = Bitmap.createBitmap(image.width, image.height, Bitmap.Config.ARGB_8888)
        val rowBytes = image.width * 4
        val source = plane.buffer.duplicate()
        val pixels = if (plane.rowStride == rowBytes) {
            source.apply { rewind() }
        } else {
            val packed = ByteBuffer.allocate(rowBytes * image.height)
            val row = ByteArray(rowBytes)
            for (y in 0 until image.height) {
                source.position(y * plane.rowStride)
                source.get(row)
                packed.put(row)
            }
            packed.apply { rewind() }
        }
        raw.copyPixelsFromBuffer(pixels)
        val rotation = Matrix().apply { postRotate(image.imageInfo.rotationDegrees.toFloat()) }
        val result = Bitmap.createBitmap(raw, 0, 0, image.width, image.height, rotation, false)
        if (result !== raw) raw.recycle()
        return result
    }
}
