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

#include <tables/container/table.hxx>

using namespace StormByte::Multimedia::Tables::Container;

namespace {
	constexpr CompatDef table[] = {
		{ "mka",	"4GV" },
		{ "mka",	"8SVX Exponential" },
		{ "mka",	"8SVX Fibonacci" },
		{ "mka",	"AAC" },
		{ "mka",	"AAC LATM" },
		{ "mka",	"AC-3" },
		{ "mka",	"AC-4" },
		{ "mka",	"ACELP Kelvin" },
		{ "mka",	"ADPCM 4X Movie" },
		{ "mka",	"ADPCM AFC" },
		{ "mka",	"ADPCM AGM" },
		{ "mka",	"ADPCM AICA" },
		{ "mka",	"ADPCM Argonaut" },
		{ "mka",	"ADPCM CD-ROM XA" },
		{ "mka",	"ADPCM Circus" },
		{ "mka",	"ADPCM Creative" },
		{ "mka",	"ADPCM DTK" },
		{ "mka",	"ADPCM EA Maxis XA" },
		{ "mka",	"ADPCM EA R1" },
		{ "mka",	"ADPCM EA R2" },
		{ "mka",	"ADPCM EA R3" },
		{ "mka",	"ADPCM EA XAS" },
		{ "mka",	"ADPCM Electronic Arts" },
		{ "mka",	"ADPCM G.722" },
		{ "mka",	"ADPCM G.726" },
		{ "mka",	"ADPCM G.726 LE" },
		{ "mka",	"ADPCM IMA ALP" },
		{ "mka",	"ADPCM IMA AMV" },
		{ "mka",	"ADPCM IMA APC" },
		{ "mka",	"ADPCM IMA APM" },
		{ "mka",	"ADPCM IMA Acorn" },
		{ "mka",	"ADPCM IMA Cunning" },
		{ "mka",	"ADPCM IMA DAT4" },
		{ "mka",	"ADPCM IMA DK3" },
		{ "mka",	"ADPCM IMA DK4" },
		{ "mka",	"ADPCM IMA EA EACS" },
		{ "mka",	"ADPCM IMA EA SEAD" },
		{ "mka",	"ADPCM IMA Escape" },
		{ "mka",	"ADPCM IMA HVQM2" },
		{ "mka",	"ADPCM IMA HVQM4" },
		{ "mka",	"ADPCM IMA ISS" },
		{ "mka",	"ADPCM IMA MTF" },
		{ "mka",	"ADPCM IMA Magix" },
		{ "mka",	"ADPCM IMA Moflex" },
		{ "mka",	"ADPCM IMA OKI" },
		{ "mka",	"ADPCM IMA PDA" },
		{ "mka",	"ADPCM IMA QuickTime" },
		{ "mka",	"ADPCM IMA RAD" },
		{ "mka",	"ADPCM IMA SMJPEG" },
		{ "mka",	"ADPCM IMA SSI" },
		{ "mka",	"ADPCM IMA WAV" },
		{ "mka",	"ADPCM IMA Westwood" },
		{ "mka",	"ADPCM IMA Xbox" },
		{ "mka",	"ADPCM MTAF" },
		{ "mka",	"ADPCM Microsoft" },
		{ "mka",	"ADPCM Nintendo 64" },
		{ "mka",	"ADPCM PSX" },
		{ "mka",	"ADPCM PSXC" },
		{ "mka",	"ADPCM SEGA CRI ADX" },
		{ "mka",	"ADPCM Sanyo" },
		{ "mka",	"ADPCM Shockwave Flash" },
		{ "mka",	"ADPCM Sound Blaster Pro 2" },
		{ "mka",	"ADPCM Sound Blaster Pro 3" },
		{ "mka",	"ADPCM Sound Blaster Pro 4" },
		{ "mka",	"ADPCM THP" },
		{ "mka",	"ADPCM THP LE" },
		{ "mka",	"ADPCM VIMA" },
		{ "mka",	"ADPCM XMD" },
		{ "mka",	"ADPCM Yamaha" },
		{ "mka",	"ADPCM Zork" },
		{ "mka",	"AHX" },
		{ "mka",	"ALAC" },
		{ "mka",	"AMR-NB" },
		{ "mka",	"AMR-WB" },
		{ "mka",	"APAC" },
		{ "mka",	"ATRAC1" },
		{ "mka",	"ATRAC3" },
		{ "mka",	"ATRAC3 AL" },
		{ "mka",	"ATRAC3+" },
		{ "mka",	"ATRAC3+ AL" },
		{ "mka",	"ATRAC9" },
		{ "mka",	"Amazing Studio PAF Audio" },
		{ "mka",	"Apple APAC" },
		{ "mka",	"Bink Audio DCT" },
		{ "mka",	"Bink Audio RDFT" },
		{ "mka",	"Bonk" },
		{ "mka",	"CBD2 DPCM" },
		{ "mka",	"CELT" },
		{ "mka",	"Comfort Noise" },
		{ "mka",	"Codec2" },
		{ "mka",	"DERF DPCM" },
		{ "mka",	"DFPWM" },
		{ "mka",	"DSD LSBF" },
		{ "mka",	"DSD LSBF Planar" },
		{ "mka",	"DSD MSBF" },
		{ "mka",	"DSD MSBF Planar" },
		{ "mka",	"DSS SP" },
		{ "mka",	"DST" },
		{ "mka",	"DTS" },
		{ "mka",	"DV Audio" },
		{ "mka",	"Delphine CIN Audio" },
		{ "mka",	"Discworld II BMV Audio" },
		{ "mka",	"Dolby E" },
		{ "mka",	"E-AC3" },
		{ "mka",	"EVRC" },
		{ "mka",	"FLAC" },
		{ "mka",	"FTR" },
		{ "mka",	"FastAudio" },
		{ "mka",	"G.723.1" },
		{ "mka",	"G.728" },
		{ "mka",	"G.729" },
		{ "mka",	"GSM" },
		{ "mka",	"GSM Microsoft" },
		{ "mka",	"Gremlin DPCM" },
		{ "mka",	"HCA" },
		{ "mka",	"HCOM" },
		{ "mka",	"IAC" },
		{ "mka",	"IMC" },
		{ "mka",	"Interplay ACM" },
		{ "mka",	"Interplay DPCM" },
		{ "mka",	"LC3" },
		{ "mka",	"MACE 3:1" },
		{ "mka",	"MACE 6:1" },
		{ "mka",	"MLP" },
		{ "mka",	"MP1" },
		{ "mka",	"MP2" },
		{ "mka",	"MP3" },
		{ "mka",	"MP3 ADU" },
		{ "mka",	"MP3On4" },
		{ "mka",	"MPEG-4 ALS" },
		{ "mka",	"MPEG-H 3D Audio" },
		{ "mka",	"MSN Siren" },
		{ "mka",	"Micronas Speech" },
		{ "mka",	"Monkey's Audio" },
		{ "mka",	"Musepack SV7" },
		{ "mka",	"Musepack SV8" },
		{ "mka",	"Nellymoser" },
		{ "mka",	"OSQ" },
		{ "mka",	"On2 AVC" },
		{ "mka",	"Opus" },
		{ "mka",	"PCM A-law" },
		{ "mka",	"PCM Blu-ray" },
		{ "mka",	"PCM DVD" },
		{ "mka",	"PCM DVD-Audio" },
		{ "mka",	"PCM F16 LE" },
		{ "mka",	"PCM F24 LE" },
		{ "mka",	"PCM F32 BE" },
		{ "mka",	"PCM F32 LE" },
		{ "mka",	"PCM F64 BE" },
		{ "mka",	"PCM F64 LE" },
		{ "mka",	"PCM LXF" },
		{ "mka",	"PCM S16 BE" },
		{ "mka",	"PCM S16 BE Planar" },
		{ "mka",	"PCM S16 LE" },
		{ "mka",	"PCM S16 LE Planar" },
		{ "mka",	"PCM S24 BE" },
		{ "mka",	"PCM S24 DAUD" },
		{ "mka",	"PCM S24 LE" },
		{ "mka",	"PCM S24 LE Planar" },
		{ "mka",	"PCM S32 BE" },
		{ "mka",	"PCM S32 LE" },
		{ "mka",	"PCM S32 LE Planar" },
		{ "mka",	"PCM S64 BE" },
		{ "mka",	"PCM S64 LE" },
		{ "mka",	"PCM S8" },
		{ "mka",	"PCM S8 Planar" },
		{ "mka",	"PCM SGA" },
		{ "mka",	"PCM U16 BE" },
		{ "mka",	"PCM U16 LE" },
		{ "mka",	"PCM U24 BE" },
		{ "mka",	"PCM U24 LE" },
		{ "mka",	"PCM U32 BE" },
		{ "mka",	"PCM U32 LE" },
		{ "mka",	"PCM U8" },
		{ "mka",	"PCM VIDC" },
		{ "mka",	"PCM Zork" },
		{ "mka",	"PCM μ-law" },
		{ "mka",	"QCELP" },
		{ "mka",	"QDM2" },
		{ "mka",	"QDMC" },
		{ "mka",	"QOA" },
		{ "mka",	"RKA" },
		{ "mka",	"RealAudio 1.0" },
		{ "mka",	"RealAudio 2.0" },
		{ "mka",	"RealAudio Cook" },
		{ "mka",	"RealAudio Lossless" },
		{ "mka",	"RoQ DPCM" },
		{ "mka",	"S302M" },
		{ "mka",	"SBC" },
		{ "mka",	"SDX2 DPCM" },
		{ "mka",	"SMV" },
		{ "mka",	"Shorten" },
		{ "mka",	"Sierra VMD Audio" },
		{ "mka",	"Sipro" },
		{ "mka",	"Siren" },
		{ "mka",	"Smacker Audio" },
		{ "mka",	"Sol DPCM" },
		{ "mka",	"Sonic" },
		{ "mka",	"Sonic LS" },
		{ "mka",	"Speex" },
		{ "mka",	"TAK" },
		{ "mka",	"TTA" },
		{ "mka",	"TrueHD" },
		{ "mka",	"TrueSpeech" },
		{ "mka",	"TwinVQ" },
		{ "mka",	"Vorbis" },
		{ "mka",	"Voxware MetaSound" },
		{ "mka",	"WMA Lossless" },
		{ "mka",	"WMA Pro" },
		{ "mka",	"WMA Voice" },
		{ "mka",	"WMA v1" },
		{ "mka",	"WMA v2" },
		{ "mka",	"Wady DPCM" },
		{ "mka",	"WaveArc" },
		{ "mka",	"WaveSynth" },
		{ "mka",	"WavPack" },
		{ "mka",	"Westwood SND1" },
		{ "mka",	"XMA 1" },
		{ "mka",	"XMA 2" },
		{ "mka",	"Xan DPCM" },
		{ "mka",	"aptX" },
		{ "mka",	"aptX HD" },
		{ "mka",	"iLBC" },

		{ "mks",	"3GPP Timed Text" },
		{ "mks",	"ARIB Caption" },
		{ "mks",	"ASS" },
		{ "mks",	"DVB Subtitle" },
		{ "mks",	"DVB Teletext" },
		{ "mks",	"DVD Subtitle" },
		{ "mks",	"EIA-608" },
		{ "mks",	"HDMV Text" },
		{ "mks",	"IVTV VBI" },
		{ "mks",	"JACOsub" },
		{ "mks",	"MPL2" },
		{ "mks",	"MicroDVD" },
		{ "mks",	"PGS" },
		{ "mks",	"PJS" },
		{ "mks",	"RealText" },
		{ "mks",	"SAMI" },
		{ "mks",	"SRT" },
		{ "mks",	"SSA" },
		{ "mks",	"Spruce STL" },
		{ "mks",	"SubRip" },
		{ "mks",	"SubViewer" },
		{ "mks",	"SubViewer 1" },
		{ "mks",	"TTML" },
		{ "mks",	"UTF-8 Text" },
		{ "mks",	"VPlayer" },
		{ "mks",	"WebVTT" },
		{ "mks",	"XSUB" },

		{ nullptr,	"MJPEG" },
		{ nullptr,	"PNG" },
		{ nullptr,	"TrueType Font" },
		{ nullptr,	"OpenType Font" },
		{ nullptr,	"Binary Data" },
	};
}

std::span<const CompatDef> StormByte::Multimedia::Tables::Container::Matroska() noexcept {
	return table;
}
