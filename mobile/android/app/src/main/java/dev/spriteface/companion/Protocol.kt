package dev.spriteface.companion

import java.net.URI
import java.net.URLDecoder
import kotlin.math.PI
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.sqrt

/** No Android dependency: validation and pose math run in local unit tests. */
data class PairConfig(val host: String, val port: Int, val token: String) {
    init {
        val octets = host.split('.')
        require(octets.size == 4 && octets.all {
            it.isNotEmpty() && it.all(Char::isDigit) && it.length <= 3 &&
                it.toIntOrNull() in 0..255 && (it == "0" || !it.startsWith('0'))
        }) { "PCのIPv4アドレスを入力してください" }
        require(octets[0].toInt() in 1..223) { "ユニキャストIPv4アドレスが必要です" }
        require(port in 1..65535) { "ポートは1〜65535を指定してください" }
        require(token.matches(Regex("[0-9a-fA-F]{32}"))) { "トークンは32桁の16進数です" }
    }

    companion object {
        fun parse(value: String): PairConfig {
            val uri = URI(value.trim())
            require(uri.scheme == "spriteface" && uri.host == "pair" &&
                uri.userInfo == null && uri.port == -1 &&
                uri.path.isNullOrEmpty() && uri.fragment == null) { "SpriteFaceの接続QRが必要です" }
            val query = uri.rawQuery?.split('&') ?: error("接続情報がありません")
            val values = mutableMapOf<String, String>()
            for (field in query) {
                val pair = field.split('=', limit = 2)
                require(pair.size == 2) { "接続情報が不正です" }
                val key = URLDecoder.decode(pair[0], "UTF-8")
                require(key in setOf("v", "host", "port", "token") && !values.containsKey(key)) {
                    "接続情報の項目が不正です"
                }
                values[key] = URLDecoder.decode(pair[1], "UTF-8")
            }
            require(values["v"] == "2") { "未対応のプロトコルです" }
            return PairConfig(
                values["host"] ?: error("PCアドレスがありません"),
                values["port"]?.toIntOrNull() ?: error("ポートが不正です"),
                values["token"]?.lowercase() ?: error("トークンがありません")
            )
        }
    }
}

data class Pose(val yaw: Double = 0.0, val pitch: Double = 0.0, val roll: Double = 0.0) {
    companion object {
        /** MediaPipe column-major matrix; R = Rz(roll) Ry(yaw) Rx(pitch). */
        fun fromColumnMajor(matrix: FloatArray): Pose {
            require(matrix.size == 16 && matrix.all { it.isFinite() }) { "Invalid face matrix" }
            fun columnScale(offset: Int): Double {
                val x = matrix[offset].toDouble()
                val y = matrix[offset + 1].toDouble()
                val z = matrix[offset + 2].toDouble()
                val scale = sqrt(x * x + y * y + z * z)
                require(scale > 1e-8) { "Degenerate face matrix" }
                return scale
            }
            val scale0 = columnScale(0)
            val scale1 = columnScale(4)
            val scale2 = columnScale(8)
            val yaw = asin((-matrix[2].toDouble() / scale0).coerceIn(-1.0, 1.0))
            return Pose(yaw, atan2(matrix[6].toDouble() / scale1, matrix[10].toDouble() / scale2),
                atan2(matrix[1].toDouble() / scale0, matrix[0].toDouble() / scale0))
        }
    }
}

data class FaceFrame(
    val tracking: Boolean = false,
    val confidence: Double = 0.0,
    val head: Pose = Pose(),
    val eyeLeft: Double = 1.0,
    val eyeRight: Double = 1.0,
    val mouthOpen: Double = 0.0,
    val mouthSmile: Double = 0.0,
    val capturedAtMs: Long = 0L
) {
    fun encode(token: String, seq: Long): String {
        require(token.matches(Regex("[0-9a-fA-F]{32}")))
        fun bounded(value: Double, low: Double, high: Double): Double =
            if (value.isFinite()) value.coerceIn(low, high) else 0.0
        return """{"v":1,"type":"frame","token":"${token.lowercase()}","seq":${seq and 0xffff_ffffL},"tracking":$tracking,"confidence":${bounded(confidence, 0.0, 1.0)},"head":{"yaw":${bounded(head.yaw, -PI, PI)},"pitch":${bounded(head.pitch, -PI, PI)},"roll":${bounded(head.roll, -PI, PI)}},"eyes":{"left":${bounded(eyeLeft, 0.0, 1.0)},"right":${bounded(eyeRight, 0.0, 1.0)}},"mouth":{"open":${bounded(mouthOpen, 0.0, 1.0)},"smile":${bounded(mouthSmile, 0.0, 1.0)}}"""
    }
}
