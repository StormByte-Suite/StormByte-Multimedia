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

#include <StormByte/multimedia/codec.hxx>
#include <tables/codec/table.hxx>
#include <tables/decoder/table.hxx>
#include <tables/encoder/table.hxx>

#include <algorithm>
#include <initializer_list>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
}

using namespace StormByte::Multimedia;

Codec::Codec(enum Type type, std::string_view name, std::string_view description, Access access) noexcept
: m_type(type), m_name(name), m_description(description), m_access(access) {}

Codec::Codec(Codec&& other) noexcept = default;

Codec::~Codec() noexcept = default;

Codec& Codec::operator=(Codec&& other) noexcept = default;

bool Codec::operator==(const Codec& other) const noexcept {
	return this == &other;
}

bool Codec::operator!=(const Codec& other) const noexcept {
	return !(*this == other);
}

StormByte::Safe::Optional<enum SubtitleKind> Codec::SubtitleKind() const noexcept {
	if (m_type != Type::Subtitle)
		return std::nullopt;

	int properties = 0;
	for (const auto& row : Tables::Codec::Subtitle()) {
		if (m_name != row.name)
			continue;
		for (std::size_t i = 0; i < row.FfmpegIdCount(); ++i) {
			const auto* descriptor = avcodec_descriptor_get_by_name(row.FfmpegId(i));
			if (descriptor && descriptor->type == AVMEDIA_TYPE_SUBTITLE)
				properties |= descriptor->props;
		}
		break;
	}

	const bool text = (properties & AV_CODEC_PROP_TEXT_SUB) != 0;
	const bool bitmap = (properties & AV_CODEC_PROP_BITMAP_SUB) != 0;
	if (text == bitmap)
		return StormByte::Multimedia::SubtitleKind::Unknown;
	return text ? StormByte::Multimedia::SubtitleKind::Text : StormByte::Multimedia::SubtitleKind::Bitmap;
}

bool Codec::HasAccess(Access access) const noexcept {
	return m_access.Has(access);
}

bool Codec::HasAccess(Access access, const Features& need) const noexcept {
	if (!HasAccess(access))
		return false;
	if (need == Features{})
		return true;
	if (access.Has(Operation::Write) && !Tables::Encoder::PickEncoder(m_name, {}, need))
		return false;
	if (access.Has(Operation::Read)) {
		for (const auto rows : {Tables::Decoder::Video(), Tables::Decoder::Audio()}) {
			for (const auto& row : rows) {
				if (m_name == row.codec && row.features.Has(need)
					&& avcodec_find_decoder_by_name(row.name) != nullptr)
					return true;
			}
		}
		return false;
	}
	return true;
}

StormByte::Safe::Vector<EncoderInfo> Codec::Encoders(const Features& need) const {
	StormByte::Safe::Vector<EncoderInfo> out;
	if (!HasAccess(Operation::Write))
		return out;
	std::vector<const Tables::Encoder::EncoderDef*> rows;
	for (const auto table : {Tables::Encoder::Video(), Tables::Encoder::Audio(), Tables::Encoder::Subtitle()}) {
		for (const auto& row : table) {
			if (m_name == row.codec && row.features.Has(need)
				&& avcodec_find_encoder_by_name(row.name) != nullptr)
				rows.push_back(&row);
		}
	}
	std::stable_sort(rows.begin(), rows.end(), [](const auto* a, const auto* b) {
		return a->preference < b->preference;
	});
	const auto key = [](const char* text) noexcept {
		return text ? std::string_view{text} : std::string_view{};
	};
	for (const auto* row : rows) {
		EncoderInfo info;
		info.m_name = row->name;
		info.m_description = row->description;
		info.m_features = row->features;
		info.m_keys[static_cast<std::size_t>(EncoderSetting::CRF)] = key(row->crf_key);
		info.m_keys[static_cast<std::size_t>(EncoderSetting::BitRate)] = key(row->bitrate_key);
		info.m_keys[static_cast<std::size_t>(EncoderSetting::MaxBitRate)] = key(row->maxrate_key);
		info.m_keys[static_cast<std::size_t>(EncoderSetting::Preset)] = key(row->preset_key);
		info.m_keys[static_cast<std::size_t>(EncoderSetting::Tune)] = key(row->style_key);
		info.m_keys[static_cast<std::size_t>(EncoderSetting::Params)] = key(row->tune_key);
		out.push_back(info);
	}
	return out;
}
