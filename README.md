# IKASCC-verilator

Konami SCC（K051649）のVGMを読み込み、Verilator上でIKASCCのRTLをシミュレーションして、結果をWAVとして取得します。音を生成するのはRTLです。C/C++側はVGMの解読、カートリッジバス（KonamiマッパーとSCCレジスタページ）とクロックの駆動、デジタル出力の取得、WAVへの変換を担当します。

## 準備と実行

WSL上のBash、Verilator、make、C/C++コンパイラ、zlib開発ライブラリが必要です。フィクスチャ生成と回帰テストにはPython 3を使います（追加Pythonパッケージは不要）。

まず、IKASCCのソースとライセンスを`rtl/`に配置してください。既に配置されているものを上書きする必要はありません。新規取得の場合は、上流ソースを確認したうえで次を使用できます。

```bash
bash tools/create_rtl_dir.sh --fetch-raw https://github.com/ika-musume/IKASCC
```

テスト用のフィクスチャを生成します（初回のみ）。

```bash
python3 tests/make_fixtures.py
```

リポジトリのルートから実行します。

```bash
bash build_and_run.sh tests/vgm/scc_scale.vgm --output outputs/scale.wav
```

出力先のディレクトリは先に作成してください。出力レートも指定できます。

```bash
mkdir -p outputs
bash build_and_run.sh tests/vgm/scc_scale.vgm --output outputs/scale.wav --sample-rate 48000
```

既にビルド済みなら、実行ファイルを直接使えます。

```bash
build/obj_dir/ikascc_sim input.vgm --output outputs/result.wav
```

通常の出力はWAVのみです。診断用のCSVやバス記録は、解析用のオプション（`--enable-csv`、`--debug`）を指定した場合だけ生成されます。

## 音声と時間の扱い

- VGMの待ち時間は1/44,100秒単位です。
- マスタークロックはVGMヘッダーのK051649クロック値を使用します。エクスポータによっては0の場合があり、そのときは3,579,545 Hzです（`--clock HZ`でも指定できます）。
- ネイティブ出力は符号付き11-bitの加算出力で、マスタークロックの立ち上がりごとに取得します。更新レートはマスタークロックと同じです。
- 取得したPCMは、整数時刻に基づく線形補間で44,100 Hz（既定）または48,000 Hzへ変換し、モノラルのWAVとして出力します。
- WAVへの変換では11-bit出力を32倍して符号付き16-bitの範囲に載せます。それ以外のゲイン、フィルタ、ピーク正規化は行いません。
- 上流パラメータは`IMPL_TYPE=1`（同期、3.58 MHz級のクロック）と`RAM_BLOCK=0`（DFFの波形テーブル）に固定しています。`i_MCLK_PCEN_n`はローに固定しているため、供給するクロックがそのままチップクロックになります。

線形補間はPCMの変換方法です。実機のアナログ出力との完全一致を保証するものではありません。

## 入力のVGMについて

入力は単一K051649のVGM/VGZ（バージョン1.10–1.71、gzip圧縮を含む）、または本ツールが出力したイベントCSVです。VGMの先頭から終了コマンドまでを一度だけ再生し、ループの繰り返し再生は行いません。他の音源への書き込みを含むVGMは、K051649以外の書き込みを定義済みの長さで読み飛ばし、SCCの部分だけを再生します。dualチップのヘッダーは明示的に拒否します。

VGMのストリームはSCCのレジスタページが選択済みであることを前提としています。ハーネスは実カートリッジのマッパー書き込み（BR2 := 0x3F）を先に発行し、各論理書き込みを0x9800のレジスタページへ変換します。

イベントCSVは物理バスの形式で、`delay,abhi,ablo,data`の行（先頭に`# clock_hz=`のメタデータ付き）からなります。`abhi`はA15..A11です。`wait`行は書き込みなしに時間を進めます。VGMの実行で`--enable-csv`を指定すると同じ形式で書き出すため、VGMとCSVのレンダリングをバイト単位で比較できます。

## ライセンス

Copyright (c) 2026 emef2247.

Licensed under the [BSD 2-Clause License](LICENSE).

本プロジェクトで利用または参照している第三者コンポーネントのライセンスおよび著作権表示については、[NOTICE.md](NOTICE.md)を参照してください。IKASCCのRTL本体は本リポジトリに含まず、各自で取得する形です（取得時に上流のライセンス文が`rtl/IKASCC_LICENSE`へ保存されます）。RTLの取得元と固定リビジョンは[docs/rtl_provenance.json](docs/rtl_provenance.json)に記録しています。

## 謝辞

本プロジェクトの開発、検証、およびテスト生成では、以下のツールやプロジェクトを利用または参考にしています。開発者・メンテナー・貢献者の皆様に感謝します。

とりわけ、K051649（Konami SCC）のRTL再実装であるIKASCCを公開してくださったika-musumeさんに感謝します。本プロジェクトはIKASCCのRTLそのものを音源として鳴らすもので、その公開がなければ成り立ちませんでした。

| プロジェクト | 用途 |
| --- | --- |
| [IKASCC](https://github.com/ika-musume/IKASCC) | 音源となるK051649のRTL本体（ika-musumeさん） |
| [Verilator](https://www.veripool.org/verilator/) | RTLシミュレーション |

使用した第三者コンポーネントの参照元、著作権表示、ライセンス条件は[NOTICE.md](NOTICE.md)に記録しています。
