# Notices

- The IKASCC RTL is the work of Sehyeon Kim (ika-musume) and is
  licensed under the BSD 2-Clause License. It is not vendored in this
  repository: `tools/prepare_rtl.py` fetches the pinned revision
  recorded in `docs/rtl_provenance.json` and verifies every file by
  SHA-256 before use. The upstream license text is kept beside the
  fetched sources as `rtl/IKASCC_LICENSE`.
- The harness code in `src/` and the tools in `tools/` are original
  to this repository.
