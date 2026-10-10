# Synthetic media fixtures

All video and audio payloads in this directory were generated locally from seeded white noise; each clip is approximately two seconds long. The late-subtitle fixture also has a two-second cue, but its timestamp starts at one hour. No source film, music, artwork, or other copyrighted media was used. `attachments/synthetic-cover.png` is also generated from a synthetic noise frame.

## Video

- `video/bluray_like_hdr10.mkv`: HEVC Main 10, BT.2020/PQ, mastering-display and content-light metadata; English AC-3 5.1 and Spanish AAC 5.1; English and Spanish PGS; attached synthetic cover.
- `video/bluray_like_hdr10.mp4`: despite the historical filename, this fixture contains H.264 8-bit SDR video with a BT.709 color matrix, two 5.1 AAC tracks and English/Spanish `mov_text` subtitles. Primaries and transfer are unspecified; HDR metadata is absent. MP4 does not carry the PGS tracks used by the MKV cases.
- `video/bluray_like_vp9.mkv`: VP9 video, the same two distinct 5.1 audio codecs and English/Spanish PGS subtitles.
- `video/anime_like.mkv`: exactly one HEVC video, one English Opus 5.1 track, three bitmap PGS tracks (`spa`, `eng`, `jpn`) and one `ipag-mona.ttf` attachment. The font is the IPA Mona font installed from Gentoo's `media-fonts/ipamonafont` package; its redistribution notice remains beside the fixture as `attachments/IPAMona-LICENSE.txt`.
- `video/hdr10_metadata_source.mkv`: HEVC Main 10 source with BT.2020/PQ, mastering-display and content-light metadata.
- `video/hdr_without_mastering_metadata.mkv`: BT.2020/PQ HEVC Main 10 with mastering-display and content-light side-data intentionally absent. `File` is expected to classify it as HDR10-like and report `HDR10::Source::Heuristics` from the signaled primaries/transfer, rather than treating it as SDR.
- `video/xvid_mp3_stereo.avi`: Xvid/MPEG-4 Part 2 video with stereo MP3 audio.

When creating these video/audio container fixtures, video and audio were encoded from the valid synthetic VP9, audio-noise, or PCM sources rather than stream-copied. HDR10 and metadata-free HDR-like HEVC sources were re-encoded with x265's normal GOP settings, then each container's video/audio was encoded again as appropriate. Only subtitle tracks were copied while packaging. The cover and font attachments were added with FFmpeg's Matroska attachment support. PGS tracks were rendered from local synthetic SRT files using tsMuxer, then exported as SUP. Repository tests may remux or re-encode these fixtures through Multimedia APIs; they do not invoke those fixture-generation tools.

## Audio

- `audio/noise_stereo.wav` and `audio/noise_stereo.mp3`: stereo PCM and MP3 sources.
- `audio/noise_51.m4a`: 5.1 AAC source.
- `audio/noise_51.opus`: 5.1 Opus source.
- `audio/noise_71.wav`: deterministic seeded PCM S16 LE noise, 48 kHz, eight channels and an explicit WAVEFORMATEXTENSIBLE 7.1 speaker mask (`0x63f`). Covers permitted automatic 7.1-to-5.1 encoding to AC-3/E-AC3 without a manual downmix filter.
- `audio/noise_opus_source_51.opus`: explicit 5.1 Opus decoder input.
- `audio/noise_opus_destination_51.opus`: independently encoded 5.1 Opus reference for encode-destination tests.

## Logos

- `logo/watermark-noise-logo.png`: original synthetic 96x96 RGB PNG, with seeded noise on a cyan background and a four-pixel yellow border. Generated with ImageMagick using seed `20261010`; contains no third-party artwork. The colored square makes the watermark distinguishable from the synthetic video noise even at 70% transparency (30% opacity).
- `test_watermark_path_and_embedded_binary` uses this same PNG from its file path and as bytes embedded with `#embed`. It produces `pipeline/watermark/watermark-logo-path.webm` and `pipeline/watermark/watermark-logo-binary.webm` under the configured test-output directory. Both are 640x360 VP9 WebM videos. The test checks their container, codec, resolution and positive duration using `File`, and prints both output paths for manual visual comparison. The automated checks validate video generation, not the appearance of the overlay. No image-generation tool is required to build or run the test.

## Subtitles and attachments

- `subtitles/*.sup`: genuine HDMV PGS bitmap tracks in SUP format; the anime-like set includes Spanish, English and Japanese.
- `subtitles/*.srt`: matching source text cues for subtitle tooling and inspection, except for the independent late-cue regression below.
- `subtitles/late_first_cue.srt`: original synthetic SubRip text containing one cue from `01:00:00,000` to `01:00:02,000`. It exercises a subtitle track whose first cue is timestamped one hour after the start. Reading the fixture does not wait in real time; the test encodes it as ASS in Matroska through `Transcoder` and inspects the output with `File`.
- `attachments/IPAMona-LICENSE.txt`: sidecar IPA Mona redistribution notice and the Gentoo package's declared M+ and public-domain notices. It is not muxed as an attachment because plain-text attachments have no media codec parameters and trigger an expected FFmpeg probe warning.
- `attachments/synthetic-cover.png`: original generated PNG used as a Matroska attachment.

The Japanese PGS was rendered with `IPAMonaGothic` at a larger size for OCR coverage. The test transcodes it to SubRip using the bundled `jpn.traineddata` and checks for the hardcoded Japanese tokens `字幕` and `出発` in the resulting Matroska subtitle packet.

## Test-generated outputs

Generated media is written under the configured `STORMBYTE_TEST_OUTPUT_DIR`, normally `<build>/test-output`. These outputs are not additional redistributed fixtures. Every case uses its own output filename; codec roundtrips generate their intermediate inputs within the same case and do not depend on execution order.

- `pipeline/audio-codecs/`: stereo PCM, surround AAC and Opus fixtures exercise FDK-AAC, Vorbis, Opus, LAME, FLAC, ALAC and AC-3/E-AC3. The cases inspect container and codec identities, channel count, 48 kHz sample rate and positive duration. FDK-AAC is required when FFmpeg is bundled with nonfree enabled.
- `pipeline/video-codecs/`: the H.264 SDR MP4 fixture is scaled to 320x192 for SVT/libaom AV1, VP8/VP9, x264, OpenH264, Kvazaar and x265. AV1 decoder cases generate their own input before selecting dav1d or libaom. The scaled dimensions are multiples of eight for Kvazaar compatibility.
- `pipeline/mux-policy/`: the stereo PCM fixture produces direct FLAC, FLAC in OGA, ALAC in CAF and direct E-AC3. Negative cases cover incompatible codec/container combinations and WebM attachment rejection.
- `pipeline/mp4-policy/`: the five-track H.264/AAC/timed-text MP4 fixture exercises individual and mixed outputs, two audio or subtitle tracks, reversed audio order, all five tracks, `.m4a`/`.m4b` aliases and mixed remux/encode. The HEVC MKV supplies a HEVC/AAC remux case. Its synthetic PNG cover is preserved byte-for-byte in video MP4 and audio-only M4A as cover art, and in Matroska as a real attachment. `File` must expose each cover through `Attachments()` without adding a playable video stream. Matroska additionally preserves the original filename. The IPA Mona font exercises explicit rejection of resources that the MP4 backend cannot represent; this is an implementation limit, not a claim that ISO BMFF forbids embedded resources. Successful outputs are checked for track count, order, codec, language, dimensions, audio properties and duration.
- `pipeline/subtitle-header/`: manual pipelines use the existing MKV and MP4 fixtures as audio sources while reserving ASS or MOV timed-text encoders. Input subtitles are decoded and passed through a test filter that removes their payload before reaching the encoder; all queues have consumers and EOF propagates normally, but no output cues are generated. These cases check that codec preparation does not wait for a first cue and that Matroska retains the selected empty subtitle track. MP4 may omit a track that finishes entirely empty, as libavformat does; the test does not require an artificial empty final track. `late_first_cue.srt` separately exercises encoding of a real cue at one hour, including decoder initialization after the short source reaches EOF.
- `pipeline/watermark/`: both variants use the same colored-noise logo, with distinct output filenames for path and embedded bytes. Overlay appearance is checked manually rather than inferred from successful file generation.

Generated files are inspected with `File`; tests do not require system `ffmpeg`, `ffprobe`, tsMuxer or ImageMagick executables. Optional implementations may be skipped when unavailable; actual encoding failures are not successful skips.

## Invalid inputs

`invalid/random_garbage.mkv` contains random bytes. `invalid/truncated_hevc.mkv` is a deliberately incomplete Matroska header. Both are expected to fail parsing/decoding.
