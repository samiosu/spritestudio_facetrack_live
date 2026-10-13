# 初期版の検証結果

2026-10-13時点のローカル検証です。GitHub Actionsの実行結果ではありません。

| 対象 | 結果 |
| --- | --- |
| Linux C++ビルド | GCC 14.2 / CMake 4.4でGUI・受信CLIのビルド成功 |
| C++テスト | 入力検証、UDP hello/ready、重複・順序逆転・シーケンス周回、peer切替、停止、平滑化が成功 |
| Pythonテスト | OpenSeeFace変換と合成送信の8件が成功 |
| SDK描画 | 独自デモのSSPJ/SSAE/SSCE/PNG読込、表情による画素変化、GLエラーなし |
| UDPからSDK描画 | 別プロセスの合成送信で8フレーム受信・拒否0、通常表情と描画の差を確認 |
| メッシュ／ボーン | 公式SDKのローカルサンプルで読み込み・通常描画を確認。サンプル素材は同梱していない |
| ライセンス表示 | 保存した60件の原文／通知のSHA-256を確認 |
| Android | SDK35 / JDK17 / Gradle8.11.1でAPKビルド、単体テスト7件、lint成功 |
| Android統計logger | ビルド後のbytecodeでno-op loggerへの置換を確認。通信の実機キャプチャ検証は未実施 |
| iOS | 実装とXcodeGen/CocoaPods/CI設定を静的確認。XcodeビルドとXCTest実行は未確認 |

Linuxの描画確認にはXvfbとMesa llvmpipeのソフトウェア描画を使用しました。
実際のPC GPUとの性能比較やUnityとの速度・メモリ比較は行っていません。

実機のAndroid／iPhoneカメラ、表情の方向、QR読取、LAN権限、Wi-Fiの遅延、
バッテリー消費、OBSでの最終配信は確認が必要です。ユーザーの実キャラも未提供のため、
そのキャラの見た目やパーツ割り当ては未検証です。

モバイルのMediaPipe JNI/frameworkに静的結合された内部依存の完全な表示は、
公開バイナリの配布前に確認します。直接依存・確認済みAndroid依存の表示とハッシュは
`licenses/manifest.json`にあります。ソース公開の準備と公開APKの準備は区別します。
