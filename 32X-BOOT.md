# スーパー32XのBIOSなし起動

`src/Gens/bin/Release/gens.exe` を起動し、File → Open ROM から
`src/SampleGame/MetalHead.32x` を開いてください。
BIOSファイルのダウンロードや設定は不要です。
Startでタイトルへ進み、「GAME START」を選んで開始できます。
キー割り当てはエミュレーターのコントローラー設定に従います。

68000・Master SH2・Slave SH2の外部BIOSがすべて読み込める場合は、
従来の起動方法を使用します。未設定・見つからない・必要な長さに満たない
ファイルがある場合は、3つとも内蔵の代替起動コードを使用します。

## 実装と確認範囲

内蔵コードは実機BIOSのコピーではありません。ROMヘッダーの情報から
SH2用プログラムをSDRAMへ転送し、割り込みベクター、通信レジスターの
M_OK / S_OK、68000のジャンプテーブルを用意してゲームへ制御を渡します。
通常のCPUエミュレーションで実行するため、起動待ち専用のホスト状態は
追加していません。ROMファイル自体は変更しません。

提供されたMetal Headで、オープニング、3Dデモ、タイトル、ミッション開始、
前進、旋回、攻撃を確認しました。通常描画・フレームスキップの両経路で
音声ミキサーの出力、起動中とミッション中のステート復元、リセット後の
再起動を検査しています。ステート検査はRAMとCPUの復元およびその後の
動作を対象とし、音声の完全一致や異なるBIOS構成間の互換性までは検証していません。

対象は32Xカートリッジです。全タイトルの互換性、全編クリア、
他タイトルのスーパー32X CD起動は確認していません。
提供されたMODPlayer32X.isoへの対応は [32X-CD-BOOT.md](32X-CD-BOOT.md) を参照してください。
外部BIOSの選択・読み込み処理は合成データで回帰検査しており、
実機BIOSを用いたゲーム実行の比較は行っていません。

## 検証方法

Visual StudioのC++ビルドツールとPythonを使います。

```powershell
python tests/run_32x_boot_tests.py --image src/SampleGame/MetalHead.32x
```

Releaseをビルドし、外部BIOSの優先、不完全なBIOSセットからの切り替え、
ROMヘッダーの転送範囲検査を実行します。ゲームは各経路で6000フレーム、
リセット後はさらに1800フレーム実行します。
画像とテスト用ステートは `src/Gens/bin/32x-boot-tests/run-*` に保存します。
ゲームやBIOSのデータはテストに同梱していません。

起動プロトコルを確認する際の参照先:
[PicoDriveの32X初期化](https://github.com/notaz/picodrive/blob/master/pico/32x/32x.c)、
[32Xメモリーマップ](https://github.com/notaz/picodrive/blob/master/pico/32x/memory.c)。
