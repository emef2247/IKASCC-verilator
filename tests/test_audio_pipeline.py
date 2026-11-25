#!/usr/bin/env python3
"""Audio pipeline regression: render the synthetic scale fixture through
both the VGM and the CSV input paths and check the results.

Usage: python3 tests/test_audio_pipeline.py build/obj_dir/ikascc_sim
Checks (smoke level, by design): both WAVs are produced, have the exact
expected frame count, are byte-identical to each other, each note's
dominant frequency is near the intended pitch, and the tail after the
final key-off is silent.
"""
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTES = [261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25]
NOTE_SAMPLES = 6615
TOTAL_FRAMES = 57330


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2, path
        assert w.getframerate() == 44100, path
        frames = w.getnframes()
        data = w.readframes(frames)
    samples = [int.from_bytes(data[i:i + 2], "little", signed=True)
               for i in range(0, len(data), 2)]
    return frames, samples


def dominant_freq(samples, rate=44100):
    # Zero-crossing estimate; the fixture waveform is a sine.
    seg = samples[1500:6000]
    crossings = sum(1 for a, b in zip(seg, seg[1:]) if (a < 0) != (b < 0))
    return crossings / 2.0 / (len(seg) / rate)


def main():
    sim = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/obj_dir/ikascc_sim")
    out = ROOT / "outputs"
    out.mkdir(exist_ok=True)
    wavs = {}
    for name, fixture in (("vgm", ROOT / "tests/vgm/scc_scale.vgm"),
                          ("csv", ROOT / "tests/csv/scc_scale.vgm.csv")):
        target = out / ("test_scale_%s.wav" % name)
        subprocess.run([str(sim), str(fixture), "--output", str(target)],
                       check=True, capture_output=True)
        wavs[name] = target.read_bytes()
        frames, samples = read_wav(target)
        assert frames == TOTAL_FRAMES, (name, frames)
        for i, freq in enumerate(NOTES):
            measured = dominant_freq(samples[i * NOTE_SAMPLES:(i + 1) * NOTE_SAMPLES])
            assert abs(measured - freq) < freq * 0.03, (name, i, freq, measured)
        tail = samples[8 * NOTE_SAMPLES + 2000:]
        assert all(v == 0 for v in tail), (name, "tail not silent")
    assert wavs["vgm"] == wavs["csv"], "VGM and CSV renders differ"
    print("test_audio_pipeline: OK")


if __name__ == "__main__":
    main()
