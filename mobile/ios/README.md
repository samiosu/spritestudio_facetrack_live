# SpriteFace Phone for iOS

iOS 15以上の前面カメラを持つ実機で、顔向き・まばたき・口・笑顔を端末内で解析し、PCへUDPで送ります。カメラ映像は送信しません。画面のプレビューのみ鏡像表示で、推論には反転していない画像を使用します。

## ビルド（macOS）

XcodeとCommand Line Tools、XcodeGen 2.38以上、Ruby/Bundlerが必要です。MediaPipeのPodは`0.10.21`、CocoaPodsは`1.16.2`に固定しています。シミュレータはカメラ動作の確認には使えません。

1. リポジトリ直下でモデル取得スクリプトを実行します。

```sh
python3 tools/fetch_models.py --target ios
```

`Resources/face_landmarker.task`を取得し、SHA-256を検証します。
2. 以下を実行します。

```sh
cd mobile/ios
bundle install
xcodegen generate
bundle exec pod install --deployment
open SpriteFacePhone.xcworkspace
```

3. Xcodeで`SpriteFacePhone`ターゲットのSigning Teamと一意のBundle Identifierを設定し、実機を選んでRunします。
4. `Podfile.lock`と`Gemfile.lock`を使ってCIで解決した依存を再現します。ライブラリの直接・間接依存とバイナリのライセンスを、配布前にその版で確認してください。

CLIでのビルド確認と単体テストは、署名設定後に行えます。

```sh
xcodebuild -workspace SpriteFacePhone.xcworkspace -scheme SpriteFacePhone -destination 'generic/platform=iOS' build
xcodebuild -workspace SpriteFacePhone.xcworkspace -scheme SpriteFacePhone -destination 'platform=iOS Simulator,name=iPhone 16' test
```

実際にインストールしたSimulator名に合わせてdestinationを変更してください。単体テストは接続情報検証・JSON形式・角度計算を確認します。カメラ・MediaPipe・ローカルネットワークの許可・PCへの送信は実機で確認する必要があります。SimulatorのアプリビルドはGitHub ActionsのmacOS/Xcode環境で確認しました。実機動作は未検証です。

GitHub Actionsの`iOS`ワークフローも生成プロジェクトのSimulatorビルドと単体テストを実行します。初回の依存取得にはネットワークが必要です。`Podfile.lock`・`Gemfile.lock`はmacOS CIで成功した依存解決のartifactから取得しています。CIでは`pod install --deployment`を使用し、Podfileとlockfileの不一致をエラーにします。

## PCへの接続

PCでスマホ入力を開始して表示された接続QRをスキャンするか、接続URIを貼り付けます。QR表示のない環境では、IPv4・UDPポート（既定`11574`）・32桁の16進トークンを手入力します。URIを入力している場合はURIの情報を優先します。両端末は同じLANに接続し、PCの受信ポートをファイアウォールで許可します。

接続ボタンを押し、カメラとローカルネットワークを許可します。`hello`に対する`ready`を受信してからカメラを開始します。10秒以内にPCの応答がない場合はエラーを表示します。ネットワーク許可の初回ダイアログで停止した場合は再度接続してください。`NSLocalNetworkUsageDescription`を含み、マルチキャストやブロードキャストは使用しないため、そのためのentitlementは不要です。

顔が映らないフレームは`tracking:false`、正面・開いた目・閉じた口の通常表情を送ります。停止またはバックグラウンド移行でカメラと送信を終了します。カメラ割り込み・推論エラー・権限拒否・モデル不足も画面に表示します。PC側の受信タイムアウトと合わせて、切断後に表情が固まるのを防ぎます。

`confidence`はFaceLandmarkerが全体の確度を公開していないため、検出あり`1`／なし`0`です。眼の左右は本人基準、眼の値は`1 - eyeBlink`、口は`jawOpen`、笑顔は左右`mouthSmile`の平均です。角度はスケールを取り除いたMediaPipe行列の`Rz(roll) Ry(yaw) Rx(pitch)`分解から計算し、単位はラジアンです。

## ライセンス

自作部分のライセンスはリポジトリ直下の`LICENSE`に従います。MediaPipe Tasks Vision / Tasks CommonはApache-2.0です。モデルはコードと別に管理します。XcodeGenはMIT、CocoaPodsはMITの開発用ツールです。

`project.yml`は根元の`licenses/`と`THIRD_PARTY_NOTICES.md`をバンドルし、CocoaPodsのacknowledgementsをビルド時にコピーします。アプリの「ライセンス」から閲覧できます。配布時は実際のPodsのライセンス・NOTICEとモデルの表示条件を確認し、必要なファイルを含めます。

確認した一次資料:

- [MediaPipeTasksVision 0.10.21のPodspec](https://raw.githubusercontent.com/CocoaPods/Specs/master/Specs/9/9/0/MediaPipeTasksVision/0.10.21/MediaPipeTasksVision.podspec.json)
- [Face Landmarker iOSガイド](https://developers.google.com/edge/mediapipe/solutions/vision/face_landmarker/ios)
- [0.10.21の行列・結果API](https://github.com/google-ai-edge/mediapipe/blob/v0.10.21/mediapipe/tasks/ios/vision/face_landmarker/sources/MPPFaceLandmarkerResult.h)
