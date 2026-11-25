# Quick start

WSLでVerilator、make、C/C++コンパイラを用意し、IKASCCのRTLとライセンスを`rtl/`に配置してください。

```bash
# RTLが未配置の場合のみ
bash tools/create_rtl_dir.sh --fetch-raw https://github.com/ika-musume/IKASCC

# フィクスチャ生成（初回のみ）
python3 tests/make_fixtures.py

# VGM → RTLシミュレーション → WAV
mkdir -p outputs
bash build_and_run.sh tests/vgm/scc_scale.vgm --output outputs/scale.wav

# 48 kHzで出力
build/obj_dir/ikascc_sim tests/vgm/scc_scale.vgm --output outputs/scale48.wav --sample-rate 48000

# 問題を調査するときの中間CSVとバス記録
build/obj_dir/ikascc_sim tests/vgm/scc_scale.vgm --output outputs/inspect.wav --enable-csv --debug outputs/inspect.bus.csv
```

通常はWAVだけを出力します。VGMヘッダーのK051649クロック（0の場合は3,579,545 Hz）でIKASCCを動かし、ネイティブの出力更新を取得して、指定のWAVレートへ実際に変換します。

詳細、対応範囲、回帰テストは[README.md](README.md)を参照してください。
