# スーパー32X CDの起動

`src/Gens/bin/Release/gens.exe` を起動し、File → Open ROM（Ctrl+O）から
`src/SampleGame/MODPlayer32X.iso` を開いてください。追加のBIOS設定は不要です。
ISO本体は変更していません。

このディスクはCDローダーから32Xを起動する「CD32X MOD Player」です。
ファイル選択画面では上下で移動し、パッドのAでフォルダーへ入り、MODを再生します。
現在の設定では上下矢印キーとキーボードのAに対応します。
確認例は `[MODS]` → `[6_8CH]` → `D2.MOD` です。

## 実装

- CD HLEの先頭セクターの起動コード、CDCSETMODE（モード0）、ユーザーコールを補完。
- CDのメモリーマップを保ったまま32Xのレジスターとフレームバッファーを接続。
  32Xへのアクセスで自動的に複合モードを有効にします。
- CDサブ68000、2つのSH2、Z80、VDP、PCM、PWMを同時に進めます。
- 32Xの内蔵代替コードで `_CD_` → `M_OK` / `S_OK` の起動手順と
  フレームバッファーからSDRAMへの転送を実行します。
- PWMから両SH2のDMAチャネル1へ要求を送り、音声を1サンプルずつ転送。
  転送数が0になると終了します。

32XのMaster/Slave BIOSが両方読み込める場合は外部BIOSを優先します。
それ以外は内蔵代替コードを使います。実機BIOSのデータは含めていません。
CD用の複合ステート形式は未実装のため、32X CDのステート保存・読込は無効です。

## 確認

```powershell
python tests/run_cd32x_tests.py --image src/SampleGame/MODPlayer32X.iso
```

通常描画・フレームスキップ・精密設定と11,025／22,050／44,100 Hzで、
起動、フォルダー選択、D2.MODの再生、32X背景の変化、継続した音声出力、
リセット後の再起動、終了を検証します。各ケースは3,600フレームです。
複合モードではCDの精密設定にかかわらず32Xの分割実行ループを使います。
起動ヘッダーの範囲・不正サイズと、両SH2のPWM DMA・転送終了も合成データで検査します。
結果は `src/Gens/bin/cd32x-tests/run-*` に保存します。

確認対象は提供ISOです。他の32X CDタイトル全般、実機BIOSとの実行比較、
デバッガーの命令単位実行は今回の検証範囲に含めていません。

起動手順・PWM要求の参照:
[PicoDriveの32Xメモリー・代替起動コード](https://github.com/notaz/picodrive/blob/master/pico/32x/memory.c)、
[PWM処理](https://github.com/notaz/picodrive/blob/master/pico/32x/pwm.c)。
