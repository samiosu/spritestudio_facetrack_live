package dev.spriteface.companion

import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/** SF2 wire envelope: authenticated magic, random nonce, ciphertext and GCM tag. */
object SecurePacket {
    private val magic = "SF2".toByteArray(Charsets.US_ASCII)
    private val random = SecureRandom()

    fun challenge(): String = ByteArray(16).also(random::nextBytes)
        .joinToString("") { "%02x".format(it.toInt() and 0xff) }

    fun validChallenge(value: String): Boolean = value.matches(Regex("[0-9a-f]{32}"))

    private fun key(token: String): SecretKeySpec {
        require(token.matches(Regex("[0-9a-fA-F]{32}")))
        return SecretKeySpec(token.chunked(2).map { it.toInt(16).toByte() }.toByteArray(), "AES")
    }

    fun seal(json: String, token: String): ByteArray {
        val nonce = ByteArray(12).also(random::nextBytes)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, key(token), GCMParameterSpec(128, nonce))
        cipher.updateAAD(magic)
        return magic + nonce + cipher.doFinal(json.toByteArray(Charsets.UTF_8))
    }

    fun open(packet: ByteArray, length: Int, token: String): String {
        require(length >= magic.size + 12 + 16 && magic.indices.all { packet[it] == magic[it] })
        val nonce = packet.copyOfRange(magic.size, magic.size + 12)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key(token), GCMParameterSpec(128, nonce))
        cipher.updateAAD(magic)
        val plain = cipher.doFinal(packet, magic.size + 12, length - magic.size - 12)
        return String(plain, Charsets.UTF_8)
    }
}
