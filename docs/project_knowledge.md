# IKASCC-verilator project knowledge

## Purpose and boundaries

Read K051649 (Konami SCC) VGM, drive upstream IKASCC RTL under
Verilator, and write mono WAV. The host performs parsing, bus
scheduling, capture and explicit sample-rate conversion. It must not
replace RTL synthesis, interpret a musical score, quantize source
times, normalize gain or remap to another chip.

Keep original source timestamps/register bytes, actual bus timing,
native output samples, derived resampled PCM and final WAV separately
inspectable. WAV generation alone does not establish correct sound or
hardware equivalence.

## Versioned RTL and implementation parameters

The supported upstream is https://github.com/ika-musume/IKASCC,
revision 5aa3f13fabf6a5cbe613d8703100c115481aadea. All four Verilog
sources are retained unmodified under ignored rtl/src.
docs/rtl_provenance.json records SHA256 hashes and the BSD-2-Clause
license hash. tools/prepare_rtl.py verifies them before build and
supports repeatable local preparation without committing vendor files.

Explicit parameters: IMPL_TYPE=1 (synchronous implementation for a
slow, 3.58 MHz-class clock; wavetable RAM samples CPU data on the
falling edge), RAM_BLOCK=0 (DFF wavetable storage, no vendor BRAM).
i_MCLK_PCEN_n is held low, so i_EMUCLK is the chip clock itself. The
upstream localparam RAMCTRL_ASYNC=1 stays as shipped. Reset is
synchronous in this mode; the harness asserts i_RST_n for 1024 master
clocks and releases it for 256 more, all excluded from the playback
time origin and audio recording.

## Four distinct rates

- VGM time is integer units of 1/44100 second.
- The K051649 master clock comes from the VGM 0xD0 header field; the
  usual MSX clock is 3579545 Hz. Several exporters (notably
  msxplay.com) leave the field zero; the harness then uses 3579545 Hz
  or an explicit --clock. Clock flag bit 30 (dual chip) and bit 31
  are rejected. CSV uses recorded metadata or explicit --clock.
- The native output o_SOUND may change on every master clock; the
  harness captures it on every rising edge.
- WAV is 44100 Hz by default, or 48000 Hz. Conversion uses rational
  integer master-clock positions and linear interpolation. Relabeling
  the WAV header is not resampling. This is not a claim of ideal
  antialias/analog filtering.

## The bus: mapper first, then the register page

IKASCC is the whole cartridge block: a Konami VRC mapper plus the SCC
player. The SCC register page is selected only after bank register 2
(the 0x9000 window, A15..A11 = 0x12) holds 0x3F. Register writes then
target A15..A11 = 0x13 (the 0x9800 window) with the register number in
A7..A0:

- 0x00..0x7F waveform RAM (ch4 and ch5 share 0x60..0x7F)
- 0x80..0x89 frequency (two bytes per channel, low then high)
- 0x8A..0x8E volume, 0x8F channel enable
- 0xE0..0xFF test register

A VGM stream assumes the page is already selected, so the player
issues the BR2 := 0x3F write once at the start of a VGM run. CSV
input is raw bus form and carries its own mapper writes.

In synchronous mode the RTL samples the bus through two-stage
synchronizers and raises an internal write request on the first clock
that sees /CS and /WR asserted. The harness holds each write for
several clocks and lets the synchronizers settle before the next
transaction. Transaction time is accounted on the one master-clock
timeline; unavoidable lateness is reported, not hidden by stretching
the source timeline.

## VGM K051649 command form

0xD2 carries three operand bytes: port, register, data. The port is
the register group (0/4 waveform, 1 frequency, 2 volume, 3 key
on/off, 5 test; bit 7 is the dual-chip select). There is no persistent
register latch across commands. Logical-to-physical translation:

- waveform reg 0x00..0x5F -> page 0x00..0x5F; reg 0x60..0xFF ->
  page 0x60..0x7F (the shared ch4/ch5 RAM, as in the MAME core)
- frequency reg 0x00..0x09 -> page 0x80..0x89
- volume reg 0x00..0x07 -> page 0x8A..0x91 (only 0..4 exist)
- key on/off -> page 0x8F; test reg r -> page 0xE0 | (r & 0x1F)

Writes to other chips are skipped by their defined VGM lengths, so
mixed MSX files render their SCC part. Unknown opcodes fail.

## Native output and signed values

o_SOUND is the chip's signed 11-bit summer output, updated on the
master clock. Verilator stores the port unsigned; sign extension is
explicit in the wrapper. The WAV conversion scales the 11-bit value
by 32 onto the int16 range — a representation change of the chip's
own full range, not a gain adjustment. No filtering, normalization
or output-path fallback is applied. The .native.csv log records each
output change with its master-clock position; the resampled PCM is
derived data and is kept in a separate .pcm.csv.
