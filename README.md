# IKASCC-verilator

Read a Konami SCC (K051649) VGM, run the upstream IKASCC RTL with
Verilator, and obtain its rendered WAV.

The RTL generates the sound. The C/C++ harness decodes the VGM, drives
the cartridge bus (Konami mapper + SCC register page) and the clock,
captures the digital output, and converts it to WAV. It does not
replace IKASCC with a software sound-chip implementation.

## Quick start

See [QUICK_START.md](QUICK_START.md).

## Inputs and scope

- VGM 1.10–1.71, plain or gzip-compressed. Only K051649 writes are
  rendered; writes to other chips (PSG, OPLL, …) are skipped by their
  defined lengths, so a mixed MSX VGM renders its SCC part. Loops are
  not followed (first traversal only).
- The K051649 clock comes from the VGM header (0xD0) when nonzero;
  several exporters leave it zero, in which case 3,579,545 Hz is used
  (or `--clock HZ`). Dual-chip headers are rejected.
- A VGM stream assumes the SCC register page is already selected. The
  harness issues the physical cartridge's mapper write first
  (BR2 := 0x3F), then translates each logical write to the register
  page at 0x9800.
- CSV input is the physical bus form: `delay,abhi,ablo,data` rows
  (with `# clock_hz=` metadata), where `abhi` is A15..A11. A `wait`
  row advances time without a write. `--enable-csv` on a VGM run
  writes exactly this form, so VGM and CSV renders can be compared
  byte for byte.

## Layout

- `src/vgm_parser.*`: bounded VGM decoding and header validation.
- `src/vgm_player.*`: event ordering, K051649 group translation, CSV input.
- `src/scc_bus.*`: physical bus transactions and clock advancement.
- `src/ikascc_wrapper.*`: Verilator lifecycle, clock/reset, output capture, resampling.
- `src/wav_writer.*`: WAV serialization (mono, signed 16-bit).
- `rtl/`: locally prepared upstream IKASCC sources (pinned by
  `docs/rtl_provenance.json`, never vendored).
- `tests/`: synthetic fixtures and regression checks.
- `docs/`: project knowledge and collaboration principles.

## Notes

- Upstream parameters are fixed at `IMPL_TYPE=1` (synchronous,
  3.58 MHz-class clock) and `RAM_BLOCK=0` (DFF wavetable storage).
  `i_MCLK_PCEN_n` is tied low, so the supplied clock is the chip clock.
- `o_SOUND` is the chip's signed 11-bit summer output. The WAV
  conversion scales it by 32 onto the int16 range; no other gain,
  filtering, or normalization is applied.
- A WAV file alone does not establish hardware equivalence.
