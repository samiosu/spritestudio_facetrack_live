# SpriteFace

SpriteStudioで作ったキャラクターを、PCカメラまたはスマホの顔の動きで
リアルタイムに動かすための、開発中のOSSアプリです。
PC側はC++とSpriteStudio SDKを使用します。

## 現在の機能

- `.sspj`と参照する`.ssae`・`.ssce`・画像の読み込み、アニメーション再生。
- 顔向き・左右のまばたき・口の開閉・笑顔の値を、パーツの位置・回転・大きさ・透明度へ割り当て。
- 顔の値の平滑化と、顔が見つからない／接続が切れた場合の通常表情への復帰。
- Android／iOSの端末内顔認識とUDP送信、QR／手入力によるペアリング。
- PCカメラ用OpenSeeFaceの外部連携。
- OBS向けの独立したキャラ表示ウィンドウ。初期版の透過はクロマキーで行います。

独自に描いた簡単なデモキャラを同梱しています。既存キャラは、動かしたい
パーツへの割り当てを設定してください。顔を横に向けたときの絵や変形が
自動生成される機能はありません。


## PCアプリのビルド

C++17コンパイラ、CMake 3.24以上、Git、OpenGLの開発環境が必要です。
補助スクリプトにはPython 3.11以上を使用します。
初回構成時に、固定したバージョンの依存ソースを取得します。

```bash
git clone https://github.com/samiosu/spritestudio_facetrack_live.git
cd spritestudio_facetrack_live
```

Ubuntu／Debianの開発環境例:

```bash
sudo apt-get install build-essential cmake ninja-build git libssl-dev libgl1-mesa-dev \
  libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
./build/desktop/spriteface
```

WindowsではVisual Studio 2022の「C++によるデスクトップ開発」、CMake、Gitを使用します。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 4
.\build\desktop\Release\spriteface.exe
```

Linuxのビルドと仮想ディスプレイ上の描画、WindowsのGitHub Actions上での
Releaseビルドとテストを検証しています。Windowsの実画面での描画と
macOSのPCビルドは未検証です。iOSアプリのビルドにはmacOSとXcodeが必要です。

GUIを使わずに通信だけ確認する場合:

```bash
cmake -S . -B build-core -DSPRITEFACE_BUILD_GUI=OFF
cmake --build build-core --parallel 4
./build-core/spriteface-receiver
```

## デモを動かす

GUIを開くとデモキャラが表示されます。「Manual test input」で手動の目・口・
頭の動きを試せます。カメラを使わない通信デモは、GUIの「Copy token」で
コピーしたセッショントークンを使って実行します。

```bash
python3 -m pip install -r tools/requirements.txt
python3 tools/send_demo.py --token YOUR_SESSION_TOKEN
```

このスクリプトは合成した動きの送信です。実際の顔認識とは分けて確認できます。
「Face mapping」で割り当てを編集し、JSONへ保存できます。

既存キャラを開く例:

```bash
./build/desktop/spriteface --project /path/to/character.sspj --mapping /path/to/mapping.json
```

同じフォルダー構成を維持したプロジェクトと参照ファイルを渡してください。
割り当て例は `assets/demo/mapping.json`、設定仕様は [キャラ設定](docs/CHARACTER_MAPPING.md) にあります。

## スマホから動かす

1. `python3 tools/fetch_models.py`でハッシュを検証した公式モデルを取得します。
2. [Android](mobile/android/README.md)または[iOS](mobile/ios/README.md)の手順でアプリをビルドします。
3. PCの「Allow phone on same LAN」を有効にし、到達可能なPCのLAN IPv4を指定します。
4. スマホでPCに表示されたQRを読み取り、カメラと必要なLAN権限を許可します。
5. OBSで「SpriteFace - OBS Capture」を取り込み、緑背景へクロマキーフィルターを適用します。

初期版は、同じ信頼できるLAN内のPCとスマホ1台を対象にしています。
必要に応じてPCのファイアウォールでUDP 11574を許可してください。
映像はスマホ内で顔認識に使い、PCには顔の数値を送ります。
UDP転送はペアリングトークンを鍵としてAES-GCMで暗号化・認証します。従来の平文形式は受信しません。[通信仕様](protocol/README.md)を参照してください。

## PCカメラを使う

[OpenSeeFace](https://github.com/emilianavt/OpenSeeFace)を別途導入してカメラ取得を開始します。
OpenSeeFace側は **127.0.0.1:11573** へ送信してください。
別ターミナルで、GUIのトークンを指定して変換アダプターを実行します。

```bash
python3 -m pip install -r tools/requirements.txt
python3 tools/openseeface_adapter.py --token YOUR_SESSION_TOKEN
```

OpenSeeFaceのバイナリ形式を、アプリが受け取る暗号化されたSF2 UDPへ変換します。
OpenSeeFaceを直接11574へ送信しても動きません。
表情の値はOpenSeeFace側のキャリブレーションにも依存します。

## 検証

```bash
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 tools/check_licenses.py
# GUI描画のスモークテスト。Linuxヘッドレス環境ではxvfb-runを使用。
./build/desktop/spriteface --smoke-test
```

通信の異常値、セッショントークン、順序逆転、シーケンスの周回、停止後の復帰、
平滑化、OpenSeeFace変換をテストしています。実機のスマホカメラ、Wi-Fi接続、
OBSでの最終配信は、この環境では検証できていません。
[検証結果](docs/VALIDATION.md)に詳細を記録しています。

## ライセンスと公開

自作のアプリ本体と独自デモ素材はMITです。著作権者は`samiosu`です。
`tools/openseeface_adapter.py`、`tools/send_demo.py`、`tests/test_adapter.py`は、
GPL-3.0-or-laterです。全文は[COPYING.GPL-3](COPYING.GPL-3)にあります。
依存のライセンスは維持します。
[依存一覧](docs/DEPENDENCIES.md)、[サードパーティー表示](THIRD_PARTY_NOTICES.md)、
`licenses/manifest.json`に取得元・バージョン・ライセンスを記録しています。
GPL／LGPLも必要条件を満たしたうえで利用できます。

OpenSeeFaceとOBSは初期版では外部アプリとして連携します。同梱する場合は
それぞれのライセンス条件も満たしてください。SpriteStudioのエディタ本体や
第三者のキャラ素材は、このリポジトリに含めていません。

このリポジトリは作成中であり、テストプレイが不十分です。
