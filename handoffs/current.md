# Current handoff — IKASCC-verilator

## State (2026-11-25)

- Repository scaffold created ahead of the public GitHub repository,
  following IKAOPM-verilator / IKAOPLL-verilator conventions.
- Harness implemented: `src/vgm_parser.*` (VGM 1.10–1.71, gzip,
  K051649 clock at header 0xD0), `src/vgm_player.*` (0xD2 port/reg/data
  group translation to the 0x9800 register page), `src/scc_bus.*`
  (physical bus writes, mapper BR2 := 0x3F enable first on VGM runs),
  `src/ikascc_wrapper.*` (IMPL_TYPE=1, RAM_BLOCK=0, signed 11-bit
  o_SOUND captured every clock, linear-interpolation resampling),
  `src/wav_writer.*` (mono 16-bit).
- RTL pinned: ika-musume/IKASCC @ 5aa3f13fabf6a5cbe613d8703100c115481aadea,
  hashes in docs/rtl_provenance.json; `tools/create_rtl_dir.sh
  --from-dir <upstream checkout>` verified.
- Verified 2026-11-25: clean rebuild passes
  `python3 tests/test_audio_pipeline.py` (VGM and CSV renders
  byte-identical, per-note pitch within tolerance, exact frame
  count, silent key-off tail). Real mixed fixtures from
  msx_vgm2mml (public psg_scc_001 / 002) render their SCC part with
  sound: 2,974 / 2,096 bus writes, max lateness 183 clocks.
  Equivalence with the physical chip beyond this smoke level is the
  upstream RTL's claim, not separately verified here.

## Traps found

- VGM 0xD2 is `port, reg, data` — three operand bytes, self-contained
  per command. It is NOT a two-step latch protocol across commands.
  (Confirmed against libvgm's SendYMCommand and msx_vgm2mml's
  vgm_reader.py group bases 0x9800/0x9880/0x988A/0x988F.)
- A failed `build_and_run.sh` invocation filtered through grep can
  leave a stale `ikascc_sim` binary that silently runs old code.
  Rebuild from a clean `build/` when behavior contradicts the source.

## Next allowed action

- Owner creates the GitHub repository and pushes the prepared
  initial commit.
- Possible follow-up (owner's idea, 2026-11-25): a multi-instance
  renderer that layers slightly detuned / waveform-deformed IKASCC
  instances over the original — an SCC form of the "layered"
  treatment. Not started.
