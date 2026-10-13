# OSS依存と配布方針

自作コードはMITです。外部のコード・フォント・モデルは、それぞれの元の
ライセンスと著作権表示を維持します。GPL/LGPLも採用可能ですが、採用時に
その版・組み込み方・配布範囲に合わせて対応するソースなどを用意します。

## 採用した直接依存

| 対象 | OSS | 固定版 | ライセンス | 配布上の扱い |
| --- | --- | --- | --- | --- |
| PC | SpriteStudio6-SDK | 2.3.1 | BSD 3条型の原文 | コアのみビルド。改変をビルド用コピーに適用 |
| PC | SDK内TinyXML2 | SDK 2.3.1のコピー | zlib | 元の表示を保持 |
| PC | SDK内MersenneTwister | SDK 2.3.1のコピー | MIT | 元の表示を保持 |
| PC | SDK内GLEW | 2.2.0 | BSD系＋Mesa/KhronosのMIT | 元のライセンス全文を保持 |
| PC | GLFW | 3.4 | zlib | ウィンドウ・入力 |
| PC | Dear ImGui | 1.91.9b | MIT | 設定UI。内包stbヘッダもMITを選択 |
| PC | ProggyClean | ImGui内包版 | MIT | 初期英語UIの内包フォント |
| PC | nlohmann/json | 3.11.3 | MIT | 設定・通信。内包著作者表示とAbseilの由来も保持 |
| PC | Nayuki QR Code generator | 1.8.0 | MIT | PC接続QRを生成 |
| PC | stb_image | `2c980bb59875b0d32144a71867fbdebb2f77cd20` | MITを選択 | SDK内の古いstbはコンパイルしない |
| Android | MediaPipe Tasks Vision | 0.10.32 | Apache-2.0と元の追加表示 | スマホで顔推定 |
| Android | CameraX core/camera2/lifecycle/view | 1.4.2 | Apache-2.0 | カメラ取得 |
| Android | AndroidX activity-ktx | 1.10.1 | Apache-2.0 | Activity・ライフサイクル |
| Android | Kotlin標準ライブラリ | 2.1.20、JDK互換用1.8.20 | Apache-2.0 | releaseRuntimeClasspathで解決を確認 |
| Android | ZXing core | 3.5.3 | Apache-2.0 | 接続QRの読み取り |
| iOS | MediaPipeTasksVision / TasksCommon | 0.10.21 | Apache-2.0と元の追加表示 | スマホで顔推定。VisionのpodspecがCommonも同版に固定 |
| スマホ | Face Landmarkerモデル | `float16/1`、SHA-256固定 | Apache-2.0 | スクリプトで別途取得。コードとは別の権利表示 |
| ビルド | Gradle Wrapper / Gradle | 8.11.1 | WrapperはApache-2.0。Gradle本体には追加依存の表示あり | Wrapperはリポジトリ内。本体は開発時に取得 |
| ビルド | Android Gradle Plugin | 8.9.2 | Apache-2.0 | 外部ビルド依存 |
| ビルド | Kotlin Gradle Plugin | 2.1.20 | Apache-2.0 | 外部ビルド依存 |
| テスト | JUnit | 4.13.2 | EPL-1.0 | Android JVMテストのみ。アプリには同梱しない |

採用版のソースURL、解決したGitコミット、正式ライセンス本文のSHA-256、
抽出元ファイル・ソースアーカイブのSHA-256は
[`licenses/manifest.json`](../licenses/manifest.json)に記録しています。
これらはライセンスの来歴の記録です。依存リポジトリ全体やバイナリ全体の
ハッシュではありません。

デスクトップで日本語フォントを追加する場合は、そのフォントの表示と再配布条件を
追加します。現時点ではNotoなどの別フォントは取り込んでいません。

## Androidの実解決した推移依存

[`mobile/android/docs/dependencies.txt`](../mobile/android/docs/dependencies.txt)に
releaseRuntimeClasspath全体の解決済み版を保存しています。実際のAARに埋め込まれた
ライセンスは`licenses/android-embedded/`へ原文のまま保存し、Java依存の著作権表示は
`licenses/android-transitive/`へ保存しました。

protobuf-javalite 4.26.1はBSD-3-Clause、animal-sniffer-annotations 1.17はMITです。
checker-compat-qual 2.5.3は、元のライセンスが注釈・互換注釈に明示的に許可するMITを
適用します。Guava 27.0.1-android、failureaccess 1.0.1、Kotlin coroutines 1.7.3、
Flogger 0.6、各種Google/JetBrains注釈、datatransport、Firebase encodersなどの
Apache-2.0依存も確認・表示を追加しています。

JSR305 3.0.2はPOMでApache-2.0と宣言されていますが、ソースの4つの並行処理用注釈には
Brian Goetzの著作権とCC-BY-2.5が明記されています。この原文表示と正式なCC-BY-2.5
法的コード、出典リンクを保持します。これらの注釈ソースは改変していません。

AndroidビルドではMediaPipe tasks-core 0.10.32の`TasksStatsLoggerFactory.class`を
変更し、同梱のダミーロガーを選択してSDKテレメトリーを無効にします。
Apache-2.0の改変表示を`THIRD_PARTY_NOTICES.md`に残し、詳細を
[`mediapipe-telemetry.md`](../mobile/android/docs/mediapipe-telemetry.md)に記録します。

## 外部アプリ

OpenSeeFaceはユーザーが別途導入し、UDPでアダプターへ接続します。
本体・モデル・実行環境・カメラDLLのコピーはリポジトリや配布物に入れていません。
本体のBSD-2-Clauseは確認済みですが、配布パッケージ全体の条件とは異なります。
将来同梱する場合はlibdshowcaptureのLGPL-2.1-or-later、OpenCVと使用するFFmpeg、
ONNX Runtime・NumPy・Pillow・Pythonなどを実際の配布ファイルごとに確認します。

OBS Studioも別途導入します。通常のウィンドウキャプチャで使用し、OBSのコードや
ライブラリは組み込みません。OBS自体はGPL-2.0-or-laterです。

SpriteStudioの編集ソフトはプロプライエタリで、本アプリから再配布しません。
SDKのJUCE使用Viewerは今回のビルドには含めません。SDK付属キャラ素材や
ユーザーのキャラ素材にも、自作コードのMITを適用しません。

## ソース公開とバイナリ配布

GitHubに公開するソースにはルートの`LICENSE`、
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)、`licenses/`を含めます。
デスクトップのインストール対象にも同じ表示を含め、Android/iOSはアプリリソースに
ライセンス一式を入れます。元のソース内の著作権・ライセンス表示も保持します。

モデルカードは公式PDFの全テキストを保存し、Apache-2.0の記載を確認しました。
PDFの図は含めず、元PDFのURLとSHA-256を残しています。モデルファイルは
ダウンロードスクリプトのSHA-256検証を通して取得します。

Android/iOSの実際のバイナリを配布する段階では、次を追加します。

1. Gradleの解決済みrelease依存グラフをリリースごとに更新し、CocoaPodsの`Podfile.lock`を保存する。
2. AAR/framework内のライセンスとNOTICE、Podsのacknowledgementsを確認・保持する。
3. MediaPipe内の推論エンジンやその他の推移依存を、最終配布物に合わせて一覧化する。
4. バイナリと対応するソースの版を一致させ、依存一覧とライセンスを同梱する。

現在の一覧は直接依存と確認済み表示の記録で、最終モバイルバイナリの完全なSBOM
ではありません。確認したMediaPipe Android AARにはJNIバイナリの内包ライブラリの
ライセンス一覧が付いておらず、CameraX coreにもネイティブバイナリがあります。
これらのネイティブ推移依存の表示を揃えることは、モバイルバイナリの公開前に残る作業です。
モデル・キャラ素材・フォントはコードとは別に管理します。

## GPL/LGPLを追加する場合

GPL部品を結合したアプリは、該当GPLの条件で配布できるライセンス構成にし、
バイナリと対応するソース・必要なビルドスクリプト・表示を提供します。
LGPLでは採用版と結合方法に応じたソース提供・差し替え・再リンク条件を満たします。
別プロセスでも、GPL/LGPLソフト自体を同梱する場合はその配布条件が残ります。

Apache-2.0はGPLv3と互換ですがGPLv2-onlyとは互換ではないため、追加する依存の
ライセンス版まで確認します。自作コードを公開するだけで外部依存の条件が
自動的に満たされるとは扱いません。

- [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html)
- [ApacheのGPL互換性説明](https://www.apache.org/licenses/GPL-compatibility.html)
- [OpenSeeFace](https://github.com/emilianavt/OpenSeeFace)
- [libdshowcapture](https://github.com/emilianavt/libdshowcapture)
- [opencv-pythonの配布ライセンス](https://github.com/opencv/opencv-python#licensing)
