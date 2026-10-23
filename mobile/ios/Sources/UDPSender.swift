import Foundation
import Network

/// All socket state belongs to `queue`. Camera frames contain numbers only.
final class UDPSender {
    var onStatus: ((String) -> Void)?
    var onReady: (() -> Void)?
    var onFailure: ((String) -> Void)?

    private let queue = DispatchQueue(label: "org.spriteface.phone.udp")
    private var connection: NWConnection?
    private var pairing: Pairing?
    private var helloTimer: DispatchSourceTimer?
    private var handshakeDeadline: TimeInterval = 0
    private var receiverReady = false
    private var cameraStarted = false
    private var clientChallenge = ""
    private var serverChallenge = ""
    private var sequence: UInt32 = 0
    private let encoder = JSONEncoder()

    private struct Hello: Encodable {
        let v = 1
        let type = "hello"
        let token: String
        let client: String
    }
    private struct Ready: Decodable {
        let v: Int
        let type: String
        let client: String
        let server: String
    }

    func connect(_ pairing: Pairing) {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.disconnectOnQueue()
            self.pairing = pairing
            self.sequence = 0
            self.clientChallenge = SecurePacket.challenge()
            let connection = NWConnection(host: NWEndpoint.Host(pairing.host),
                                          port: NWEndpoint.Port(rawValue: pairing.port)!, using: .udp)
            self.connection = connection
            connection.stateUpdateHandler = { [weak self, weak connection] state in
                guard let self = self, let connection = connection, self.connection === connection else { return }
                switch state {
                case .ready:
                    self.status("PCの応答を待っています…")
                    self.receive(on: connection)
                    self.beginHandshake(on: connection)
                case .waiting:
                    self.status("ネットワークを待っています。ローカルネットワーク権限とWi-Fiを確認してください。")
                case .failed(let error):
                    self.fail("PC接続に失敗しました: \(error.localizedDescription)")
                default: break
                }
            }
            self.status("PCに接続しています…")
            connection.start(queue: self.queue)
        }
    }

    func send(_ frame: FaceFrame) {
        queue.async { [weak self] in
            guard let self = self, self.receiverReady, !self.serverChallenge.isEmpty,
                  let connection = self.connection, let pairing = self.pairing else { return }
            var frame = frame
            frame.token = pairing.token
            frame.client = self.clientChallenge
            frame.server = self.serverChallenge
            frame.seq = self.sequence
            self.sequence &+= 1
            guard let plain = try? self.encoder.encode(frame),
                  let data = try? SecurePacket.seal(plain, token: pairing.token) else { return }
            self.send(data, on: connection)
        }
    }

    /// Send tracking=false before closing. The PC also has a stale-frame timeout.
    func disconnect() {
        queue.async { [weak self] in
            guard let self = self else { return }
            guard self.receiverReady, let pairing = self.pairing, let connection = self.connection else {
                self.disconnectOnQueue()
                return
            }
            var neutral = FaceFrame.neutral(token: pairing.token)
            neutral.seq = self.sequence
            neutral.client = self.clientChallenge
            neutral.server = self.serverChallenge
            self.receiverReady = false
            self.helloTimer?.cancel()
            self.helloTimer = nil
            let data = (try? self.encoder.encode(neutral)).flatMap {
                try? SecurePacket.seal($0, token: pairing.token)
            }
            // Keep a strong reference until the final datagram has reached the network stack.
            connection.send(content: data, completion: .contentProcessed { [weak self, connection] _ in
                connection.cancel()
                guard let self = self, self.connection === connection else { return }
                self.disconnectOnQueue()
            })
        }
    }

    private func beginHandshake(on connection: NWConnection) {
        guard let pairing = pairing else { return }
        handshakeDeadline = ProcessInfo.processInfo.systemUptime + 10
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.schedule(deadline: .now(), repeating: .seconds(1))
        timer.setEventHandler { [weak self, weak connection] in
            guard let self = self, let connection = connection, self.connection === connection else { return }
            if !self.receiverReady && !self.cameraStarted && ProcessInfo.processInfo.systemUptime > self.handshakeDeadline {
                self.fail("PCの応答がありません。接続先・トークン・PCの入力設定・ファイアウォールを確認してください。")
                return
            }
            if let plain = try? self.encoder.encode(Hello(token: pairing.token, client: self.clientChallenge)),
               let data = try? SecurePacket.seal(plain, token: pairing.token) {
                self.send(data, on: connection)
            }
        }
        helloTimer?.cancel()
        helloTimer = timer
        timer.resume()
    }

    private func receive(on connection: NWConnection) {
        // Connected unicast UDP only accepts replies from the configured PC endpoint.
        connection.receiveMessage { [weak self, weak connection] data, _, _, error in
            guard let self = self, let connection = connection, self.connection === connection else { return }
            if let error = error {
                self.fail("PCからの応答受信に失敗しました: \(error.localizedDescription)")
                return
            }
            if let data = data, data.count <= 1024, let pairing = self.pairing,
               let plain = try? SecurePacket.open(data, token: pairing.token),
               let ready = try? JSONDecoder().decode(Ready.self, from: plain),
               ready.v == 1, ready.client == self.clientChallenge {
                if ready.type == "ready" && SecurePacket.validChallenge(ready.server) {
                    if self.serverChallenge != ready.server { self.sequence = 0 }
                    self.serverChallenge = ready.server
                    if !self.receiverReady { self.status("PCに接続しました。カメラを準備しています…") }
                    self.receiverReady = true
                    if !self.cameraStarted {
                        self.cameraStarted = true
                        DispatchQueue.main.async { [weak self] in self?.onReady?() }
                    }
                } else if ready.type == "busy" {
                    self.receiverReady = false
                    self.serverChallenge = ""
                }
            }
            self.receive(on: connection)
        }
    }

    private func send(_ data: Data, on connection: NWConnection) {
        connection.send(content: data, completion: .contentProcessed { [weak self, weak connection] error in
            guard let self = self, let connection = connection, self.connection === connection else { return }
            if let error = error { self.fail("顔データを送信できません: \(error.localizedDescription)") }
        })
    }

    private func disconnectOnQueue() {
        receiverReady = false
        cameraStarted = false
        clientChallenge = ""
        serverChallenge = ""
        helloTimer?.cancel()
        helloTimer = nil
        connection?.stateUpdateHandler = nil
        connection?.cancel()
        connection = nil
        pairing = nil
    }

    private func fail(_ message: String) {
        disconnectOnQueue()
        DispatchQueue.main.async { [weak self] in self?.onFailure?(message) }
    }

    private func status(_ message: String) {
        DispatchQueue.main.async { [weak self] in self?.onStatus?(message) }
    }
}
