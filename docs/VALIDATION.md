# 初期版の検証結果

2026-10-13時点のローカル検証と、PrivateリポジトリのGitHub Actionsでの検証です。

| 対象 | 結果 |
| --- | --- |
| Linux C++ビルド | GCC 14.2 / CMake 4.4でGUI・受信CLIのビルド成功 |
| PCのGitHub Actions | Ubuntu 24.04／Windows Server 2022でReleaseビルド、CTest 4件、インストール成功。LinuxはGUI描画のスモークテストも成功 |
| C++テスト | 入力検証、UDP hello/ready、重複・順序逆転・シーケンス周回、peer切替、停止、平滑化が成功 |
| Pythonテスト | OpenSeeFace変換と合成送信の8件が成功 |
| SDK描画 | 独自デモのSSPJ/SSAE/SSCE/PNG読込、表情による画素変化、GLエラーなし |
| SDKパス処理 | Release＋`_FORTIFY_SOURCE=3`で長いパス・日本語・存在しないパス・作業ディレクトリ不変の回帰テスト成功。元SDKのバッファ検出による異常終了を再現し、修正後のGUI描画も成功 |
| UDPからSDK描画 | 別プロセスの合成送信で8フレーム受信・拒否0、通常表情と描画の差を確認 |
| メッシュ／ボーン | 公式SDKのローカルサンプルで読み込み・通常描画を確認。サンプル素材は同梱していない |
| ライセンス表示 | 保存した60件の原文／通知のSHA-256を確認 |
| Android | SDK35 / JDK17 / Gradle8.11.1でAPKビルド、単体テスト7件がローカルとCIで成功。CIのlintはエラー0・警告19件 |
| Android統計logger | ビルド後のbytecodeでno-op loggerへの置換を確認。通信の実機キャプチャ検証は未実施 |
| iOS | iPhone 16 Pro／iOS Simulator 18.6（x86_64）でアプリビルドとXCTest 3件がCIで成功。失敗・スキップ0件 |

GitHub Actionsの実行記録:

- [PC（Windows／Linux）](https://github.com/samiosu/spritestudio_facetrack_live/actions/runs/38352171818)
- [Android](https://github.com/samiosu/spritestudio_facetrack_live/actions/runs/38352171822)
- [iOS](https://github.com/samiosu/spritestudio_facetrack_live/actions/runs/38353822046)

Linuxの描画確認にはXvfbとMesa llvmpipeのソフトウェア描画を使用しました。
Windowsの実画面での描画とmacOSのPCビルドは未検証です。
実際のPC GPUとの性能比較やUnityとの速度・メモリ比較は行っていません。

実機のAndroid／iPhoneカメラ、表情の方向、QR読取、LAN権限、Wi-Fiの遅延、
バッテリー消費、OBSでの最終配信は確認が必要です。ユーザーの実キャラも未提供のため、
そのキャラの見た目やパーツ割り当ては未検証です。

モバイルのMediaPipe JNI/frameworkに静的結合された内部依存の完全な表示は、
公開バイナリの配布前に確認します。直接依存・確認済みAndroid依存の表示とハッシュは
`licenses/manifest.json`にあります。ソース公開の準備と公開APKの準備は区別します。
