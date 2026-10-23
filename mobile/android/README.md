# SpriteFace Android companion

Android 8.0以降のスマホで顔の動きを取得し、同じLANのPCへ数値だけを送信します。
CameraX、MediaPipe Tasks Face Landmarker、ZXing coreを使ったネイティブアプリです。
Unity、WebView、Composeは使用していません。

## ビルド

必要なものはJDK 17、Android SDK Platform 35、Build Tools 35.0.0です。
Android Studioでこのディレクトリを開くか、Android SDKのパスを`ANDROID_HOME`に設定します。
SDKやモデルはリポジトリに含めません。リポジトリのルートでモデルを取得してからビルドします。

```sh
python3 tools/fetch_models.py --target android
cd mobile/android
./gradlew testDebugUnitTest assembleDebug
```

モデルは`app/src/main/assets/face_landmarker.task`に配置されます。
出力APKは`app/build/outputs/apk/debug/app-debug.apk`です。
USBデバッグを有効にした実機に`adb install -r app/build/outputs/apk/debug/app-debug.apk`でインストールできます。
モデルがないAPKもビルドできますが、顔の送信開始時に不足を表示します。

公開版の作成には別途署名キーを設定してください。署名キーをGitHubへコミットしないでください。
Gradleの`syncThirdPartyLicenses`がルートの`LICENSE`、`THIRD_PARTY_NOTICES.md`、`licenses/`を
APKの`assets/third_party_licenses/`へ同梱します。依存ライブラリ内のライセンス表示も残します。

## 接続

1. PCのSpriteFaceでLAN受信を有効にし、PCのIPv4アドレス、UDPポート、トークンを表示します。
2. スマホを同じLANへ接続し、アプリにカメラ権限を許可します。
3. 「接続QRを撮影」でPCのQRを背面カメラに映すか、接続先の3項目を手入力します。
4. 「顔の送信を開始」を押し、前面カメラへ顔を向けます。

QRの形式は` spriteface://pair?v=2&host=192.168.1.10&port=11574&token=<32桁の16進数> `です。
このURIをOSのリンク経由で開く方法にも対応します。読み込んだだけでは送信を開始しません。
PCから暗号化された`ready`を受け取り、接続ごとの乱数を確認してから顔フレームを送ります。
送受信には同じUDPソケットを使い、接続先IPv4/ポート以外からの応答は受け取りません。
初期ポートは11574です。PCのファイアウォールで、このポートの受信を許可してください。

アプリを背面に移すと、カメラと送信を停止します。戻ったときはもう一度開始してください。
QR撮影も顔の送信も、カメラを外部アプリへ提供する機能ではありません。
Android 15までの対象APIではUDPのLAN通信は`INTERNET`権限で利用できます。
Wi-Fiの検索や設定変更をしないため、位置情報・近くのデバイス権限は要求しません。
`targetSdk`を将来引き上げる際にはAndroidのローカルネットワーク権限の変更を確認してください。

## データ

- 顔画像を送信・保存しません。スマホ内で640×480を目安にCPUで解析します。
- MediaPipe Android版の統計送信はビルド時に同ライブラリのno-op loggerへ切り替えます。
  改変箇所と検証範囲は[telemetry modification](docs/mediapipe-telemetry.md)を参照してください。
- 送信周期は約30Hzで、推論が遅い場合は最新値を使います。
- `eyeBlinkLeft/Right`を`1-score`に変換し、目は0=閉、1=開です。
- 口は`jawOpen`、笑顔は`mouthSmileLeft/Right`の平均です。
- 姿勢はMediaPipeのcolumn-major変換行列から`R=Rz(roll) Ry(yaw) Rx(pitch)`で取得します。
  角度はラジアンです。前面カメラのプレビューは鏡像でも、解析用の画像は反転しません。
- 顔の検出閾値は0.5です。APIが検出信頼度を公開しないため、共通形式の`confidence`は
  検出中1、未検出0という利用可能状態です。
- 顔が消えた場合や最新推論から500ms経過した場合は、`tracking:false`の通常表情を送信します。
- 停止時にも通常表情を送り、PC側のタイムアウト処理と組み合わせます。

通信仕様の詳細はルートのプロトコル文書を参照してください。
UDP通信はトークンを鍵とするAES-GCMで暗号化・認証します。平文の旧形式には接続できません。トークンが漏れた場合はPCで再生成してください。ルーターのポート転送で公開しないでください。

## 確認範囲

ローカル単体テストで接続URIの検証、姿勢のcolumn-major/ラジアン変換、値の範囲、
32bitシーケンス番号を確認します。カメラ、前面画像の向き、顔モデル、QR認識、
実機の温度・バッテリー消費とLAN接続はAndroid実機で確認が必要です。
