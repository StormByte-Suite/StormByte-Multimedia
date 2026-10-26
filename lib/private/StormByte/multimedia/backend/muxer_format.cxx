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
#include <StormByte/multimedia/backend/muxer_format.hxx>
#include <StormByte/multimedia/codec.hxx>
#include <StormByte/multimedia/container.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <tables/codec/catalog.hxx>

#include <string>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

using namespace StormByte::Multimedia;

namespace {
	// Any token of FFmpeg's comma-separated muxer name that maps to @p container.
	bool FormatServes(const Registry& registry, const AVOutputFormat& format,
		const Container& container) noexcept {
		std::string_view names{format.name ? format.name : ""};
		while (!names.empty()) {
			const auto separator = names.find(',');
			const auto token = names.substr(0, separator);
			if (token == container.Extension())
				return true;
			const auto found = registry.FindContainer(token);
			if (found && found.value().get().Name() == container.Name())
				return true;
			if (separator == std::string_view::npos)
				break;
			names.remove_prefix(separator + 1);
		}
		return false;
	}

	const AVOutputFormat* IdentityFormat(const Registry& registry, const Container& container) noexcept {
		// extensions[0] / the first FFmpeg id is the muxer we mean to open.
		if (!container.Extension().empty()) {
			const std::string intended{container.Extension()};
			if (const auto* format = av_guess_format(intended.c_str(), nullptr, nullptr)) {
				if (!(format->flags & AVFMT_NOFILE) && FormatServes(registry, *format, container))
					return format;
			}
		}

		const AVOutputFormat* fallback = nullptr;
		void* cursor = nullptr;
		while (const auto* format = av_muxer_iterate(&cursor)) {
			if (!format->name || (format->flags & AVFMT_NOFILE) || !FormatServes(registry, *format, container))
				continue;
			if (std::string_view{format->name}.find(',') == std::string_view::npos)
				return format;
			if (!fallback)
				fallback = format;
		}
		return fallback;
	}
}

Backend::MuxerChoice Backend::ResolveMuxer(const Registry& registry, const Container& container) noexcept {
	const auto* format = IdentityFormat(registry, container);
	if (!format)
		return {};
	// Keep the intended id, not FFmpeg's first alias in a comma-separated name.
	const char* name = nullptr;
	if (format->name && std::string_view{format->name}.find(',') == std::string_view::npos)
		name = format->name;
	else if (!container.Extension().empty())
		name = container.Extension().data();
	else
		name = format->name;
	// The muxer reopens it by name; it must land on the same single-file muxer.
	const auto* reopened = name ? av_guess_format(name, nullptr, nullptr) : nullptr;
	if (!reopened || (reopened->flags & AVFMT_NOFILE))
		return {};
	return {reopened, name};
}

bool Backend::MuxerAccepts(const AVOutputFormat& muxer, const Codec& codec) noexcept {
	if (codec.Type() == StormByte::Multimedia::Type::Attachment)
		return true;
	const auto* def = Tables::Codec::Catalog::Instance().Find(codec.Name());
	if (!def)
		return true;
	bool known = false;
	for (std::size_t i = 0; i < def->FfmpegIdCount(); ++i) {
		const char* id = def->FfmpegId(i);
		if (!id)
			continue;
		const AVCodecDescriptor* descriptor = avcodec_descriptor_get_by_name(id);
		if (!descriptor)
			continue;
		known = true;
		if (avformat_query_codec(&muxer, descriptor->id, FF_COMPLIANCE_NORMAL) != 0)
			return true;
	}
	return !known;
}
