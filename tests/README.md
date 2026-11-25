# Tests

Synthetic fixtures only; no game-derived data is committed here.

- `make_fixtures.py` regenerates `vgm/scc_scale.vgm` and
  `csv/scc_scale.vgm.csv`: one SCC channel playing an ascending C major
  scale with a sine-like waveform. The CSV is the same performance as
  physical bus writes, so both inputs must render identical WAVs.
- `test_audio_pipeline.py build/obj_dir/ikascc_sim` renders both
  fixtures and checks frame counts, pitch per note, tail silence, and
  VGM/CSV equality.
