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

#include <tables/container/catalog.hxx>

namespace StormByte::Multimedia::Tables::Container {
	namespace {
		struct CompatMap {
			const char* name;
			std::span<const CompatDef> (*rows)() noexcept;
		};

		const CompatMap CompatDispatch[] = {
			{ "3DO STR",			ThreeDOSTR },
			{ "3GPP",				ThreeGPP },
			{ "3GPP2",				ThreeGPP2 },
			{ "4X Movie",			FourXM },
			{ "A-law",				ALaw },
			{ "AA",					AA },
			{ "AAC",				AAC },
			{ "AAX",				AAX },
			{ "AC-3",				AC3 },
			{ "ACE",				ACE },
			{ "ACM",				ACM },
			{ "ACT",				ACT },
			{ "ADF",				ADF },
			{ "ADP",				ADP },
			{ "ADS",				ADS },
			{ "ADX",				ADX },
			{ "AEA",				AEA },
			{ "AFC",				AFC },
			{ "AIFF",				AIFF },
			{ "AIX",				AIX },
			{ "ALAC",				ALAC },
			{ "AMR",				AMR },
			{ "AMV",				AMV },
			{ "ANM",				ANM },
			{ "APC",				APC },
			{ "APE",				APE },
			{ "APM",				APM },
			{ "APNG",				APNG },
			{ "APTX",				APTX },
			{ "APTX HD",			APTXHD },
			{ "AQTitle",			AQTitle },
			{ "ASF",				ASF },
			{ "AST",				AST },
			{ "AU",					AU },
			{ "AVI",				AVI },
			{ "AVR",				AVR },
			{ "AVS",				AVS },
			{ "AVS2",				AVS2 },
			{ "AVS3",				AVS3 },
			{ "Adobe Filmstrip",	Filmstrip },
			{ "Argo ASF",			ArgoASF },
			{ "Argo BRP",			ArgoBRP },
			{ "Argo CVG",			ArgoCVG },
			{ "BETHEX",				BethsoftVID },
			{ "BFI",				BFI },
			{ "BFSTM",				BFSTM },
			{ "BINK",				Bink },
			{ "BIT",				BIT },
			{ "BMV",				BMV },
			{ "BOA",				BOA },
			{ "BONK",				BONK },
			{ "BRSTM",				BRSTM },
			{ "C93",				C93 },
			{ "CAF",				CAF },
			{ "CDG",				CDG },
			{ "CDXL",				CDXL },
			{ "CINE",				CINE },
			{ "Codec2",				Codec2 },
			{ "Codec2 raw",			Codec2Raw },
			{ "DASH",				DASH },
			{ "D-Cinema audio",		Daud },
			{ "DCSTR",				DCSTR },
			{ "DERF",				DERF },
			{ "DFA",				DFA },
			{ "DFPWM",				DFPWM },
			{ "DHAV",				DHAV },
			{ "Dirac",				Dirac },
			{ "DNxHD",				DNxHD },
			{ "DSF",				DSF },
			{ "DSI CIN",			DSICIN },
			{ "DSS",				DSS },
			{ "DTS",				DTS },
			{ "DTSHD",				DTSHD },
			{ "DV",					DV },
			{ "DXA",				DXA },
			{ "EA",					EA },
			{ "EA cdata",			EACdata },
			{ "E-AC3",				EAC3 },
			{ "EPAF",				EPAF },
			{ "FFmpeg metadata",	FFMetadata },
			{ "FITS",				FITS },
			{ "FLAC",				FLAC },
			{ "FLIC",				FLIC },
			{ "FLV",				FLV },
			{ "FRM",				FRM },
			{ "FSB",				FSB },
			{ "FWSE",				FWSE },
			{ "Film CPACK",			FilmCPK },
			{ "G.722",				G722 },
			{ "G.723.1",			G7231 },
			{ "G.726",				G726 },
			{ "G.726 LE",			G726LE },
			{ "G.729",				G729 },
			{ "GDV",				GDV },
			{ "GENH",				GENH },
			{ "GIF",				GIF },
			{ "GSM",				GSM },
			{ "GXF",				GXF },
			{ "H.261",				H261 },
			{ "H.263",				H263 },
			{ "H.264",				H264 },
			{ "H.265",				H265 },
			{ "HCA",				HCA },
			{ "HCOM",				HCOM },
			{ "HLS",				HLS },
			{ "HNM",				HNM },
			{ "ICO",				ICO },
			{ "IDCIN",				IDCIN },
			{ "IDF",				IDF },
			{ "IFF",				IFF },
			{ "IFV",				IFV },
			{ "iLBC",				ILBC },
			{ "IMF",				IMF },
			{ "IPMovie",			IPMovie },
			{ "IPU",				IPU },
			{ "IRCAM",				IRCAM },
			{ "ISS",				ISS },
			{ "IV8",				IV8 },
			{ "IVF",				IVF },
			{ "IVR",				IVR },
			{ "JACOsub",			JACOsub },
			{ "JPEG XL Animated",	JPEGXLAnim },
			{ "JV",					JV },
			{ "KUX",				KUX },
			{ "KVAG",				KVAG },
			{ "LAF",				LAF },
			{ "LMLM4",				LMLM4 },
			{ "LOAS",				LOAS },
			{ "LRC",				LRC },
			{ "LVF",				LVF },
			{ "LXF",				LXF },
			{ "M4V",				M4V },
			{ "MCA",				MCA },
			{ "MCC",				MCC },
			{ "MGSTS",				MGSTS },
			{ "MJPEG",				MJPEG },
			{ "MJPEG 2000",			MJPEG2000 },
			{ "MLP",				MLP },
			{ "MLV",				MLV },
			{ "MM",					MM },
			{ "MMF",				MMF },
			{ "MODS",				MODS },
			{ "MOFLEX",				MOFLEX },
			{ "MOV",				MOV },
			{ "MP2",				MP2 },
			{ "MP3",				MP3 },
			{ "MP4",				MP4 },
			{ "MPC",				MPC },
			{ "MPC8",				MPC8 },
			{ "MPEG-1 System",		MPEG },
			{ "MPEG-2 TS",			MpegTs },
			{ "MPEG video",			MpegVideo },
			{ "MPJPEG",				MPJPEG },
			{ "MPL2",				MPL2 },
			{ "MPSUB",				MPSUB },
			{ "MSF",				MSF },
			{ "MSP",				MSP },
			{ "MTAF",				MTAF },
			{ "MTV",				MTV },
			{ "MUSX",				MUSX },
			{ "MV",					MV },
			{ "MVI",				MVI },
			{ "MXF",				MXF },
			{ "MXG",				MXG },
			{ "Matroska",			Matroska },
			{ "MicroDVD",			MicroDVD },
			{ "NC",					NC },
			{ "NIST SPHERE",		NISTSphere },
			{ "NSP",				NSP },
			{ "NSV",				NSV },
			{ "NUT",				NUT },
			{ "NUV",				NUV },
			{ "OBU",				OBU },
			{ "Ogg",				Ogg },
			{ "OMA",				OMA },
			{ "Opus",				OpusRaw },
			{ "PAF",				PAF },
			{ "PJS",				PJS },
			{ "PKM",				PKM },
			{ "PMP",				PMP },
			{ "PPBnk",				PPBnk },
			{ "PVA",				PVA },
			{ "PVF",				PVF },
			{ "QCP",				QCP },
			{ "QOA",				QOA },
			{ "R3D",				R3D },
			{ "RL2",				RL2 },
			{ "RPL",				RPL },
			{ "RSD",				RSD },
			{ "RSO",				RSO },
			{ "RealMedia",			RealMedia },
			{ "RealText",			RealText },
			{ "RedSpark",			RedSpark },
			{ "RKA",				RKA },
			{ "RoQ",				RoQ },
			{ "S337M",				S337M },
			{ "SAMI",				SAMI },
			{ "SBC",				SBC },
			{ "SBG",				SBG },
			{ "SCC",				SCC },
			{ "SCD",				SCD },
			{ "SDNS",				SDNS },
			{ "SDR2",				SDR2 },
			{ "SDS",				SDS },
			{ "SDX",				SDX },
			{ "SER",				SER },
			{ "SGA",				SGA },
			{ "SHN",				SHN },
			{ "SIFF",				SIFF },
			{ "SIMX",				Simbiosis },
			{ "SLN",				SLN },
			{ "SMK",				SMK },
			{ "SMUSH",				SMUSH },
			{ "SOL",				SOL },
			{ "SoX",				SoX },
			{ "SPDIF",				SPDIF },
			{ "SRT",				SRT },
			{ "STL",				STL },
			{ "SUB",				DVBSub },
			{ "SUP",				SUP },
			{ "SVAG",				SVAG },
			{ "SVS",				SVS },
			{ "SWF",				SWF },
			{ "Speex",				SpeexRaw },
			{ "SubViewer",			SubViewer },
			{ "SubViewer 1",		SubViewer1 },
			{ "TAK",				TAK },
			{ "TED Captions",		TEDCaptions },
			{ "THP",				THP },
			{ "Theora",				TheoraRaw },
			{ "Tiertex SEQ",		TiertexSEQ },
			{ "TMV",				TMV },
			{ "TrueHD",				TrueHD },
			{ "TTA",				TTA },
			{ "TTY",				TTY },
			{ "TXD",				TXD },
			{ "TiVo TY",			TY },
			{ "V210",				V210 },
			{ "V210X",				V210X },
			{ "VAG",				VAG },
			{ "VC-1",				VC1 },
			{ "VC-1 test",			VC1Test },
			{ "VMD",				VMD },
			{ "VOBSUB",				VobSub },
			{ "VOC",				VOC },
			{ "VPK",				VPK },
			{ "VPlayer",			VPlayer },
			{ "VQF",				VQF },
			{ "Vividas",			Vividas },
			{ "Vivo",				Vivo },
			{ "Vorbis",				VorbisRaw },
			{ "W64",				W64 },
			{ "WADY",				WADY },
			{ "WAV",				WAV },
			{ "WAVARC",				WAVARC },
			{ "WC3",				WC3 },
			{ "WebM",				WebM },
			{ "WebVTT",				WebVTT },
			{ "Westwood AUD",		WestwoodAUD },
			{ "Westwood VQA",		WestwoodVQA },
			{ "WSD",				WSD },
			{ "WTV",				WTV },
			{ "WV",					WV },
			{ "WVE",				WVE },
			{ "WebP",				WebP },
			{ "XA",					XA },
			{ "XBIN",				XBIN },
			{ "XMD",				XMD },
			{ "XMV",				XMV },
			{ "XVAG",				XVAG },
			{ "XWMA",				XWMA },
			{ "YOP",				YOP },
			{ "YUV4MPEG",			YUV4MPEG },
		};
	}

	const Catalog& Catalog::Instance() noexcept {
		static Catalog instance;
		return instance;
	}

	Catalog::Catalog() noexcept {
		Initialize();
	}

	void Catalog::Initialize() noexcept {
		const auto identity = Identity();
		m_byName.reserve(identity.size());
		for (const auto& row : identity)
			m_byName.emplace(row.name, &row);

		m_compat.reserve(std::size(CompatDispatch));
		for (const auto& item : CompatDispatch)
			m_compat.emplace(item.name, item.rows());
	}

	std::span<const ContainerDef> Catalog::All() const noexcept {
		return Identity();
	}

	const ContainerDef* Catalog::Find(std::string_view name) const noexcept {
		const auto it = m_byName.find(name);
		return it == m_byName.end() ? nullptr : it->second;
	}

	std::span<const CompatDef> Catalog::Compat(std::string_view name) const noexcept {
		const auto it = m_compat.find(name);
		return it == m_compat.end() ? std::span<const CompatDef>{} : it->second;
	}

	std::span<const CompatDef> Catalog::Compat(const ContainerDef& def) const noexcept {
		return Compat(std::string_view{def.name});
	}
}
