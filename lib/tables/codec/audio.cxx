/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Multimedia.
 *
 * StormByte-Multimedia original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Multimedia source in this
 * file. Third-party components — including FFmpeg and embedded trained data —
 * remain under their own licenses and are not covered by the commercial grant.
 *
 * A written StormByte commercial agreement may license this original source
 * on terms other than the LGPL, including specific use, distribution or
 * linking arrangements such as static linking, as stated in that agreement.
 * It does not grant rights to dependencies or waive their license conditions.
 * Enabling WITH_GPL or WITH_NONFREE may include components with separate
 * obligations for modification, linking (static or dynamic), redistribution
 * or works that incorporate them. The person modifying, linking, packaging or
 * distributing the resulting work is responsible for determining and meeting
 * all applicable requirements, including any needed patent permissions.
 * A StormByte commercial agreement does not provide those rights for GPL or
 * nonfree components.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Multimedia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Multimedia. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <tables/codec/table.hxx>

using namespace StormByte::Multimedia::Tables::Codec;

namespace {
	constexpr CodecDef table[] = {
		{ "4GV",						"Fourth Generation Vocoder",					{ "4gv" } },
		{ "8SVX Exponential",			"Commodore 8SVX exponential",					{ "8svx_exp" } },
		{ "8SVX Fibonacci",				"Commodore 8SVX Fibonacci",						{ "8svx_fib" } },
		{ "AAC",						"Advanced Audio Coding",						{ "aac" } },
		{ "AAC LATM",					"AAC LATM/LOAS",								{ "aac_latm" } },
		{ "AC-3",						"Dolby Digital",								{ "ac3" } },
		{ "AC-4",						"Dolby AC-4",									{ "ac4" } },
		{ "ACELP Kelvin",				"ACELP Kelvin",									{ "acelp.kelvin" } },
		{ "ADPCM 4X Movie",				"ADPCM 4X Movie",								{ "adpcm_4xm" } },
		{ "ADPCM AFC",					"Nintendo AFC ADPCM",							{ "adpcm_afc" } },
		{ "ADPCM AGM",					"ADPCM AmuseGraphics Movie",					{ "adpcm_agm" } },
		{ "ADPCM AICA",					"SEGA AICA ADPCM",								{ "adpcm_aica" } },
		{ "ADPCM Argonaut",				"ADPCM Argonaut Games",							{ "adpcm_argo" } },
		{ "ADPCM CD-ROM XA",			"ADPCM CD-ROM XA",								{ "adpcm_xa" } },
		{ "ADPCM Circus",				"ADPCM Circus",									{ "adpcm_circus" } },
		{ "ADPCM Creative",				"Creative ADPCM",								{ "adpcm_ct" } },
		{ "ADPCM DTK",					"Nintendo DTK ADPCM",							{ "adpcm_dtk" } },
		{ "ADPCM EA Maxis XA",			"ADPCM Electronic Arts Maxis CDROM XA",			{ "adpcm_ea_maxis_xa" } },
		{ "ADPCM EA R1",				"ADPCM Electronic Arts R1",						{ "adpcm_ea_r1" } },
		{ "ADPCM EA R2",				"ADPCM Electronic Arts R2",						{ "adpcm_ea_r2" } },
		{ "ADPCM EA R3",				"ADPCM Electronic Arts R3",						{ "adpcm_ea_r3" } },
		{ "ADPCM EA XAS",				"ADPCM Electronic Arts XAS",					{ "adpcm_ea_xas" } },
		{ "ADPCM Electronic Arts",		"ADPCM Electronic Arts",						{ "adpcm_ea" } },
		{ "ADPCM G.722",				"G.722 ADPCM",									{ "adpcm_g722" } },
		{ "ADPCM G.726",				"G.726 ADPCM",									{ "adpcm_g726" } },
		{ "ADPCM G.726 LE",				"G.726 ADPCM little-endian",					{ "adpcm_g726le" } },
		{ "ADPCM IMA ALP",				"ADPCM IMA High Voltage ALP",					{ "adpcm_ima_alp" } },
		{ "ADPCM IMA AMV",				"ADPCM IMA AMV",								{ "adpcm_ima_amv" } },
		{ "ADPCM IMA APC",				"ADPCM IMA CRYO APC",							{ "adpcm_ima_apc" } },
		{ "ADPCM IMA APM",				"ADPCM IMA Ubisoft APM",						{ "adpcm_ima_apm" } },
		{ "ADPCM IMA Acorn",			"ADPCM IMA Acorn Replay",						{ "adpcm_ima_acorn" } },
		{ "ADPCM IMA Cunning",			"ADPCM IMA Cunning Developments",				{ "adpcm_ima_cunning" } },
		{ "ADPCM IMA DAT4",				"ADPCM IMA Eurocom DAT4",						{ "adpcm_ima_dat4" } },
		{ "ADPCM IMA DK3",				"ADPCM IMA Duck DK3",							{ "adpcm_ima_dk3" } },
		{ "ADPCM IMA DK4",				"ADPCM IMA Duck DK4",							{ "adpcm_ima_dk4" } },
		{ "ADPCM IMA EA EACS",			"ADPCM IMA Electronic Arts EACS",				{ "adpcm_ima_ea_eacs" } },
		{ "ADPCM IMA EA SEAD",			"ADPCM IMA Electronic Arts SEAD",				{ "adpcm_ima_ea_sead" } },
		{ "ADPCM IMA Escape",			"ADPCM IMA Escape",								{ "adpcm_ima_escape" } },
		{ "ADPCM IMA HVQM2",			"ADPCM IMA HVQM2",								{ "adpcm_ima_hvqm2" } },
		{ "ADPCM IMA HVQM4",			"ADPCM IMA HVQM4",								{ "adpcm_ima_hvqm4" } },
		{ "ADPCM IMA ISS",				"ADPCM IMA Funcom ISS",							{ "adpcm_ima_iss" } },
		{ "ADPCM IMA MTF",				"ADPCM IMA Capcom MTF",							{ "adpcm_ima_mtf" } },
		{ "ADPCM IMA Magix",			"ADPCM IMA Magix",								{ "adpcm_ima_magix" } },
		{ "ADPCM IMA Moflex",			"ADPCM IMA MobiClip MOFLEX",					{ "adpcm_ima_moflex" } },
		{ "ADPCM IMA OKI",				"ADPCM IMA OKI",								{ "adpcm_ima_oki" } },
		{ "ADPCM IMA PDA",				"ADPCM IMA PDA",								{ "adpcm_ima_pda" } },
		{ "ADPCM IMA QuickTime",		"ADPCM IMA QuickTime",							{ "adpcm_ima_qt" } },
		{ "ADPCM IMA RAD",				"ADPCM IMA Radical",							{ "adpcm_ima_rad" } },
		{ "ADPCM IMA SMJPEG",			"ADPCM IMA Loki SDL MJPEG",						{ "adpcm_ima_smjpeg" } },
		{ "ADPCM IMA SSI",				"ADPCM IMA Simon & Schuster",					{ "adpcm_ima_ssi" } },
		{ "ADPCM IMA WAV",				"ADPCM IMA WAV",								{ "adpcm_ima_wav" } },
		{ "ADPCM IMA Westwood",			"ADPCM IMA Westwood",							{ "adpcm_ima_ws" } },
		{ "ADPCM IMA Xbox",				"ADPCM IMA Xbox",								{ "adpcm_ima_xbox" } },
		{ "ADPCM MTAF",					"ADPCM MTAF",									{ "adpcm_mtaf" } },
		{ "ADPCM Microsoft",			"ADPCM Microsoft",								{ "adpcm_ms" } },
		{ "ADPCM Nintendo 64",			"Nintendo 64 ADPCM",							{ "adpcm_n64" } },
		{ "ADPCM PSX",					"PlayStation ADPCM",							{ "adpcm_psx" } },
		{ "ADPCM PSXC",					"PlayStation ADPCM (PSXC)",						{ "adpcm_psxc" } },
		{ "ADPCM SEGA CRI ADX",			"SEGA CRI ADX",									{ "adpcm_adx" } },
		{ "ADPCM Sanyo",				"ADPCM Sanyo",									{ "adpcm_sanyo" } },
		{ "ADPCM Shockwave Flash",		"SWF ADPCM",									{ "adpcm_swf" } },
		{ "ADPCM Sound Blaster Pro 2",	"Sound Blaster Pro 2-bit ADPCM",				{ "adpcm_sbpro_2" } },
		{ "ADPCM Sound Blaster Pro 3",	"Sound Blaster Pro 2.6-bit ADPCM",				{ "adpcm_sbpro_3" } },
		{ "ADPCM Sound Blaster Pro 4",	"Sound Blaster Pro 4-bit ADPCM",				{ "adpcm_sbpro_4" } },
		{ "ADPCM THP",					"Nintendo THP ADPCM",							{ "adpcm_thp" } },
		{ "ADPCM THP LE",				"Nintendo THP ADPCM little-endian",				{ "adpcm_thp_le" } },
		{ "ADPCM VIMA",					"ADPCM VIMA",									{ "adpcm_vima" } },
		{ "ADPCM XMD",					"ADPCM Konami XMD",								{ "adpcm_xmd" } },
		{ "ADPCM Yamaha",				"Yamaha ADPCM",									{ "adpcm_yamaha" } },
		{ "ADPCM Zork",					"ADPCM Zork",									{ "adpcm_zork" } },
		{ "AHX",						"CRI ADX AHX",									{ "ahx" } },
		{ "ALAC",						"Apple Lossless",								{ "alac" } },
		{ "AMR-NB",						"Adaptive Multi-Rate Narrowband",				{ "amr_nb" } },
		{ "AMR-WB",						"Adaptive Multi-Rate Wideband",					{ "amr_wb" } },
		{ "APAC",						"Automatic PCM Audio Codec",					{ "apac" } },
		{ "ATRAC1",						"Sony ATRAC1",									{ "atrac1" } },
		{ "ATRAC3",						"Sony ATRAC3",									{ "atrac3" } },
		{ "ATRAC3 AL",					"Sony ATRAC3 Advanced Lossless",				{ "atrac3al" } },
		{ "ATRAC3+",					"Sony ATRAC3+",									{ "atrac3p" } },
		{ "ATRAC3+ AL",					"Sony ATRAC3+ Advanced Lossless",				{ "atrac3pal" } },
		{ "ATRAC9",						"Sony ATRAC9",									{ "atrac9" } },
		{ "Amazing Studio PAF Audio",	"Amazing Studio Packed Animation File audio",	{ "paf_audio" } },
		{ "Apple APAC",					"Apple APAC",									{ "apple_apac" } },
		{ "Bink Audio DCT",				"Bink Audio (DCT)",								{ "binkaudio_dct" } },
		{ "Bink Audio RDFT",			"Bink Audio (RDFT)",							{ "binkaudio_rdft" } },
		{ "Bonk",						"Bonk",											{ "bonk" } },
		{ "CBD2 DPCM",					"CBD2 DPCM",									{ "cbd2_dpcm" } },
		{ "CELT",						"Xiph CELT",									{ "celt" } },
		{ "Comfort Noise",				"RFC 3389 comfort noise",						{ "comfortnoise" } },
		{ "Codec2",						"Codec2",										{ "codec2" } },
		{ "DERF DPCM",					"Xilam DERF DPCM",								{ "derf_dpcm" } },
		{ "DFPWM",						"DFPWM",										{ "dfpwm" } },
		{ "DSD LSBF",					"Direct Stream Digital LSB first",				{ "dsd_lsbf" } },
		{ "DSD LSBF Planar",			"Direct Stream Digital LSB first planar",		{ "dsd_lsbf_planar" } },
		{ "DSD MSBF",					"Direct Stream Digital MSB first",				{ "dsd_msbf" } },
		{ "DSD MSBF Planar",			"Direct Stream Digital MSB first planar",		{ "dsd_msbf_planar" } },
		{ "DSS SP",						"Digital Speech Standard SP",					{ "dss_sp" } },
		{ "DST",						"Direct Stream Transfer",						{ "dst" } },
		{ "DTS",						"DTS Coherent Acoustics",						{ "dca", "dts" } },
		{ "DV Audio",					"DV Audio",										{ "dvaudio" } },
		{ "Delphine CIN Audio",			"Delphine Software CIN audio",					{ "dsicinaudio" } },
		{ "Discworld II BMV Audio",		"Discworld II BMV audio",						{ "bmv_audio" } },
		{ "Dolby E",					"Dolby E",										{ "dolby_e" } },
		{ "E-AC3",						"Dolby Digital Plus",							{ "eac3" } },
		{ "EVRC",						"Enhanced Variable Rate Codec",					{ "evrc" } },
		{ "FLAC",						"Free Lossless Audio Codec",					{ "flac" } },
		{ "FTR",						"FTR Voice",									{ "ftr" } },
		{ "FastAudio",					"FastAudio",									{ "fastaudio" } },
		{ "G.723.1",					"G.723.1",										{ "g723_1" } },
		{ "G.728",						"G.728",										{ "g728" } },
		{ "G.729",						"G.729",										{ "g729" } },
		{ "GSM",						"GSM",											{ "gsm" } },
		{ "GSM Microsoft",				"GSM Microsoft",								{ "gsm_ms" } },
		{ "Gremlin DPCM",				"Gremlin DPCM",									{ "gremlin_dpcm" } },
		{ "HCA",						"CRI HCA",										{ "hca" } },
		{ "HCOM",						"Macintosh HCOM",								{ "hcom" } },
		{ "IAC",						"Indeo Audio Coder",							{ "iac" } },
		{ "IMC",						"Intel Music Coder",							{ "imc" } },
		{ "Interplay ACM",				"Interplay ACM",								{ "interplayacm" } },
		{ "Interplay DPCM",				"Interplay DPCM",								{ "interplay_dpcm" } },
		{ "LC3",						"Bluetooth LC3",								{ "lc3" } },
		{ "MACE 3:1",					"Macintosh Audio Compression 3:1",				{ "mace3" } },
		{ "MACE 6:1",					"Macintosh Audio Compression 6:1",				{ "mace6" } },
		{ "MLP",						"Meridian Lossless Packing",					{ "mlp" } },
		{ "MP1",						"MPEG Audio Layer I",							{ "mp1" } },
		{ "MP2",						"MPEG Audio Layer II",							{ "mp2" } },
		{ "MP3",						"MPEG Audio Layer III",							{ "mp3" } },
		{ "MP3 ADU",					"MP3 ADU",										{ "mp3adu" } },
		{ "MP3On4",						"MP3 in MP4",									{ "mp3on4" } },
		{ "MPEG-4 ALS",					"MPEG-4 Audio Lossless Coding",					{ "mp4als" } },
		{ "MPEG-H 3D Audio",			"MPEG-H 3D Audio",								{ "mpegh_3d_audio" } },
		{ "MSN Siren",					"MSN Siren",									{ "msnsiren" } },
		{ "Micronas Speech",			"Micronas SC-4",								{ "misc4" } },
		{ "Monkey's Audio",				"Monkey's Audio",								{ "ape" } },
		{ "Musepack SV7",				"Musepack SV7",									{ "musepack7" } },
		{ "Musepack SV8",				"Musepack SV8",									{ "musepack8" } },
		{ "Nellymoser",					"Nellymoser Asao",								{ "nellymoser" } },
		{ "OSQ",						"Original Sound Quality",						{ "osq" } },
		{ "On2 AVC",					"On2 Audio for Video Codec",					{ "on2avc" } },
		{ "Opus",						"Opus",											{ "opus" } },
		{ "PCM A-law",					"PCM A-law",									{ "pcm_alaw" } },
		{ "PCM Blu-ray",				"PCM Blu-ray",									{ "pcm_bluray" } },
		{ "PCM DVD",					"PCM DVD",										{ "pcm_dvd" } },
		{ "PCM DVD-Audio",				"PCM DVD-Audio",								{ "pcm_dvda" } },
		{ "PCM F16 LE",					"PCM 16-bit float little-endian",				{ "pcm_f16le" } },
		{ "PCM F24 LE",					"PCM 24-bit float little-endian",				{ "pcm_f24le" } },
		{ "PCM F32 BE",					"PCM 32-bit float big-endian",					{ "pcm_f32be" } },
		{ "PCM F32 LE",					"PCM 32-bit float little-endian",				{ "pcm_f32le" } },
		{ "PCM F64 BE",					"PCM 64-bit float big-endian",					{ "pcm_f64be" } },
		{ "PCM F64 LE",					"PCM 64-bit float little-endian",				{ "pcm_f64le" } },
		{ "PCM LXF",					"PCM LXF",										{ "pcm_lxf" } },
		{ "PCM S16 BE",					"PCM signed 16-bit big-endian",					{ "pcm_s16be" } },
		{ "PCM S16 BE Planar",			"PCM signed 16-bit BE planar",					{ "pcm_s16be_planar" } },
		{ "PCM S16 LE",					"PCM signed 16-bit little-endian",				{ "pcm_s16le" } },
		{ "PCM S16 LE Planar",			"PCM signed 16-bit LE planar",					{ "pcm_s16le_planar" } },
		{ "PCM S24 BE",					"PCM signed 24-bit big-endian",					{ "pcm_s24be" } },
		{ "PCM S24 DAUD",				"PCM signed 24-bit DAUD",						{ "pcm_s24daud" } },
		{ "PCM S24 LE",					"PCM signed 24-bit little-endian",				{ "pcm_s24le" } },
		{ "PCM S24 LE Planar",			"PCM signed 24-bit LE planar",					{ "pcm_s24le_planar" } },
		{ "PCM S32 BE",					"PCM signed 32-bit big-endian",					{ "pcm_s32be" } },
		{ "PCM S32 LE",					"PCM signed 32-bit little-endian",				{ "pcm_s32le" } },
		{ "PCM S32 LE Planar",			"PCM signed 32-bit LE planar",					{ "pcm_s32le_planar" } },
		{ "PCM S64 BE",					"PCM signed 64-bit big-endian",					{ "pcm_s64be" } },
		{ "PCM S64 LE",					"PCM signed 64-bit little-endian",				{ "pcm_s64le" } },
		{ "PCM S8",						"PCM signed 8-bit",								{ "pcm_s8" } },
		{ "PCM S8 Planar",				"PCM signed 8-bit planar",						{ "pcm_s8_planar" } },
		{ "PCM SGA",					"PCM SGA",										{ "pcm_sga" } },
		{ "PCM U16 BE",					"PCM unsigned 16-bit big-endian",				{ "pcm_u16be" } },
		{ "PCM U16 LE",					"PCM unsigned 16-bit little-endian",			{ "pcm_u16le" } },
		{ "PCM U24 BE",					"PCM unsigned 24-bit big-endian",				{ "pcm_u24be" } },
		{ "PCM U24 LE",					"PCM unsigned 24-bit little-endian",			{ "pcm_u24le" } },
		{ "PCM U32 BE",					"PCM unsigned 32-bit big-endian",				{ "pcm_u32be" } },
		{ "PCM U32 LE",					"PCM unsigned 32-bit little-endian",			{ "pcm_u32le" } },
		{ "PCM U8",						"PCM unsigned 8-bit",							{ "pcm_u8" } },
		{ "PCM VIDC",					"PCM Archimedes VIDC",							{ "pcm_vidc" } },
		{ "PCM Zork",					"PCM Zork",										{ "pcm_zork" } },
		{ "PCM μ-law",					"PCM mu-law",									{ "pcm_mulaw" } },
		{ "QCELP",						"Qualcomm PureVoice",							{ "qcelp" } },
		{ "QDM2",						"QDesign Music Codec 2",						{ "qdm2" } },
		{ "QDMC",						"QDesign Music Codec",							{ "qdmc" } },
		{ "QOA",						"Quite OK Audio",								{ "qoa" } },
		{ "RKA",						"RK Audio",										{ "rka" } },
		{ "RealAudio 1.0",				"RealAudio 14.4K",								{ "ra_144" } },
		{ "RealAudio 2.0",				"RealAudio 28.8K",								{ "ra_288" } },
		{ "RealAudio Cook",				"RealAudio Cooker",								{ "cook" } },
		{ "RealAudio Lossless",			"RealAudio Lossless",							{ "ralf" } },
		{ "RoQ DPCM",					"Id RoQ DPCM",									{ "roq_dpcm" } },
		{ "S302M",						"SMPTE 302M",									{ "s302m" } },
		{ "SBC",						"Bluetooth Low Complexity Subband Codec",		{ "sbc" } },
		{ "SDX2 DPCM",					"Squareroot-delta-exact DPCM",					{ "sdx2_dpcm" } },
		{ "SMV",						"Selectable Mode Vocoder",						{ "smv" } },
		{ "Shorten",					"Shorten",										{ "shorten" } },
		{ "Sierra VMD Audio",			"Sierra VMD audio",								{ "vmdaudio" } },
		{ "Sipro",						"RealAudio Sipro",								{ "sipr" } },
		{ "Siren",						"Siren",										{ "siren" } },
		{ "Smacker Audio",				"RAD Smacker audio",							{ "smackaud" } },
		{ "Sol DPCM",					"Sol DPCM",										{ "sol_dpcm" } },
		{ "Sonic",						"FFmpeg Sonic",									{ "sonic" } },
		{ "Sonic LS",					"FFmpeg Sonic lossless",						{ "sonicls" } },
		{ "Speex",						"Speex",										{ "speex" } },
		{ "TAK",						"Tom's lossless Audio Kompressor",				{ "tak" } },
		{ "TTA",						"True Audio",									{ "tta" } },
		{ "TrueHD",						"Dolby TrueHD",									{ "truehd" } },
		{ "TrueSpeech",					"DSP Group TrueSpeech",							{ "truespeech" } },
		{ "TwinVQ",						"NTT TwinVQ",									{ "twinvq" } },
		{ "Vorbis",						"Xiph Vorbis",									{ "vorbis" } },
		{ "Voxware MetaSound",			"Voxware MetaSound",							{ "metasound" } },
		{ "WMA Lossless",				"Windows Media Audio Lossless",					{ "wmalossless" } },
		{ "WMA Pro",					"Windows Media Audio Professional",				{ "wmapro" } },
		{ "WMA Voice",					"Windows Media Audio Voice",					{ "wmavoice" } },
		{ "WMA v1",						"Windows Media Audio 1",						{ "wmav1" } },
		{ "WMA v2",						"Windows Media Audio 2",						{ "wmav2" } },
		{ "Wady DPCM",					"Marble WADY DPCM",								{ "wady_dpcm" } },
		{ "WaveArc",					"WaveArc",										{ "wavarc" } },
		{ "WaveSynth",					"FFmpeg wave synthesizer",						{ "ffwavesynth" } },
		{ "WavPack",					"WavPack",										{ "wavpack" } },
		{ "Westwood SND1",				"Westwood Studios SND1",						{ "westwood_snd1" } },
		{ "XMA 1",						"Xbox Media Audio 1",							{ "xma1" } },
		{ "XMA 2",						"Xbox Media Audio 2",							{ "xma2" } },
		{ "Xan DPCM",					"Origin Xan DPCM",								{ "xan_dpcm" } },
		{ "aptX",						"aptX",											{ "aptx" } },
		{ "aptX HD",					"aptX HD",										{ "aptx_hd" } },
		{ "iLBC",						"Internet Low Bitrate Codec",					{ "ilbc" } },
	};
}

std::span<const CodecDef> StormByte::Multimedia::Tables::Codec::Audio() noexcept {
	return table;
}
