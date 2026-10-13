import AVFoundation
import UIKit

final class CameraPreview: UIView {
    override class var layerClass: AnyClass { AVCaptureVideoPreviewLayer.self }
    var previewLayer: AVCaptureVideoPreviewLayer { layer as! AVCaptureVideoPreviewLayer }

    override func layoutSubviews() {
        super.layoutSubviews()
        guard let connection = previewLayer.connection else { return }
        if connection.isVideoOrientationSupported { connection.videoOrientation = .portrait }
        if connection.isVideoMirroringSupported {
            connection.automaticallyAdjustsVideoMirroring = false
            connection.isVideoMirrored = true
        }
    }
}

final class MainViewController: UIViewController {
    private let tracker = FaceTracker()
    private let sender = UDPSender()
    private let preview = CameraPreview()
    private let uriField = UITextField()
    private let hostField = UITextField()
    private let portField = UITextField()
    private let tokenField = UITextField()
    private let statusLabel = UILabel()
    private let metricsLabel = UILabel()
    private let connectButton = UIButton(type: .system)
    private let scanButton = UIButton(type: .system)
    private var running = false
    private var lastMetricsTime: TimeInterval = 0
    private var backgroundObserver: NSObjectProtocol?

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "SpriteFace Phone"
        view.backgroundColor = .systemBackground
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "ライセンス", style: .plain, target: self, action: #selector(showLicenses))

        let scroll = UIScrollView()
        scroll.keyboardDismissMode = .onDrag
        scroll.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll)
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(stack)
        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 16),
            stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -16),
            stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 16),
            stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -16),
            stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -32)
        ])

        let help = UILabel()
        help.text = "PCでスマホ入力を選び、同じWi-Fiに接続してください。PCの接続QRを読み取るか、接続URI・IP・トークンを入力します。"
        help.numberOfLines = 0
        stack.addArrangedSubview(help)
        setupField(uriField, placeholder: "接続URIを貼り付け（任意）", keyboard: .URL)
        setupField(hostField, placeholder: "PCのIPv4（例: 192.168.1.10）", keyboard: .decimalPad)
        setupField(portField, placeholder: "UDPポート", keyboard: .numberPad)
        portField.text = "11574"
        setupField(tokenField, placeholder: "PCの32桁トークン", keyboard: .asciiCapable)
        tokenField.font = .monospacedSystemFont(ofSize: 14, weight: .regular)
        [uriField, hostField, portField, tokenField].forEach { stack.addArrangedSubview($0) }

        scanButton.setTitle("PCの接続QRを読み取る", for: .normal)
        scanButton.addTarget(self, action: #selector(scanQR), for: .touchUpInside)
        stack.addArrangedSubview(scanButton)
        connectButton.setTitle("接続して顔の送信を開始", for: .normal)
        connectButton.titleLabel?.font = .preferredFont(forTextStyle: .headline)
        connectButton.addTarget(self, action: #selector(toggleConnection), for: .touchUpInside)
        stack.addArrangedSubview(connectButton)

        statusLabel.numberOfLines = 0
        statusLabel.text = "未接続。カメラ映像はPCへ送信しません。"
        stack.addArrangedSubview(statusLabel)
        metricsLabel.font = .monospacedSystemFont(ofSize: 12, weight: .regular)
        metricsLabel.numberOfLines = 0
        stack.addArrangedSubview(metricsLabel)
        preview.backgroundColor = .black
        preview.previewLayer.session = tracker.session
        preview.previewLayer.videoGravity = .resizeAspectFill
        preview.clipsToBounds = true
        preview.layer.cornerRadius = 12
        preview.heightAnchor.constraint(equalToConstant: 280).isActive = true
        stack.addArrangedSubview(preview)

        sender.onStatus = { [weak self] text in
            guard self?.running == true else { return }
            self?.statusLabel.text = text
        }
        sender.onReady = { [weak self] in
            guard self?.running == true else { return }
            self?.tracker.start()
        }
        sender.onFailure = { [weak self] text in self?.stop(message: text) }
        tracker.onStatus = { [weak self] text in
            guard self?.running == true else { return }
            self?.statusLabel.text = text
        }
        tracker.onFailure = { [weak self] text in self?.stop(message: text) }
        tracker.onFrame = { [weak self] frame in
            self?.sender.send(frame)
            DispatchQueue.main.async { [weak self] in
                guard let self = self, self.running else { return }
                let now = ProcessInfo.processInfo.systemUptime
                guard now - self.lastMetricsTime >= 0.2 else { return }
                self.lastMetricsTime = now
                self.metricsLabel.text = frame.tracking
                    ? String(format: "yaw %.2f  pitch %.2f  roll %.2f rad\n目 L %.2f R %.2f  口 %.2f  笑顔 %.2f", frame.head.yaw, frame.head.pitch, frame.head.roll, frame.eyes.left, frame.eyes.right, frame.mouth.open, frame.mouth.smile)
                    : "顔を検出できません。通常表情を送信中。"
            }
        }
        backgroundObserver = NotificationCenter.default.addObserver(forName: UIApplication.didEnterBackgroundNotification, object: nil, queue: .main) { [weak self] _ in
            guard let self = self, self.running else { return }
            self.stop(message: "送信を停止しました。再開するには接続ボタンを押してください。")
        }
    }

    deinit {
        if let observer = backgroundObserver { NotificationCenter.default.removeObserver(observer) }
    }

    private func setupField(_ field: UITextField, placeholder: String, keyboard: UIKeyboardType) {
        field.borderStyle = .roundedRect
        field.placeholder = placeholder
        field.keyboardType = keyboard
        field.autocorrectionType = .no
        field.autocapitalizationType = .none
        field.clearButtonMode = .whileEditing
        field.heightAnchor.constraint(greaterThanOrEqualToConstant: 44).isActive = true
    }

    @objc private func toggleConnection() {
        if running { stop(message: "送信を停止しました。"); return }
        view.endEditing(true)
        do {
            let uri = (uriField.text ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
            let pairing = try uri.isEmpty
                ? Pairing(host: hostField.text ?? "", port: portField.text ?? "", token: tokenField.text ?? "")
                : Pairing(uri: uri)
            hostField.text = pairing.host
            portField.text = String(pairing.port)
            tokenField.text = pairing.token
            // Obtain camera permission before connecting; its system prompt can resign activity.
            if AVCaptureDevice.authorizationStatus(for: .video) == .notDetermined {
                AVCaptureDevice.requestAccess(for: .video) { [weak self] allowed in
                    DispatchQueue.main.async {
                        guard let self = self else { return }
                        if allowed { self.begin(pairing) }
                        else { self.statusLabel.text = "カメラ権限がありません。iOSの設定から許可してください。" }
                    }
                }
            } else if AVCaptureDevice.authorizationStatus(for: .video) == .authorized {
                begin(pairing)
            } else { statusLabel.text = "カメラ権限がありません。iOSの設定から許可してください。" }
        } catch { statusLabel.text = error.localizedDescription }
    }

    private func begin(_ pairing: Pairing) {
        guard UIApplication.shared.applicationState == .active, !running else { return }
        running = true
        setEditing(false)
        UIApplication.shared.isIdleTimerDisabled = true
        sender.connect(pairing)
    }

    private func stop(message: String) {
        running = false
        tracker.stop()
        sender.disconnect()
        UIApplication.shared.isIdleTimerDisabled = false
        setEditing(true)
        statusLabel.text = message
        metricsLabel.text = ""
    }

    private func setEditing(_ enabled: Bool) {
        [uriField, hostField, portField, tokenField].forEach { $0.isEnabled = enabled }
        scanButton.isEnabled = enabled
        connectButton.setTitle(enabled ? "接続して顔の送信を開始" : "送信を停止", for: .normal)
    }

    @objc private func scanQR() {
        let scanner = QRScannerViewController()
        scanner.onCode = { [weak self] code in
            guard let self = self else { return }
            do {
                let pairing = try Pairing(uri: code)
                self.uriField.text = code
                self.hostField.text = pairing.host
                self.portField.text = String(pairing.port)
                self.tokenField.text = pairing.token
                self.statusLabel.text = "接続情報を読み取りました。接続ボタンを押してください。"
            } catch { self.statusLabel.text = error.localizedDescription }
        }
        let navigation = UINavigationController(rootViewController: scanner)
        navigation.modalPresentationStyle = .fullScreen
        present(navigation, animated: true)
    }

    @objc private func showLicenses() {
        let controller = UIViewController()
        controller.title = "ライセンス"
        controller.view.backgroundColor = .systemBackground
        let textView = UITextView(frame: .zero)
        textView.isEditable = false
        textView.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        textView.translatesAutoresizingMaskIntoConstraints = false
        controller.view.addSubview(textView)
        NSLayoutConstraint.activate([
            textView.leadingAnchor.constraint(equalTo: controller.view.safeAreaLayoutGuide.leadingAnchor, constant: 12),
            textView.trailingAnchor.constraint(equalTo: controller.view.safeAreaLayoutGuide.trailingAnchor, constant: -12),
            textView.topAnchor.constraint(equalTo: controller.view.safeAreaLayoutGuide.topAnchor),
            textView.bottomAnchor.constraint(equalTo: controller.view.safeAreaLayoutGuide.bottomAnchor)
        ])
        var sections: [String] = []
        if let url = Bundle.main.url(forResource: "LICENSE", withExtension: nil),
           let text = try? String(contentsOf: url) { sections.append(text) }
        if let url = Bundle.main.url(forResource: "THIRD_PARTY_NOTICES", withExtension: "md"),
           let text = try? String(contentsOf: url) { sections.append(text) }
        if let directory = Bundle.main.resourceURL?.appendingPathComponent("licenses"),
           let urls = try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil) {
            for url in urls.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
                if let text = try? String(contentsOf: url) { sections.append("\n\n=== \(url.lastPathComponent) ===\n\n\(text)") }
            }
        }
        if let url = Bundle.main.url(forResource: "Acknowledgements", withExtension: "plist"),
           let data = try? Data(contentsOf: url),
           let plist = try? PropertyListSerialization.propertyList(from: data, options: [], format: nil),
           let dictionary = plist as? [String: Any], let entries = dictionary["PreferenceSpecifiers"] as? [[String: Any]] {
            for entry in entries {
                if let text = entry["FooterText"] as? String { sections.append("\n\n\(entry["Title"] as? String ?? "Pods")\n\n\(text)") }
            }
        }
        textView.text = sections.isEmpty ? "THIRD_PARTY_NOTICES.md と licenses/ を含めて再ビルドしてください。" : sections.joined(separator: "\n")
        navigationController?.pushViewController(controller, animated: true)
    }
}
