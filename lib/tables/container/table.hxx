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

#pragma once

#include <StormByte/size.hxx>
#include <StormByte/multimedia/visibility.h>

#include <array>
#include <cstddef>
#include <span>

/**
 * @namespace StormByte::Multimedia::Tables::Container
 * @brief Private static container identity and codec-compatibility tables.
 */
namespace StormByte::Multimedia::Tables::Container {
	/**
	 * @struct ContainerDef
	 * @brief One catalog row.
	 *
	 * extensions[0] is the primary extension (no dot). Later slots are aliases.
	 */
	struct STORMBYTE_MULTIMEDIA_PRIVATE ContainerDef {
		const char* name;						///< StormByte name
		const char* description;				///< Description
		std::array<const char*, 4> ffmpegIds;	///< FFmpeg format ids; unused slots nullptr
		std::array<const char*, 8> extensions;	///< Primary + aliases; unused slots nullptr
		bool attachments = false;				///< Real AttachedFile (covers, fonts)

		constexpr StormByte::Size FfmpegIdCount() const noexcept {
			StormByte::Size n = 0;
			for (const char* id : ffmpegIds) {
				if (!id)
					break;
				++n;
			}
			return n;
		}

		constexpr const char* FfmpegId(StormByte::Size index) const noexcept {
			if (index >= FfmpegIdCount())
				return nullptr;
			return ffmpegIds[index];
		}

		constexpr StormByte::Size ExtensionCount() const noexcept {
			StormByte::Size n = 0;
			for (const char* ext : extensions) {
				if (!ext)
					break;
				++n;
			}
			return n;
		}

		constexpr const char* Extension(StormByte::Size index) const noexcept {
			if (index >= ExtensionCount())
				return nullptr;
			return extensions[index];
		}

		constexpr const char* PrimaryExtension() const noexcept {
			return Extension(0);
		}
	};

	/**
	 * @struct CompatDef
	 * @brief One allowed codec for a container extension.
	 *
	 * extension == nullptr applies to the primary / unspecified extension.
	 */
	struct STORMBYTE_MULTIMEDIA_PRIVATE CompatDef {
		const char* extension;	///< Extension without dot; nullptr = default
		const char* codec;		///< StormByte codec name
	};

	/**
	 * @brief Container identity rows.
	 * @return Span over the identity table.
	 */
	std::span<const ContainerDef> Identity() noexcept;

	/* Compatibility */

	/** @brief Codec rows for AA. @return Compatibility span. */
	std::span<const CompatDef> AA() noexcept;
	/** @brief Codec rows for AAC. @return Compatibility span. */
	std::span<const CompatDef> AAC() noexcept;
	/** @brief Codec rows for AAX. @return Compatibility span. */
	std::span<const CompatDef> AAX() noexcept;
	/** @brief Codec rows for AC-3. @return Compatibility span. */
	std::span<const CompatDef> AC3() noexcept;
	/** @brief Codec rows for ACE. @return Compatibility span. */
	std::span<const CompatDef> ACE() noexcept;
	/** @brief Codec rows for ACM. @return Compatibility span. */
	std::span<const CompatDef> ACM() noexcept;
	/** @brief Codec rows for ACT. @return Compatibility span. */
	std::span<const CompatDef> ACT() noexcept;
	/** @brief Codec rows for ADF. @return Compatibility span. */
	std::span<const CompatDef> ADF() noexcept;
	/** @brief Codec rows for ADP. @return Compatibility span. */
	std::span<const CompatDef> ADP() noexcept;
	/** @brief Codec rows for ADS. @return Compatibility span. */
	std::span<const CompatDef> ADS() noexcept;
	/** @brief Codec rows for ADX. @return Compatibility span. */
	std::span<const CompatDef> ADX() noexcept;
	/** @brief Codec rows for AEA. @return Compatibility span. */
	std::span<const CompatDef> AEA() noexcept;
	/** @brief Codec rows for AFC. @return Compatibility span. */
	std::span<const CompatDef> AFC() noexcept;
	/** @brief Codec rows for AIFF. @return Compatibility span. */
	std::span<const CompatDef> AIFF() noexcept;
	/** @brief Codec rows for AIX. @return Compatibility span. */
	std::span<const CompatDef> AIX() noexcept;
	/** @brief Codec rows for ALAC raw. @return Compatibility span. */
	std::span<const CompatDef> ALAC() noexcept;
	/** @brief Codec rows for A-law raw. @return Compatibility span. */
	std::span<const CompatDef> ALaw() noexcept;
	/** @brief Codec rows for AMR. @return Compatibility span. */
	std::span<const CompatDef> AMR() noexcept;
	/** @brief Codec rows for AMV. @return Compatibility span. */
	std::span<const CompatDef> AMV() noexcept;
	/** @brief Codec rows for ANM. @return Compatibility span. */
	std::span<const CompatDef> ANM() noexcept;
	/** @brief Codec rows for APC. @return Compatibility span. */
	std::span<const CompatDef> APC() noexcept;
	/** @brief Codec rows for APE. @return Compatibility span. */
	std::span<const CompatDef> APE() noexcept;
	/** @brief Codec rows for APM. @return Compatibility span. */
	std::span<const CompatDef> APM() noexcept;
	/** @brief Codec rows for APNG. @return Compatibility span. */
	std::span<const CompatDef> APNG() noexcept;
	/** @brief Codec rows for aptX. @return Compatibility span. */
	std::span<const CompatDef> APTX() noexcept;
	/** @brief Codec rows for aptX HD. @return Compatibility span. */
	std::span<const CompatDef> APTXHD() noexcept;
	/** @brief Codec rows for AQTitle. @return Compatibility span. */
	std::span<const CompatDef> AQTitle() noexcept;
	/** @brief Codec rows for ASF. @return Compatibility span. */
	std::span<const CompatDef> ASF() noexcept;
	/** @brief Codec rows for AST. @return Compatibility span. */
	std::span<const CompatDef> AST() noexcept;
	/** @brief Codec rows for AU. @return Compatibility span. */
	std::span<const CompatDef> AU() noexcept;
	/** @brief Codec rows for AVI. @return Compatibility span. */
	std::span<const CompatDef> AVI() noexcept;
	/** @brief Codec rows for AVR. @return Compatibility span. */
	std::span<const CompatDef> AVR() noexcept;
	/** @brief Codec rows for AVS. @return Compatibility span. */
	std::span<const CompatDef> AVS() noexcept;
	/** @brief Codec rows for AVS2. @return Compatibility span. */
	std::span<const CompatDef> AVS2() noexcept;
	/** @brief Codec rows for AVS3. @return Compatibility span. */
	std::span<const CompatDef> AVS3() noexcept;
	/** @brief Codec rows for Argo ASF. @return Compatibility span. */
	std::span<const CompatDef> ArgoASF() noexcept;
	/** @brief Codec rows for Argo BRP. @return Compatibility span. */
	std::span<const CompatDef> ArgoBRP() noexcept;
	/** @brief Codec rows for Argo CVG. @return Compatibility span. */
	std::span<const CompatDef> ArgoCVG() noexcept;
	/** @brief Codec rows for Bethesda VID. @return Compatibility span. */
	std::span<const CompatDef> BethsoftVID() noexcept;
	/** @brief Codec rows for BFI. @return Compatibility span. */
	std::span<const CompatDef> BFI() noexcept;
	/** @brief Codec rows for BFSTM. @return Compatibility span. */
	std::span<const CompatDef> BFSTM() noexcept;
	/** @brief Codec rows for BIT. @return Compatibility span. */
	std::span<const CompatDef> BIT() noexcept;
	/** @brief Codec rows for BMV. @return Compatibility span. */
	std::span<const CompatDef> BMV() noexcept;
	/** @brief Codec rows for BOA. @return Compatibility span. */
	std::span<const CompatDef> BOA() noexcept;
	/** @brief Codec rows for BONK. @return Compatibility span. */
	std::span<const CompatDef> BONK() noexcept;
	/** @brief Codec rows for BRSTM. @return Compatibility span. */
	std::span<const CompatDef> BRSTM() noexcept;
	/** @brief Codec rows for Bink. @return Compatibility span. */
	std::span<const CompatDef> Bink() noexcept;
	/** @brief Codec rows for C93. @return Compatibility span. */
	std::span<const CompatDef> C93() noexcept;
	/** @brief Codec rows for CAF. @return Compatibility span. */
	std::span<const CompatDef> CAF() noexcept;
	/** @brief Codec rows for CDG. @return Compatibility span. */
	std::span<const CompatDef> CDG() noexcept;
	/** @brief Codec rows for CDXL. @return Compatibility span. */
	std::span<const CompatDef> CDXL() noexcept;
	/** @brief Codec rows for CINE. @return Compatibility span. */
	std::span<const CompatDef> CINE() noexcept;
	/** @brief Codec rows for Codec2. @return Compatibility span. */
	std::span<const CompatDef> Codec2() noexcept;
	/** @brief Codec rows for Codec2 raw. @return Compatibility span. */
	std::span<const CompatDef> Codec2Raw() noexcept;
	/** @brief Codec rows for DASH. @return Compatibility span. */
	std::span<const CompatDef> DASH() noexcept;
	/** @brief Codec rows for DCSTR. @return Compatibility span. */
	std::span<const CompatDef> DCSTR() noexcept;
	/** @brief Codec rows for DERF. @return Compatibility span. */
	std::span<const CompatDef> DERF() noexcept;
	/** @brief Codec rows for DFA. @return Compatibility span. */
	std::span<const CompatDef> DFA() noexcept;
	/** @brief Codec rows for DFPWM. @return Compatibility span. */
	std::span<const CompatDef> DFPWM() noexcept;
	/** @brief Codec rows for DHAV. @return Compatibility span. */
	std::span<const CompatDef> DHAV() noexcept;
	/** @brief Codec rows for DNxHD. @return Compatibility span. */
	std::span<const CompatDef> DNxHD() noexcept;
	/** @brief Codec rows for DSF. @return Compatibility span. */
	std::span<const CompatDef> DSF() noexcept;
	/** @brief Codec rows for DSI CIN. @return Compatibility span. */
	std::span<const CompatDef> DSICIN() noexcept;
	/** @brief Codec rows for DSS. @return Compatibility span. */
	std::span<const CompatDef> DSS() noexcept;
	/** @brief Codec rows for DTS. @return Compatibility span. */
	std::span<const CompatDef> DTS() noexcept;
	/** @brief Codec rows for DTS-HD. @return Compatibility span. */
	std::span<const CompatDef> DTSHD() noexcept;
	/** @brief Codec rows for DV. @return Compatibility span. */
	std::span<const CompatDef> DV() noexcept;
	/** @brief Codec rows for raw DVB subtitle. @return Compatibility span. */
	std::span<const CompatDef> DVBSub() noexcept;
	/** @brief Codec rows for DXA. @return Compatibility span. */
	std::span<const CompatDef> DXA() noexcept;
	/** @brief Codec rows for D-Cinema audio. @return Compatibility span. */
	std::span<const CompatDef> Daud() noexcept;
	/** @brief Codec rows for Dirac. @return Compatibility span. */
	std::span<const CompatDef> Dirac() noexcept;
	/** @brief Codec rows for EA. @return Compatibility span. */
	std::span<const CompatDef> EA() noexcept;
	/** @brief Codec rows for EA cdata. @return Compatibility span. */
	std::span<const CompatDef> EACdata() noexcept;
	/** @brief Codec rows for E-AC3. @return Compatibility span. */
	std::span<const CompatDef> EAC3() noexcept;
	/** @brief Codec rows for EPAF. @return Compatibility span. */
	std::span<const CompatDef> EPAF() noexcept;
	/** @brief Codec rows for FFmpeg metadata. @return Compatibility span. */
	std::span<const CompatDef> FFMetadata() noexcept;
	/** @brief Codec rows for FITS. @return Compatibility span. */
	std::span<const CompatDef> FITS() noexcept;
	/** @brief Codec rows for FLAC. @return Compatibility span. */
	std::span<const CompatDef> FLAC() noexcept;
	/** @brief Codec rows for FLIC. @return Compatibility span. */
	std::span<const CompatDef> FLIC() noexcept;
	/** @brief Codec rows for FLV. @return Compatibility span. */
	std::span<const CompatDef> FLV() noexcept;
	/** @brief Codec rows for FRM. @return Compatibility span. */
	std::span<const CompatDef> FRM() noexcept;
	/** @brief Codec rows for FSB. @return Compatibility span. */
	std::span<const CompatDef> FSB() noexcept;
	/** @brief Codec rows for FWSE. @return Compatibility span. */
	std::span<const CompatDef> FWSE() noexcept;
	/** @brief Codec rows for Film CPACK. @return Compatibility span. */
	std::span<const CompatDef> FilmCPK() noexcept;
	/** @brief Codec rows for Adobe Filmstrip. @return Compatibility span. */
	std::span<const CompatDef> Filmstrip() noexcept;
	/** @brief Codec rows for 4X Movie. @return Compatibility span. */
	std::span<const CompatDef> FourXM() noexcept;
	/** @brief Codec rows for G.722. @return Compatibility span. */
	std::span<const CompatDef> G722() noexcept;
	/** @brief Codec rows for G.723.1. @return Compatibility span. */
	std::span<const CompatDef> G7231() noexcept;
	/** @brief Codec rows for G.726. @return Compatibility span. */
	std::span<const CompatDef> G726() noexcept;
	/** @brief Codec rows for G.726 LE. @return Compatibility span. */
	std::span<const CompatDef> G726LE() noexcept;
	/** @brief Codec rows for G.729. @return Compatibility span. */
	std::span<const CompatDef> G729() noexcept;
	/** @brief Codec rows for GDV. @return Compatibility span. */
	std::span<const CompatDef> GDV() noexcept;
	/** @brief Codec rows for GENH. @return Compatibility span. */
	std::span<const CompatDef> GENH() noexcept;
	/** @brief Codec rows for GIF. @return Compatibility span. */
	std::span<const CompatDef> GIF() noexcept;
	/** @brief Codec rows for GSM. @return Compatibility span. */
	std::span<const CompatDef> GSM() noexcept;
	/** @brief Codec rows for GXF. @return Compatibility span. */
	std::span<const CompatDef> GXF() noexcept;
	/** @brief Codec rows for H.261. @return Compatibility span. */
	std::span<const CompatDef> H261() noexcept;
	/** @brief Codec rows for H.263. @return Compatibility span. */
	std::span<const CompatDef> H263() noexcept;
	/** @brief Codec rows for H.264. @return Compatibility span. */
	std::span<const CompatDef> H264() noexcept;
	/** @brief Codec rows for H.265. @return Compatibility span. */
	std::span<const CompatDef> H265() noexcept;
	/** @brief Codec rows for HCA. @return Compatibility span. */
	std::span<const CompatDef> HCA() noexcept;
	/** @brief Codec rows for HCOM. @return Compatibility span. */
	std::span<const CompatDef> HCOM() noexcept;
	/** @brief Codec rows for HLS. @return Compatibility span. */
	std::span<const CompatDef> HLS() noexcept;
	/** @brief Codec rows for HNM. @return Compatibility span. */
	std::span<const CompatDef> HNM() noexcept;
	/** @brief Codec rows for ICO. @return Compatibility span. */
	std::span<const CompatDef> ICO() noexcept;
	/** @brief Codec rows for IDCIN. @return Compatibility span. */
	std::span<const CompatDef> IDCIN() noexcept;
	/** @brief Codec rows for IDF. @return Compatibility span. */
	std::span<const CompatDef> IDF() noexcept;
	/** @brief Codec rows for IFF. @return Compatibility span. */
	std::span<const CompatDef> IFF() noexcept;
	/** @brief Codec rows for IFV. @return Compatibility span. */
	std::span<const CompatDef> IFV() noexcept;
	/** @brief Codec rows for iLBC. @return Compatibility span. */
	std::span<const CompatDef> ILBC() noexcept;
	/** @brief Codec rows for IMF. @return Compatibility span. */
	std::span<const CompatDef> IMF() noexcept;
	/** @brief Codec rows for IPMovie. @return Compatibility span. */
	std::span<const CompatDef> IPMovie() noexcept;
	/** @brief Codec rows for IPU. @return Compatibility span. */
	std::span<const CompatDef> IPU() noexcept;
	/** @brief Codec rows for IRCAM. @return Compatibility span. */
	std::span<const CompatDef> IRCAM() noexcept;
	/** @brief Codec rows for ISS. @return Compatibility span. */
	std::span<const CompatDef> ISS() noexcept;
	/** @brief Codec rows for IV8. @return Compatibility span. */
	std::span<const CompatDef> IV8() noexcept;
	/** @brief Codec rows for IVF. @return Compatibility span. */
	std::span<const CompatDef> IVF() noexcept;
	/** @brief Codec rows for IVR. @return Compatibility span. */
	std::span<const CompatDef> IVR() noexcept;
	/** @brief Codec rows for JACOsub. @return Compatibility span. */
	std::span<const CompatDef> JACOsub() noexcept;
	/** @brief Codec rows for JPEG XL Animated. @return Compatibility span. */
	std::span<const CompatDef> JPEGXLAnim() noexcept;
	/** @brief Codec rows for JV. @return Compatibility span. */
	std::span<const CompatDef> JV() noexcept;
	/** @brief Codec rows for KUX. @return Compatibility span. */
	std::span<const CompatDef> KUX() noexcept;
	/** @brief Codec rows for KVAG. @return Compatibility span. */
	std::span<const CompatDef> KVAG() noexcept;
	/** @brief Codec rows for LAF. @return Compatibility span. */
	std::span<const CompatDef> LAF() noexcept;
	/** @brief Codec rows for LMLM4. @return Compatibility span. */
	std::span<const CompatDef> LMLM4() noexcept;
	/** @brief Codec rows for LOAS. @return Compatibility span. */
	std::span<const CompatDef> LOAS() noexcept;
	/** @brief Codec rows for LRC. @return Compatibility span. */
	std::span<const CompatDef> LRC() noexcept;
	/** @brief Codec rows for LVF. @return Compatibility span. */
	std::span<const CompatDef> LVF() noexcept;
	/** @brief Codec rows for LXF. @return Compatibility span. */
	std::span<const CompatDef> LXF() noexcept;
	/** @brief Codec rows for M4V. @return Compatibility span. */
	std::span<const CompatDef> M4V() noexcept;
	/** @brief Codec rows for MCA. @return Compatibility span. */
	std::span<const CompatDef> MCA() noexcept;
	/** @brief Codec rows for MCC. @return Compatibility span. */
	std::span<const CompatDef> MCC() noexcept;
	/** @brief Codec rows for MGSTS. @return Compatibility span. */
	std::span<const CompatDef> MGSTS() noexcept;
	/** @brief Codec rows for MJPEG. @return Compatibility span. */
	std::span<const CompatDef> MJPEG() noexcept;
	/** @brief Codec rows for MJPEG 2000. @return Compatibility span. */
	std::span<const CompatDef> MJPEG2000() noexcept;
	/** @brief Codec rows for MLP. @return Compatibility span. */
	std::span<const CompatDef> MLP() noexcept;
	/** @brief Codec rows for MLV. @return Compatibility span. */
	std::span<const CompatDef> MLV() noexcept;
	/** @brief Codec rows for MM. @return Compatibility span. */
	std::span<const CompatDef> MM() noexcept;
	/** @brief Codec rows for MMF. @return Compatibility span. */
	std::span<const CompatDef> MMF() noexcept;
	/** @brief Codec rows for MODS. @return Compatibility span. */
	std::span<const CompatDef> MODS() noexcept;
	/** @brief Codec rows for MOFLEX. @return Compatibility span. */
	std::span<const CompatDef> MOFLEX() noexcept;
	/** @brief Codec rows for MOV. @return Compatibility span. */
	std::span<const CompatDef> MOV() noexcept;
	/** @brief Codec rows for MP2. @return Compatibility span. */
	std::span<const CompatDef> MP2() noexcept;
	/** @brief Codec rows for MP3. @return Compatibility span. */
	std::span<const CompatDef> MP3() noexcept;
	/** @brief Codec rows for MP4. @return Compatibility span. */
	std::span<const CompatDef> MP4() noexcept;
	/** @brief Codec rows for MPC. @return Compatibility span. */
	std::span<const CompatDef> MPC() noexcept;
	/** @brief Codec rows for MPC8. @return Compatibility span. */
	std::span<const CompatDef> MPC8() noexcept;
	/** @brief Codec rows for MPEG program stream. @return Compatibility span. */
	std::span<const CompatDef> MPEG() noexcept;
	/** @brief Codec rows for MPJPEG. @return Compatibility span. */
	std::span<const CompatDef> MPJPEG() noexcept;
	/** @brief Codec rows for MPL2. @return Compatibility span. */
	std::span<const CompatDef> MPL2() noexcept;
	/** @brief Codec rows for MPSUB. @return Compatibility span. */
	std::span<const CompatDef> MPSUB() noexcept;
	/** @brief Codec rows for MSF. @return Compatibility span. */
	std::span<const CompatDef> MSF() noexcept;
	/** @brief Codec rows for MSP. @return Compatibility span. */
	std::span<const CompatDef> MSP() noexcept;
	/** @brief Codec rows for MTAF. @return Compatibility span. */
	std::span<const CompatDef> MTAF() noexcept;
	/** @brief Codec rows for MTV. @return Compatibility span. */
	std::span<const CompatDef> MTV() noexcept;
	/** @brief Codec rows for MUSX. @return Compatibility span. */
	std::span<const CompatDef> MUSX() noexcept;
	/** @brief Codec rows for MV. @return Compatibility span. */
	std::span<const CompatDef> MV() noexcept;
	/** @brief Codec rows for MVI. @return Compatibility span. */
	std::span<const CompatDef> MVI() noexcept;
	/** @brief Codec rows for MXF. @return Compatibility span. */
	std::span<const CompatDef> MXF() noexcept;
	/** @brief Codec rows for MXG. @return Compatibility span. */
	std::span<const CompatDef> MXG() noexcept;
	/** @brief Codec rows for Matroska. @return Compatibility span. */
	std::span<const CompatDef> Matroska() noexcept;
	/** @brief Codec rows for MicroDVD. @return Compatibility span. */
	std::span<const CompatDef> MicroDVD() noexcept;
	/** @brief Codec rows for MPEG-TS. @return Compatibility span. */
	std::span<const CompatDef> MpegTs() noexcept;
	/** @brief Codec rows for raw MPEG video. @return Compatibility span. */
	std::span<const CompatDef> MpegVideo() noexcept;
	/** @brief Codec rows for NC. @return Compatibility span. */
	std::span<const CompatDef> NC() noexcept;
	/** @brief Codec rows for NIST SPHERE. @return Compatibility span. */
	std::span<const CompatDef> NISTSphere() noexcept;
	/** @brief Codec rows for NSP. @return Compatibility span. */
	std::span<const CompatDef> NSP() noexcept;
	/** @brief Codec rows for NSV. @return Compatibility span. */
	std::span<const CompatDef> NSV() noexcept;
	/** @brief Codec rows for NUT. @return Compatibility span. */
	std::span<const CompatDef> NUT() noexcept;
	/** @brief Codec rows for NUV. @return Compatibility span. */
	std::span<const CompatDef> NUV() noexcept;
	/** @brief Codec rows for OBU. @return Compatibility span. */
	std::span<const CompatDef> OBU() noexcept;
	/** @brief Codec rows for OMA. @return Compatibility span. */
	std::span<const CompatDef> OMA() noexcept;
	/** @brief Codec rows for Ogg. @return Compatibility span. */
	std::span<const CompatDef> Ogg() noexcept;
	/** @brief Codec rows for raw Opus. @return Compatibility span. */
	std::span<const CompatDef> OpusRaw() noexcept;
	/** @brief Codec rows for PAF. @return Compatibility span. */
	std::span<const CompatDef> PAF() noexcept;
	/** @brief Codec rows for PJS. @return Compatibility span. */
	std::span<const CompatDef> PJS() noexcept;
	/** @brief Codec rows for PKM. @return Compatibility span. */
	std::span<const CompatDef> PKM() noexcept;
	/** @brief Codec rows for PMP. @return Compatibility span. */
	std::span<const CompatDef> PMP() noexcept;
	/** @brief Codec rows for PPBnk. @return Compatibility span. */
	std::span<const CompatDef> PPBnk() noexcept;
	/** @brief Codec rows for PVA. @return Compatibility span. */
	std::span<const CompatDef> PVA() noexcept;
	/** @brief Codec rows for PVF. @return Compatibility span. */
	std::span<const CompatDef> PVF() noexcept;
	/** @brief Codec rows for QCP. @return Compatibility span. */
	std::span<const CompatDef> QCP() noexcept;
	/** @brief Codec rows for QOA. @return Compatibility span. */
	std::span<const CompatDef> QOA() noexcept;
	/** @brief Codec rows for R3D. @return Compatibility span. */
	std::span<const CompatDef> R3D() noexcept;
	/** @brief Codec rows for RKA. @return Compatibility span. */
	std::span<const CompatDef> RKA() noexcept;
	/** @brief Codec rows for RL2. @return Compatibility span. */
	std::span<const CompatDef> RL2() noexcept;
	/** @brief Codec rows for RPL. @return Compatibility span. */
	std::span<const CompatDef> RPL() noexcept;
	/** @brief Codec rows for RSD. @return Compatibility span. */
	std::span<const CompatDef> RSD() noexcept;
	/** @brief Codec rows for RSO. @return Compatibility span. */
	std::span<const CompatDef> RSO() noexcept;
	/** @brief Codec rows for RealMedia. @return Compatibility span. */
	std::span<const CompatDef> RealMedia() noexcept;
	/** @brief Codec rows for RealText. @return Compatibility span. */
	std::span<const CompatDef> RealText() noexcept;
	/** @brief Codec rows for RedSpark. @return Compatibility span. */
	std::span<const CompatDef> RedSpark() noexcept;
	/** @brief Codec rows for RoQ. @return Compatibility span. */
	std::span<const CompatDef> RoQ() noexcept;
	/** @brief Codec rows for S337M. @return Compatibility span. */
	std::span<const CompatDef> S337M() noexcept;
	/** @brief Codec rows for SAMI. @return Compatibility span. */
	std::span<const CompatDef> SAMI() noexcept;
	/** @brief Codec rows for SBC. @return Compatibility span. */
	std::span<const CompatDef> SBC() noexcept;
	/** @brief Codec rows for SBG. @return Compatibility span. */
	std::span<const CompatDef> SBG() noexcept;
	/** @brief Codec rows for SCC. @return Compatibility span. */
	std::span<const CompatDef> SCC() noexcept;
	/** @brief Codec rows for SCD. @return Compatibility span. */
	std::span<const CompatDef> SCD() noexcept;
	/** @brief Codec rows for SDNS. @return Compatibility span. */
	std::span<const CompatDef> SDNS() noexcept;
	/** @brief Codec rows for SDR2. @return Compatibility span. */
	std::span<const CompatDef> SDR2() noexcept;
	/** @brief Codec rows for SDS. @return Compatibility span. */
	std::span<const CompatDef> SDS() noexcept;
	/** @brief Codec rows for SDX. @return Compatibility span. */
	std::span<const CompatDef> SDX() noexcept;
	/** @brief Codec rows for SER. @return Compatibility span. */
	std::span<const CompatDef> SER() noexcept;
	/** @brief Codec rows for SGA. @return Compatibility span. */
	std::span<const CompatDef> SGA() noexcept;
	/** @brief Codec rows for SHN. @return Compatibility span. */
	std::span<const CompatDef> SHN() noexcept;
	/** @brief Codec rows for SIFF. @return Compatibility span. */
	std::span<const CompatDef> SIFF() noexcept;
	/** @brief Codec rows for SLN. @return Compatibility span. */
	std::span<const CompatDef> SLN() noexcept;
	/** @brief Codec rows for SMK. @return Compatibility span. */
	std::span<const CompatDef> SMK() noexcept;
	/** @brief Codec rows for SMUSH. @return Compatibility span. */
	std::span<const CompatDef> SMUSH() noexcept;
	/** @brief Codec rows for SOL. @return Compatibility span. */
	std::span<const CompatDef> SOL() noexcept;
	/** @brief Codec rows for SPDIF. @return Compatibility span. */
	std::span<const CompatDef> SPDIF() noexcept;
	/** @brief Codec rows for SRT. @return Compatibility span. */
	std::span<const CompatDef> SRT() noexcept;
	/** @brief Codec rows for STL. @return Compatibility span. */
	std::span<const CompatDef> STL() noexcept;
	/** @brief Codec rows for SUP. @return Compatibility span. */
	std::span<const CompatDef> SUP() noexcept;
	/** @brief Codec rows for SVAG. @return Compatibility span. */
	std::span<const CompatDef> SVAG() noexcept;
	/** @brief Codec rows for SVS. @return Compatibility span. */
	std::span<const CompatDef> SVS() noexcept;
	/** @brief Codec rows for SWF. @return Compatibility span. */
	std::span<const CompatDef> SWF() noexcept;
	/** @brief Codec rows for Simbiosis IMX. @return Compatibility span. */
	std::span<const CompatDef> Simbiosis() noexcept;
	/** @brief Codec rows for SoX. @return Compatibility span. */
	std::span<const CompatDef> SoX() noexcept;
	/** @brief Codec rows for raw Speex. @return Compatibility span. */
	std::span<const CompatDef> SpeexRaw() noexcept;
	/** @brief Codec rows for SubViewer. @return Compatibility span. */
	std::span<const CompatDef> SubViewer() noexcept;
	/** @brief Codec rows for SubViewer 1. @return Compatibility span. */
	std::span<const CompatDef> SubViewer1() noexcept;
	/** @brief Codec rows for TAK. @return Compatibility span. */
	std::span<const CompatDef> TAK() noexcept;
	/** @brief Codec rows for TED captions. @return Compatibility span. */
	std::span<const CompatDef> TEDCaptions() noexcept;
	/** @brief Codec rows for THP. @return Compatibility span. */
	std::span<const CompatDef> THP() noexcept;
	/** @brief Codec rows for TMV. @return Compatibility span. */
	std::span<const CompatDef> TMV() noexcept;
	/** @brief Codec rows for TTA. @return Compatibility span. */
	std::span<const CompatDef> TTA() noexcept;
	/** @brief Codec rows for TTY. @return Compatibility span. */
	std::span<const CompatDef> TTY() noexcept;
	/** @brief Codec rows for TXD. @return Compatibility span. */
	std::span<const CompatDef> TXD() noexcept;
	/** @brief Codec rows for TiVo TY. @return Compatibility span. */
	std::span<const CompatDef> TY() noexcept;
	/** @brief Codec rows for raw Theora. @return Compatibility span. */
	std::span<const CompatDef> TheoraRaw() noexcept;
	/** @brief Codec rows for 3DO STR. @return Compatibility span. */
	std::span<const CompatDef> ThreeDOSTR() noexcept;
	/** @brief Codec rows for 3GPP. @return Compatibility span. */
	std::span<const CompatDef> ThreeGPP() noexcept;
	/** @brief Codec rows for 3GPP2. @return Compatibility span. */
	std::span<const CompatDef> ThreeGPP2() noexcept;
	/** @brief Codec rows for Tiertex SEQ. @return Compatibility span. */
	std::span<const CompatDef> TiertexSEQ() noexcept;
	/** @brief Codec rows for TrueHD. @return Compatibility span. */
	std::span<const CompatDef> TrueHD() noexcept;
	/** @brief Codec rows for V210. @return Compatibility span. */
	std::span<const CompatDef> V210() noexcept;
	/** @brief Codec rows for V210X. @return Compatibility span. */
	std::span<const CompatDef> V210X() noexcept;
	/** @brief Codec rows for VAG. @return Compatibility span. */
	std::span<const CompatDef> VAG() noexcept;
	/** @brief Codec rows for VC-1. @return Compatibility span. */
	std::span<const CompatDef> VC1() noexcept;
	/** @brief Codec rows for VC-1 test. @return Compatibility span. */
	std::span<const CompatDef> VC1Test() noexcept;
	/** @brief Codec rows for VMD. @return Compatibility span. */
	std::span<const CompatDef> VMD() noexcept;
	/** @brief Codec rows for VOC. @return Compatibility span. */
	std::span<const CompatDef> VOC() noexcept;
	/** @brief Codec rows for VPK. @return Compatibility span. */
	std::span<const CompatDef> VPK() noexcept;
	/** @brief Codec rows for VPlayer. @return Compatibility span. */
	std::span<const CompatDef> VPlayer() noexcept;
	/** @brief Codec rows for VQF. @return Compatibility span. */
	std::span<const CompatDef> VQF() noexcept;
	/** @brief Codec rows for Vividas. @return Compatibility span. */
	std::span<const CompatDef> Vividas() noexcept;
	/** @brief Codec rows for Vivo. @return Compatibility span. */
	std::span<const CompatDef> Vivo() noexcept;
	/** @brief Codec rows for VobSub. @return Compatibility span. */
	std::span<const CompatDef> VobSub() noexcept;
	/** @brief Codec rows for raw Vorbis. @return Compatibility span. */
	std::span<const CompatDef> VorbisRaw() noexcept;
	/** @brief Codec rows for W64. @return Compatibility span. */
	std::span<const CompatDef> W64() noexcept;
	/** @brief Codec rows for WADY. @return Compatibility span. */
	std::span<const CompatDef> WADY() noexcept;
	/** @brief Codec rows for WAV. @return Compatibility span. */
	std::span<const CompatDef> WAV() noexcept;
	/** @brief Codec rows for WAVARC. @return Compatibility span. */
	std::span<const CompatDef> WAVARC() noexcept;
	/** @brief Codec rows for WC3. @return Compatibility span. */
	std::span<const CompatDef> WC3() noexcept;
	/** @brief Codec rows for WSD. @return Compatibility span. */
	std::span<const CompatDef> WSD() noexcept;
	/** @brief Codec rows for WTV. @return Compatibility span. */
	std::span<const CompatDef> WTV() noexcept;
	/** @brief Codec rows for WV. @return Compatibility span. */
	std::span<const CompatDef> WV() noexcept;
	/** @brief Codec rows for WVE. @return Compatibility span. */
	std::span<const CompatDef> WVE() noexcept;
	/** @brief Codec rows for WebM. @return Compatibility span. */
	std::span<const CompatDef> WebM() noexcept;
	/** @brief Codec rows for WebP. @return Compatibility span. */
	std::span<const CompatDef> WebP() noexcept;
	/** @brief Codec rows for WebVTT. @return Compatibility span. */
	std::span<const CompatDef> WebVTT() noexcept;
	/** @brief Codec rows for Westwood AUD. @return Compatibility span. */
	std::span<const CompatDef> WestwoodAUD() noexcept;
	/** @brief Codec rows for Westwood VQA. @return Compatibility span. */
	std::span<const CompatDef> WestwoodVQA() noexcept;
	/** @brief Codec rows for XA. @return Compatibility span. */
	std::span<const CompatDef> XA() noexcept;
	/** @brief Codec rows for XBIN. @return Compatibility span. */
	std::span<const CompatDef> XBIN() noexcept;
	/** @brief Codec rows for XMD. @return Compatibility span. */
	std::span<const CompatDef> XMD() noexcept;
	/** @brief Codec rows for XMV. @return Compatibility span. */
	std::span<const CompatDef> XMV() noexcept;
	/** @brief Codec rows for XVAG. @return Compatibility span. */
	std::span<const CompatDef> XVAG() noexcept;
	/** @brief Codec rows for XWMA. @return Compatibility span. */
	std::span<const CompatDef> XWMA() noexcept;
	/** @brief Codec rows for YOP. @return Compatibility span. */
	std::span<const CompatDef> YOP() noexcept;
	/** @brief Codec rows for YUV4MPEG. @return Compatibility span. */
	std::span<const CompatDef> YUV4MPEG() noexcept;
}
