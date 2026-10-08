# Synthetic media fixtures

All video and audio payloads in this directory were generated locally from seeded white noise; each clip is approximately two seconds long. No source film, music, artwork, or other copyrighted media was used. `attachments/synthetic-cover.png` is also generated from a synthetic noise frame.

## Video

- `video/bluray_like_hdr10.mkv`: HEVC Main 10, BT.2020/PQ, mastering-display and content-light metadata; English AC-3 5.1 and Spanish AAC 5.1; English and Spanish PGS; attached synthetic cover.
- `video/bluray_like_hdr10.mp4`: despite the historical filename, this fixture contains H.264 8-bit SDR video with a BT.709 color matrix, two 5.1 AAC tracks and English/Spanish `mov_text` subtitles. Primaries and transfer are unspecified; HDR metadata is absent. MP4 does not carry the PGS tracks used by the MKV cases.
- `video/bluray_like_vp9.mkv`: VP9 video, the same two distinct 5.1 audio codecs and English/Spanish PGS subtitles.
- `video/anime_like.mkv`: exactly one HEVC video, one English Opus 5.1 track, three bitmap PGS tracks (`spa`, `eng`, `jpn`) and one `ipag-mona.ttf` attachment. The font is the IPA Mona font installed from Gentoo's `media-fonts/ipamonafont` package; its redistribution notice remains beside the fixture as `attachments/IPAMona-LICENSE.txt`.
- `video/hdr10_metadata_source.mkv`: HEVC Main 10 source with BT.2020/PQ, mastering-display and content-light metadata.
- `video/hdr_without_mastering_metadata.mkv`: BT.2020/PQ HEVC Main 10 with mastering-display and content-light side-data intentionally absent. `File` is expected to classify it as HDR10-like and report `HDR10::Source::Heuristics` from the signaled primaries/transfer, rather than treating it as SDR.
- `video/xvid_mp3_stereo.avi`: Xvid/MPEG-4 Part 2 video with stereo MP3 audio.

All video/audio container fixtures are encoded from the valid synthetic VP9, audio-noise, or PCM sources; none of their video or audio streams is stream-copied into the tested destination. HDR10 and metadata-free HDR-like HEVC sources are re-encoded with x265's normal GOP settings, then each container's video/audio is encoded again as appropriate. Only subtitle tracks are copied while packaging. The cover and font attachments are added with FFmpeg's Matroska attachment support. PGS tracks were rendered from local synthetic SRT files using tsMuxer, then exported as SUP.

## Audio

- `audio/noise_stereo.wav` and `audio/noise_stereo.mp3`: stereo PCM and MP3 sources.
- `audio/noise_51.m4a`: 5.1 AAC source.
- `audio/noise_51.opus`: 5.1 Opus source.
- `audio/noise_71.wav`: deterministic seeded PCM S16 LE noise, 48 kHz, eight channels and an explicit WAVEFORMATEXTENSIBLE 7.1 speaker mask (`0x63f`). Covers permitted automatic 7.1-to-5.1 encoding to AC-3/E-AC3 without a manual downmix filter.
- `audio/noise_opus_source_51.opus`: explicit 5.1 Opus decoder input.
- `audio/noise_opus_destination_51.opus`: independently encoded 5.1 Opus reference for encode-destination tests.

## Subtitles and attachments

- `subtitles/*.sup`: genuine HDMV PGS bitmap tracks in SUP format; the anime-like set includes Spanish, English and Japanese.
- `subtitles/*.srt`: matching source text cues for subtitle tooling and inspection.
- `attachments/IPAMona-LICENSE.txt`: sidecar IPA Mona redistribution notice and the Gentoo package's declared M+ and public-domain notices. It is not muxed as an attachment because plain-text attachments have no media codec parameters and trigger an expected FFmpeg probe warning.
- `attachments/synthetic-cover.png`: original generated PNG used as a Matroska attachment.

The Japanese PGS was rendered with `IPAMonaGothic` at a larger size for OCR coverage. The test transcodes it to SubRip using the bundled `jpn.traineddata` and checks for the hardcoded Japanese tokens `字幕` and `出発` in the resulting Matroska subtitle packet.

## Invalid inputs

`invalid/random_garbage.mkv` contains random bytes. `invalid/truncated_hevc.mkv` is a deliberately incomplete Matroska header. Both are expected to fail parsing/decoding.
