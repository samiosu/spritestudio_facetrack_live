package dev.spriteface.companion

import android.os.SystemClock
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketTimeoutException
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference

/** Unicast UDP only; no camera images leave this device. */
class UdpFaceClient(private val config: PairConfig, private val status: (String) -> Unit) : AutoCloseable {
    private val latest = AtomicReference(FaceFrame())
    private val stopped = AtomicBoolean(false)
    private val ready = AtomicBoolean(false)
    private val sender = Executors.newSingleThreadScheduledExecutor()
    private var socket: DatagramSocket? = null
    private val sequence = AtomicLong(0L)
    private var nextHelloMs = 0L
    private var announcedWaiting = false

    fun update(frame: FaceFrame) { latest.set(frame) }

    fun start() {
        sender.execute {
            try {
                val udp = synchronized(this) {
                    if (stopped.get()) return@execute
                    DatagramSocket().also {
                        socket = it
                        it.connect(InetAddress.getByName(config.host), config.port)
                        it.soTimeout = 150
                    }
                }
                Thread({ receive(udp) }, "SpriteFace-UDP-receiver").start()
                sender.scheduleAtFixedRate({ sendLatest() }, 0, 33, TimeUnit.MILLISECONDS)
            } catch (error: Exception) {
                if (!stopped.get()) status("接続できません: ${error.javaClass.simpleName}")
                close()
            }
        }
    }

    private fun receive(udp: DatagramSocket) {
        val bytes = ByteArray(1024)
        while (!stopped.get()) {
            try {
                val packet = DatagramPacket(bytes, bytes.size)
                udp.receive(packet)
                val json = JSONObject(String(packet.data, 0, packet.length, Charsets.UTF_8))
                if (json.optInt("v") == 1 && json.optString("type") == "ready" && ready.compareAndSet(false, true)) {
                    status("PCと接続しました。顔をカメラに向けてください")
                }
            } catch (_: SocketTimeoutException) {
                // Expected while idle or waiting for the PC.
            } catch (_: Exception) {
                if (stopped.get() || udp.isClosed) break
            }
        }
    }

    private fun sendLatest() {
        if (stopped.get()) return
        try {
            val now = SystemClock.elapsedRealtime()
            if (!ready.get()) {
                if (now >= nextHelloMs) {
                    send("""{"v":1,"type":"hello","token":"${config.token.lowercase()}"}""")
                    nextHelloMs = now + 1000
                    if (!announcedWaiting) {
                        status("PCの応答を待っています。同じLANと受信設定を確認してください")
                        announcedWaiting = true
                    }
                }
                return
            }
            val frame = latest.get().let { if (now - it.capturedAtMs <= 500) it else FaceFrame() }
            send(frame.encode(config.token, sequence.getAndIncrement()))
        } catch (_: Exception) {
            // A temporarily unreachable receiver is retried, without blocking the camera.
            if (!stopped.get()) {
                ready.set(false)
                announcedWaiting = false
            }
        }
    }

    @Synchronized private fun send(json: String) {
        val bytes = json.toByteArray(Charsets.UTF_8)
        socket?.send(DatagramPacket(bytes, bytes.size))
    }

    override fun close() {
        if (!stopped.compareAndSet(false, true)) return
        // Activity stops on the UI thread. Keep even the final packet on the sender worker.
        sender.execute {
            synchronized(this) {
                try {
                    if (ready.get()) send(FaceFrame().encode(config.token, sequence.getAndIncrement()))
                } catch (_: Exception) { /* The peer can use its stale-frame timeout. */ }
                socket?.close()
                socket = null
            }
        }
        // Previously submitted cleanup runs; recurring sends are canceled by shutdown.
        sender.shutdown()
    }
}
