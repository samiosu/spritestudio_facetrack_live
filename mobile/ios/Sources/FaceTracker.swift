import AVFoundation
import MediaPipeTasksVision
import UIKit

final class FaceTracker: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, FaceLandmarkerLiveStreamDelegate {
    let session = AVCaptureSession()
    var onFrame: ((FaceFrame) -> Void)?
    var onStatus: ((String) -> Void)?
    var onFailure: ((String) -> Void)?

    private let queue = DispatchQueue(label: "org.spriteface.phone.camera")
    private var landmarker: FaceLandmarker?
    private var active = false
    private var configured = false
    private var lastTimestamp = 0
    private var permissionGeneration = 0
    private var observers: [NSObjectProtocol] = []

    override init() {
        super.init()
        let center = NotificationCenter.default
        observers.append(center.addObserver(forName: .AVCaptureSessionRuntimeError, object: session, queue: nil) { [weak self] notification in
            let error = notification.userInfo?[AVCaptureSessionErrorKey] as? Error
            self?.queue.async { [weak self] in self?.fail("カメラエラー: \(error?.localizedDescription ?? "不明なエラー")") }
        })
        observers.append(center.addObserver(forName: .AVCaptureSessionWasInterrupted, object: session, queue: nil) { [weak self] _ in
            self?.queue.async { [weak self] in self?.fail("カメラが中断されました。ほかのカメラ利用を終了し、再接続してください。") }
        })
    }

    deinit { observers.forEach { NotificationCenter.default.removeObserver($0) } }

    func start() {
        // Generation prevents a delayed permission response from restarting a stopped session.
        queue.async { [weak self] in
            guard let self = self else { return }
            self.permissionGeneration += 1
            let generation = self.permissionGeneration
            switch AVCaptureDevice.authorizationStatus(for: .video) {
            case .authorized: self.startOnQueue()
            case .notDetermined:
                AVCaptureDevice.requestAccess(for: .video) { [weak self] allowed in
                    self?.queue.async { [weak self] in
                        guard let self = self, self.permissionGeneration == generation else { return }
                        if allowed { self.startOnQueue() }
                        else { self.fail("カメラ権限がありません。iOSの設定から許可してください。") }
                    }
                }
            default: self.fail("カメラ権限がありません。iOSの設定から許可してください。")
            }
        }
    }

    func stop() {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.permissionGeneration += 1
            self.stopOnQueue()
        }
    }

    private func startOnQueue() {
        guard !active else { return }
        guard let model = Bundle.main.path(forResource: "face_landmarker", ofType: "task") else {
            fail("face_landmarker.task がありません。READMEのモデル取得手順を実行してアプリを再ビルドしてください。")
            return
        }
        do {
            let options = FaceLandmarkerOptions()
            options.baseOptions.modelAssetPath = model
            options.runningMode = .liveStream
            options.numFaces = 1
            options.minFaceDetectionConfidence = 0.5
            options.minFacePresenceConfidence = 0.5
            options.minTrackingConfidence = 0.5
            options.outputFaceBlendshapes = true
            options.outputFacialTransformationMatrixes = true
            options.faceLandmarkerLiveStreamDelegate = self
            landmarker = try FaceLandmarker(options: options)
            if !configured { try configureCamera() }
            active = true
            lastTimestamp = 0
            session.startRunning()
            DispatchQueue.main.async { [weak self] in self?.onStatus?("顔の動きを送信中。カメラ映像は端末内で処理されます。") }
        } catch { fail("顔解析を開始できません: \(error.localizedDescription)") }
    }

    private func configureCamera() throws {
        guard let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .front) else {
            throw NSError(domain: "SpriteFace", code: 1, userInfo: [NSLocalizedDescriptionKey: "前面カメラが見つかりません。実機を使用してください。"])
        }
        let input = try AVCaptureDeviceInput(device: device)
        session.beginConfiguration()
        defer { session.commitConfiguration() }
        if session.canSetSessionPreset(.vga640x480) { session.sessionPreset = .vga640x480 }
        guard session.canAddInput(input) else {
            throw NSError(domain: "SpriteFace", code: 2, userInfo: [NSLocalizedDescriptionKey: "カメラ入力を追加できません。"])
        }
        session.addInput(input)
        let output = AVCaptureVideoDataOutput()
        output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
        output.alwaysDiscardsLateVideoFrames = true
        output.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(output) else {
            session.removeInput(input)
            throw NSError(domain: "SpriteFace", code: 3, userInfo: [NSLocalizedDescriptionKey: "カメラ出力を追加できません。"])
        }
        session.addOutput(output)
        if let connection = output.connection(with: .video) {
            if connection.isVideoOrientationSupported { connection.videoOrientation = .portrait }
            if connection.isVideoMirroringSupported {
                connection.automaticallyAdjustsVideoMirroring = false
                connection.isVideoMirrored = false
            }
        }
        do {
            try device.lockForConfiguration()
            if device.activeFormat.videoSupportedFrameRateRanges.contains(where: { $0.minFrameRate <= 30 && $0.maxFrameRate >= 30 }) {
                device.activeVideoMinFrameDuration = CMTime(value: 1, timescale: 30)
                device.activeVideoMaxFrameDuration = CMTime(value: 1, timescale: 30)
            }
            device.unlockForConfiguration()
        } catch {
            // Keep the camera's supported default frame rate if configuration is unavailable.
        }
        configured = true
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard active, let landmarker = landmarker else { return }
        do {
            // The data output rotates to portrait and stays unmirrored; only the preview is mirrored.
            let image = try MPImage(sampleBuffer: sampleBuffer, orientation: .up)
            let timestamp = max(lastTimestamp + 1, Int(ProcessInfo.processInfo.systemUptime * 1000))
            lastTimestamp = timestamp
            try landmarker.detectAsync(image: image, timestampInMilliseconds: timestamp)
        } catch { fail("顔解析に失敗しました: \(error.localizedDescription)") }
    }

    func faceLandmarker(_ faceLandmarker: FaceLandmarker, didFinishDetection result: FaceLandmarkerResult?, timestampInMilliseconds: Int, error: Error?) {
        queue.async { [weak self] in
            guard let self = self, self.active, self.landmarker === faceLandmarker else { return }
            if let error = error { self.fail("顔解析に失敗しました: \(error.localizedDescription)"); return }
            guard let result = result, !result.faceLandmarks.isEmpty,
                  let blendshapes = result.faceBlendshapes.first,
                  let matrix = result.facialTransformationMatrixes.first,
                  matrix.rows == 4, matrix.columns == 4 else {
                self.onFrame?(.neutral())
                return
            }
            var scores: [String: Double] = [:]
            for category in blendshapes.categories {
                if let name = category.categoryName { scores[name] = FaceFrame.unit(Double(category.score)) }
            }
            var rotation: [Double] = []
            for row in 0..<3 {
                for column in 0..<3 { rotation.append(Double(matrix.value(atRow: UInt(row), column: UInt(column)))) }
            }
            var frame = FaceFrame.neutral()
            frame.tracking = true
            // FaceLandmarker does not expose a global confidence score. This is presence 1/0.
            frame.confidence = 1
            frame.head = FaceFrame.headFromRotation(rotation)
            frame.eyes.left = FaceFrame.unit(1 - (scores["eyeBlinkLeft"] ?? 0))
            frame.eyes.right = FaceFrame.unit(1 - (scores["eyeBlinkRight"] ?? 0))
            frame.mouth.open = scores["jawOpen"] ?? 0
            frame.mouth.smile = FaceFrame.unit(((scores["mouthSmileLeft"] ?? 0) + (scores["mouthSmileRight"] ?? 0)) / 2)
            self.onFrame?(frame)
        }
    }

    private func stopOnQueue() {
        active = false
        if session.isRunning { session.stopRunning() }
        landmarker = nil
        onFrame?(.neutral())
    }

    private func fail(_ message: String) {
        stopOnQueue()
        DispatchQueue.main.async { [weak self] in self?.onFailure?(message) }
    }
}
