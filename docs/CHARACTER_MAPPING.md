# キャラと顔の割り当て

`assets/demo/mapping.json`は、顔の入力信号をSpriteStudioのパーツ属性へ対応付ける例です。

```json
{
  "schema": 1,
  "bindings": [
    {
      "part": "head",
      "signal": "roll",
      "property": "rotation_z",
      "operation": "add",
      "input_min": -0.6,
      "input_max": 0.6,
      "output_min": -30,
      "output_max": 30
    }
  ]
}
```

| 項目 | 意味 |
| --- | --- |
| `part` | 選択中のアニメーションのパーツ名 |
| `signal` | `yaw`, `pitch`, `roll`, `left_eye`, `right_eye`, `mouth_open`, `mouth_smile` |
| `property` | `rotation_z`, `position_x/y`, `scale_x/y`, `local_scale_x/y`, `alpha` |
| `operation` | 通常アニメの値に`add`、`multiply`、または`replace` |
| `input_min/max` | 入力の範囲。範囲外は端点へ丸める |
| `output_min/max` | 対応する属性の範囲。回転の出力は度、入力の頭角度はラジアン |

顔の値はアニメーションの属性評価後、行列・ボーン・メッシュの計算前に適用します。
そのため、親パーツの変換が子へ伝わる流れを保ちます。
入力の目の値は0で閉じ、1で開きます。目のパーツを`local_scale_y`で縮める場合、
少し開いた最低値を設けると完全なゼロサイズを避けられます。

現在の割り当て対象は、選択中のモデルの直接のパーツです。
インスタンスパーツの内部の別モデルへ顔の割り当てを伝える機能や、複数の顔向きの
ポーズを滑らかに合成する機能、セルの自動切り替えは未実装です。
顔向きの立体感は、適切なパーツ配置・変形設定でキャラ側に作る必要があります。

アニメーションの組み合わせはキャラごとに確認してください。
未対応の見た目を、全SpriteStudio機能への対応として保証するものではありません。
