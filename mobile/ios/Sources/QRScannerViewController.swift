import AVFoundation
import UIKit

final class QRScannerViewController: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
    var onCode: ((String) -> Void)?
    private let session = AVCaptureSession()
    private let queue = DispatchQueue(label: "org.spriteface.phone.qr")
    private let previewLayer = AVCaptureVideoPreviewLayer()
    private var shouldRun = false // queue-owned
    private var configured = false // queue-owned
    private var delivered = false // main-thread-owned
    private var observers: [NSObjectProtocol] = []

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "PCの接続QRを読み取り"
        view.backgroundColor = .black
        navigationItem.leftBarButtonItem = UIBarButtonItem(title: "キャンセル", style: .plain, target: self, action: #selector(cancel))
        previewLayer.session = session
        previewLayer.videoGravity = .resizeAspectFill
        view.layer.addSublayer(previewLayer)
        let help = UILabel()
        help.text = "PC画面のSpriteFace接続QRを写してください。"
        help.textColor = .white
        help.backgroundColor = UIColor.black.withAlphaComponent(0.6)
        help.textAlignment = .center
        help.numberOfLines = 0
        help.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(help)
        NSLayoutConstraint.activate([
            help.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            help.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),
            help.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -24),
            help.heightAnchor.constraint(greaterThanOrEqualToConstant: 60)
        ])
        observers.append(NotificationCenter.default.addObserver(forName: UIApplication.didEnterBackgroundNotification, object: nil, queue: .main) { [weak self] _ in
            self?.queue.async { [weak self] in
                guard let self = self else { return }
                self.shouldRun = false
                if self.session.isRunning { self.session.stopRunning() }
            }
        })
        observers.append(NotificationCenter.default.addObserver(forName: UIApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in
            guard let self = self, self.view.window != nil, !self.delivered,
                  AVCaptureDevice.authorizationStatus(for: .video) == .authorized else { return }
            self.queue.async { [weak self] in
                self?.shouldRun = true
                self?.startOnQueue()
            }
        })
    }

    deinit { observers.forEach { NotificationCenter.default.removeObserver($0) } }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        previewLayer.frame = view.bounds
        if let connection = previewLayer.connection, connection.isVideoOrientationSupported {
            connection.videoOrientation = .portrait
        }
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        queue.async { [weak self] in
            guard let self = self else { return }
            self.shouldRun = true
            switch AVCaptureDevice.authorizationStatus(for: .video) {
            case .authorized: self.startOnQueue()
            case .notDetermined:
                AVCaptureDevice.requestAccess(for: .video) { [weak self] allowed in
                    self?.queue.async { [weak self] in
                        guard let self = self, self.shouldRun else { return }
                        if allowed { self.startOnQueue() }
                        else { self.showError("カメラ権限がありません。iOSの設定から許可するか、接続情報を手入力してください。") }
                    }
                }
            default: self.showError("カメラ権限がありません。iOSの設定から許可するか、接続情報を手入力してください。")
            }
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        queue.async { [weak self] in
            self?.shouldRun = false
            if self?.session.isRunning == true { self?.session.stopRunning() }
        }
    }

    private func startOnQueue() {
        guard shouldRun else { return }
        if !configured {
            guard let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back),
                  let input = try? AVCaptureDeviceInput(device: device) else {
                showError("QR読み取り用カメラが見つかりません。接続情報を手入力してください。")
                return
            }
            session.beginConfiguration()
            let output = AVCaptureMetadataOutput()
            guard session.canAddInput(input), session.canAddOutput(output) else {
                session.commitConfiguration()
                showError("QRカメラを準備できません。接続情報を手入力してください。")
                return
            }
            session.addInput(input)
            session.addOutput(output)
            output.setMetadataObjectsDelegate(self, queue: .main)
            guard output.availableMetadataObjectTypes.contains(.qr) else {
                session.removeInput(input)
                session.removeOutput(output)
                session.commitConfiguration()
                showError("QR読み取りに対応していません。接続情報を手入力してください。")
                return
            }
            output.metadataObjectTypes = [.qr]
            session.commitConfiguration()
            configured = true
        }
        session.startRunning()
    }

    func metadataOutput(_ output: AVCaptureMetadataOutput, didOutput metadataObjects: [AVMetadataObject], from connection: AVCaptureConnection) {
        guard !delivered, let code = metadataObjects.compactMap({ ($0 as? AVMetadataMachineReadableCodeObject)?.stringValue }).first else { return }
        delivered = true
        let callback = onCode
        dismiss(animated: true) { callback?(code) }
    }

    private func showError(_ message: String) {
        DispatchQueue.main.async { [weak self] in
            guard let self = self, self.view.window != nil else { return }
            let alert = UIAlertController(title: "QRを読み取れません", message: message, preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "閉じる", style: .default) { [weak self] _ in self?.dismiss(animated: true) })
            self.present(alert, animated: true)
        }
    }

    @objc private func cancel() { dismiss(animated: true) }
}
