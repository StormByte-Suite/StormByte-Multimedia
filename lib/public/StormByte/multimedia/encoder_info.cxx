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

#include <StormByte/multimedia/encoder_info.hxx>

#include <charconv>
#include <string>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
}

using namespace StormByte::Multimedia;

namespace {
	EncoderOptionType TypeOf(const ::AVOption& option) noexcept {
		switch (option.type) {
			case AV_OPT_TYPE_INT: case AV_OPT_TYPE_INT64: case AV_OPT_TYPE_UINT:
			case AV_OPT_TYPE_UINT64: case AV_OPT_TYPE_FLAGS:
				return EncoderOptionType::Integer;
			case AV_OPT_TYPE_FLOAT: case AV_OPT_TYPE_DOUBLE:
				return EncoderOptionType::Float;
			case AV_OPT_TYPE_BOOL:
				return EncoderOptionType::Boolean;
			case AV_OPT_TYPE_STRING:
				return EncoderOptionType::String;
			default:
				return EncoderOptionType::Other;
		}
	}

	std::string DefaultOf(const ::AVOption& option) {
		switch (option.type) {
			case AV_OPT_TYPE_INT: case AV_OPT_TYPE_INT64: case AV_OPT_TYPE_UINT:
			case AV_OPT_TYPE_UINT64: case AV_OPT_TYPE_FLAGS: case AV_OPT_TYPE_BOOL:
				return std::to_string(option.default_val.i64);
			case AV_OPT_TYPE_FLOAT: case AV_OPT_TYPE_DOUBLE: {
				char text[64];
				const auto result = std::to_chars(text, text + sizeof(text), option.default_val.dbl);
				return std::string(text, result.ptr);
			}
			case AV_OPT_TYPE_STRING:
				return option.default_val.str ? option.default_val.str : "";
			default:
				return {};
		}
	}
}

EncoderOption::EncoderOption(): Type(EncoderOptionType::Other), Minimum(0), Maximum(0) {}

EncoderOption::EncoderOption(const EncoderOption& other) = default;

EncoderOption::EncoderOption(EncoderOption&& other) noexcept = default;

EncoderOption::~EncoderOption() noexcept = default;

EncoderOption& EncoderOption::operator=(const EncoderOption& other) = default;

EncoderOption& EncoderOption::operator=(EncoderOption&& other) noexcept = default;

StormByte::Safe::Optional<EncoderOption> EncoderInfo::Option(std::string_view name) const {
	if (m_name.empty() || name.empty())
		return std::nullopt;
	const ::AVCodec* codec = avcodec_find_encoder_by_name(std::string(m_name).c_str());
	if (!codec)
		return std::nullopt;
	const std::string key(name);
	const ::AVClass* owner = codec->priv_class;
	const ::AVOption* option = owner
		? av_opt_find(&owner, key.c_str(), nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ) : nullptr;
	if (!option) {
		owner = avcodec_get_class();
		option = av_opt_find(&owner, key.c_str(), nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ);
	}
	if (!option || option->type == AV_OPT_TYPE_CONST)
		return std::nullopt;

	EncoderOption out;
	out.Name = StormByte::Safe::String{option->name};
	out.Help = StormByte::Safe::String{option->help ? option->help : ""};
	out.Type = TypeOf(*option);
	out.Minimum = option->min;
	out.Maximum = option->max;
	out.Default = StormByte::Safe::String{DefaultOf(*option)};
	if (option->unit) {
		const ::AVOption* entry = nullptr;
		while ((entry = av_opt_next(&owner, entry))) {
			if (entry->type == AV_OPT_TYPE_CONST && entry->unit && std::string_view{entry->unit} == option->unit)
				out.Choices.push_back(StormByte::Safe::String{entry->name});
		}
	}
	return out;
}
