# AGENTS.md — IKASCC-verilator

## Purpose and working environment

- Read a K051649 (Konami SCC) VGM, run IKASCC RTL with Verilator, and obtain its rendered WAV.
- The RTL generates the sound. The C/C++ harness decodes VGM, drives the bus and clocks, captures digital output, and converts it to WAV; it must not replace IKASCC with a software sound-chip implementation.
- Main code is C/C++ and Verilog; Python tools support inspection and regression checks. Build and run primarily in WSL.
- Preserve source timing, register values, chip clock, and intermediate evidence. A valid WAV file alone does not establish correct sound or hardware equivalence.
- This is a rendering/simulation project. MML conversion, musical quantization, and cross-chip projection belong to other projects.
- Keep the normal flow simple: input VGM → RTL simulation → WAV. Optional intermediate/debug outputs support validation and are not additional product goals.

## Read before changing behavior

- Read `docs/project_knowledge.md` before changing parsing, bus timing, clocking, reset, signed values, gain, sample capture, or resampling.
- Read `docs/collaboration_principles.md` before substantial architectural decisions or changes to scope.
- Search relevant external field notes for prior measurements and rejected approaches. Do not invent missing decisions or apply another project's chip-specific assumptions without checking their scope.

## Build, run, and check

From the repository root in WSL:

```bash
bash tools/create_rtl_dir.sh --fetch-raw https://github.com/ika-musume/IKASCC
python3 tests/make_fixtures.py
./build_and_run.sh tests/vgm/scc_scale.vgm
./build_and_run.sh tests/csv/scc_scale.vgm.csv
./build_and_run.sh tests/vgm/scc_scale.vgm --enable-csv --debug
python3 tests/test_audio_pipeline.py build/obj_dir/ikascc_sim
```

- The script builds `build/obj_dir/ikascc_sim`. For isolated runs, use that executable from a separate output directory with an absolute input path. Avoid overwriting source fixtures or pre-existing outputs.
- Check both VGM and CSV paths when shared scheduling or audio code changes. Inspect intermediate evidence in addition to test exit status.
- VCD traces can be very large. Keep tracing off by default; use short, targeted intervals when needed.
- Do not invent undocumented command-line options. Inspect `README.md`, the script, and the executable's help.

## Layout and transformation boundaries

- `src/vgm_parser.*`: bounded VGM decoding and source/header validation.
- `src/vgm_player.*`: source event ordering, K051649 group translation, and playback schedule.
- `src/scc_bus.*`: physical bus transactions and clock advancement.
- `src/ikascc_wrapper.*`: Verilator lifecycle, clocks/reset, raw output capture, and PCM collection.
- `src/wav_writer.*`: WAV serialization.
- `rtl/`: locally supplied upstream IKASCC sources; preserve their provenance and license.
- `tests/`: synthetic VGM/CSV fixtures and automated checks; `tools/`: inspection/conversion helpers.
- `docs/`: public, stable knowledge and focused technical explanations.
- `handoffs/current.md`: restart state, completed checks, remaining work, and next allowed action.

Keep these responsibilities distinguishable without refactoring merely to match a diagram:

```text
Raw VGM / source timestamps
  -> ordered K051649 logical writes
  -> translated physical bus writes (requested and actual clock times)
  -> IKASCC RTL
  -> raw signed 11-bit samples and capture times
  -> explicitly resampled PCM
  -> WAV
```

## Evidence and implementation rules

- Preserve human-readable dumps of relevant source events, actual bus timing, raw output samples, and resampled output when investigating a transformation. Keep observed values separate from interpolated or otherwise derived values.
- Distinguish the VGM 44,100-unit timebase, K051649 master clock, native output update rate, and WAV sample rate. Changing a WAV header is not resampling.
- Use one master-clock timeline for bus work and playback waits. Account for transaction time; report unavoidable lateness rather than silently stretching every VGM interval.
- Capture the native output on every master-clock rising edge, including during bus writes and internal waits.
- Respect Verilator port widths: o_SOUND is signed 11-bit and requires explicit sign interpretation when read through unsigned generated C++ storage. OPM/OPLL output ports are not IKASCC outputs.
- Keep vendor RTL unchanged when a harness/build correction suffices. Record explicit parameter choices. An upstream parameter default is not proof that every branch works identically.
- Avoid heuristic output-path fallback, normalization, or filtering that conceals a timing or synthesis defect. If an approximation is intentional, document it at the PCM conversion boundary.
- Do not claim FPGA, cartridge, analog, or SPDIF equivalence from simulation-only measurements. Hardware observations supplied by the owner are reference evidence with their own clock/rate path.
- Keep changes attributable to a stage and preserve unrelated user changes. Do not rewrite the project merely to introduce documentation or a workflow.

## Fixtures and external private field notes

- Private VGM, recordings, ROM-derived data, and supplied archives stay outside git unless explicitly safe to commit. Use synthetic public fixtures for committed regressions.
- This project's notes belong in `../my-external-intelligence/IKASCC-verilator/field_notes/`. References to `field_notes/...` mean that directory. Do not create a local field_notes copy or symlink.
- Search related `msx_vgm2mml`, `x68k_vgm2mml`, and `mucom88_vgm2mml` note directories in that same private repository when a relevant shared decision is missing. Respect each note's source/target scope.
- Record experiments, ablations, metrics, rejected hypotheses, and unresolved questions as `field_notes/YYYY-MM-DD_topic.md`. Put only durable public principles in `docs/project_knowledge.md`; never copy private fixture contents into public documentation.
- If external notes cannot be read or written, report the limitation and intended destination, and continue independent authorized work without inventing history.
- Inspect the public and private repositories' status/diff separately. Commit or push only when requested; stage only task-related changes. A public commit does not include external notes, and its authorization does not authorize a private push.

## Done means

- The authorized change is implemented, with relevant tests and output generation checked or a concrete blocker reported.
- Relevant intermediate evidence was generated and inspected. Compare duration, pitch, signed amplitude, sample/capture counts, event ordering, and rhythm behavior as appropriate.
- Keep causal evidence separate from cleanup: preserve baseline and isolated parameter/audio/scheduling comparisons when diagnosing sound.
- Update stable knowledge, external field notes, and `handoffs/current.md` with verified results, remaining limits, next allowed action, and known traps.
- Report what was verified and what remains unverified; do not substitute successful file generation for correctness.
