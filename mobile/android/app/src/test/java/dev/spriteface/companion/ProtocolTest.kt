package dev.spriteface.companion

import org.junit.Assert.*
import org.junit.Test
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

class ProtocolTest {
    private val token = "0123456789abcdef0123456789abcdef"

    @Test fun validPairUri() {
        assertEquals(PairConfig("192.168.1.5", 11574, token),
            PairConfig.parse("spriteface://pair?v=1&host=192.168.1.5&port=11574&token=$token"))
    }

    @Test fun rejectUntrustedPairValues() {
        for (suffix in listOf("&host=10.0.0.1", "&extra=value", "#fragment")) {
            assertThrows(IllegalArgumentException::class.java) {
                PairConfig.parse("spriteface://pair?v=1&host=192.168.1.5&port=11574&token=$token$suffix")
            }
        }
        for (host in listOf("example.org", "1.2.3.256", "01.2.3.4", "239.1.1.1", "0.0.0.0", "0.1.2.3")) {
            assertThrows(IllegalArgumentException::class.java) { PairConfig(host, 11574, token) }
        }
    }

    @Test fun yawUsesColumnMajorRadians() {
        val yaw = PI / 6
        val c = cos(yaw).toFloat()
        val s = sin(yaw).toFloat()
        val matrix = floatArrayOf(c, 0f, -s, 0f, 0f, 1f, 0f, 0f, s, 0f, c, 0f, 0f, 0f, 0f, 1f)
        val pose = Pose.fromColumnMajor(matrix)
        assertEquals(yaw, pose.yaw, 1e-6)
        assertEquals(0.0, pose.pitch, 1e-6)
        assertEquals(0.0, pose.roll, 1e-6)
    }

    @Test fun frameBoundsAndUint32Sequence() {
        val value = FaceFrame(true, Double.NaN, Pose(4.0, -4.0, 0.1), 2.0, -1.0).encode(token, 0x1_0000_0001L)
        assertTrue(value.contains("\"seq\":1"))
        assertTrue(value.contains("\"confidence\":0.0"))
        assertTrue(value.contains("\"left\":1.0,\"right\":0.0"))
        assertFalse(value.contains("NaN"))
        assertFalse(value.contains("Infinity"))
    }

    @Test fun pitchAndRollSurviveMatrixScale() {
        val angle = PI / 4
        val c = (cos(angle) * 2).toFloat()
        val s = (sin(angle) * 2).toFloat()
        val pitchMatrix = floatArrayOf(2f, 0f, 0f, 0f, 0f, c, s, 0f, 0f, -s, c, 0f, 0f, 0f, 0f, 1f)
        val rollMatrix = floatArrayOf(c, s, 0f, 0f, -s, c, 0f, 0f, 0f, 0f, 2f, 0f, 0f, 0f, 0f, 1f)
        assertEquals(angle, Pose.fromColumnMajor(pitchMatrix).pitch, 1e-6)
        assertEquals(angle, Pose.fromColumnMajor(rollMatrix).roll, 1e-6)
    }

    @Test fun combinedPoseSurvivesNonUniformAndLargeScale() {
        val yaw = 0.3
        val pitch = -0.4
        val roll = 0.2
        val cy = cos(yaw)
        val sy = sin(yaw)
        val cp = cos(pitch)
        val sp = sin(pitch)
        val cr = cos(roll)
        val sr = sin(roll)
        val rotation = doubleArrayOf(
            cr * cy, sr * cy, -sy,
            cr * sy * sp - sr * cp, sr * sy * sp + cr * cp, cy * sp,
            cr * sy * cp + sr * sp, sr * sy * cp - cr * sp, cy * cp
        )
        for (scales in listOf(doubleArrayOf(2.0, 7.0, 0.2), doubleArrayOf(1e30, 1e25, 1e20))) {
            val matrix = FloatArray(16)
            for (column in 0..2) for (row in 0..2) {
                matrix[column * 4 + row] = (rotation[column * 3 + row] * scales[column]).toFloat()
            }
            matrix[15] = 1f
            val pose = Pose.fromColumnMajor(matrix)
            assertEquals(yaw, pose.yaw, 1e-6)
            assertEquals(pitch, pose.pitch, 1e-6)
            assertEquals(roll, pose.roll, 1e-6)
        }
    }

    @Test fun rejectDegenerateRotationColumns() {
        for (column in 0..2) {
            val matrix = floatArrayOf(1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f)
            matrix[column * 4 + column] = 0f
            assertThrows(IllegalArgumentException::class.java) { Pose.fromColumnMajor(matrix) }
        }
    }
}
