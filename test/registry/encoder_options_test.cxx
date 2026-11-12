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

#include "../test_helpers.hxx"

#include <StormByte/multimedia/registry.hxx>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
}

using namespace StormByte::Multimedia;

int main() {
	const EncoderInfo empty;
	TEST_REQUIRE(empty.PrivateOptions().empty());
	TEST_REQUIRE(empty.GenericOptions().empty());
	TEST_REQUIRE(!empty.OptionChoice("deadline", "1000000"));
	int tested = 0;
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio, StormByte::Multimedia::Type::Subtitle}) {
		const int media = type == StormByte::Multimedia::Type::Video ? AV_OPT_FLAG_VIDEO_PARAM
			: type == StormByte::Multimedia::Type::Audio ? AV_OPT_FLAG_AUDIO_PARAM : AV_OPT_FLAG_SUBTITLE_PARAM;
		for (const auto codec : Registry::Instance().CodecList(type)) {
			for (const auto& encoder : codec.get().Encoders()) {
				const auto* avcodec = avcodec_find_encoder_by_name(std::string(encoder.Name()).c_str());
				TEST_REQUIRE(avcodec);
				const auto check = [&](bool privateOptions) -> int {
					const AVClass* owner = privateOptions ? avcodec->priv_class : avcodec_get_class();
					const auto options = privateOptions ? encoder.PrivateOptions() : encoder.GenericOptions();
					std::set<std::string> expected;
					const AVOption* option = nullptr;
					while (owner && (option = av_opt_next(&owner, option))) {
						const int mediaFlags = AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_SUBTITLE_PARAM;
						if (option->type == AV_OPT_TYPE_CONST || !(option->flags & AV_OPT_FLAG_ENCODING_PARAM)
							|| (option->flags & AV_OPT_FLAG_READONLY)
							|| ((option->flags & mediaFlags) && !(option->flags & media)))
							continue;
						const AVClass* privateClass = avcodec->priv_class;
						if (!privateOptions && privateClass
							&& av_opt_find(&privateClass, option->name, nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ))
							continue;
						expected.insert(option->name);
					}
					std::set<std::string> actual;
					for (const auto& item : options) {
						const std::string name{static_cast<std::string_view>(item.Name)};
						TEST_REQUIRE(actual.insert(name).second);
						const auto queried = encoder.Option(name);
						TEST_REQUIRE(queried);
						TEST_REQUIRE(queried->Name == item.Name);
						TEST_REQUIRE(queried->Help == item.Help);
						TEST_REQUIRE(queried->Type == item.Type);
						TEST_REQUIRE(queried->Minimum == item.Minimum);
						TEST_REQUIRE(queried->Maximum == item.Maximum);
						TEST_REQUIRE(queried->Default == item.Default);
						TEST_REQUIRE(queried->Choices.size() == item.Choices.size());
						for (std::size_t i = 0; i < item.Choices.size(); ++i)
							TEST_REQUIRE(queried->Choices[i] == item.Choices[i]);
						const auto* native = av_opt_find(&owner, name.c_str(), nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ);
						if (native && native->unit && (native->type == AV_OPT_TYPE_INT || native->type == AV_OPT_TYPE_INT64
							|| native->type == AV_OPT_TYPE_UINT || native->type == AV_OPT_TYPE_UINT64 || native->type == AV_OPT_TYPE_BOOL)) {
							std::map<std::int64_t, std::string> firstNames;
							const AVOption* entry = nullptr;
							while ((entry = av_opt_next(&owner, entry))) {
								if (entry->type == AV_OPT_TYPE_CONST && entry->unit && std::string_view{entry->unit} == native->unit)
									firstNames.try_emplace(entry->default_val.i64, entry->name);
							}
							for (const auto& [value, expectedName] : firstNames) {
								const auto choice = encoder.OptionChoice(name, std::to_string(value));
								TEST_REQUIRE(choice);
								TEST_REQUIRE(static_cast<std::string_view>(*choice) == expectedName);
							}
						}
					}
					TEST_REQUIRE(actual == expected);
					return 0;
				};
				TEST_REQUIRE(check(true) == 0);
				TEST_REQUIRE(check(false) == 0);
				const std::string_view name = encoder.Name();
				const auto privateOptions = encoder.PrivateOptions();
				const auto hasPrivate = [&](std::string_view key) {
					for (const auto& option : privateOptions) {
						if (static_cast<std::string_view>(option.Name) == key)
							return true;
					}
					return false;
				};
				if (name == "pcm_s16le")
					TEST_REQUIRE(privateOptions.empty());
				if (name == "libmp3lame")
					TEST_REQUIRE(hasPrivate("reservoir"));
				if (name == "libvorbis")
					TEST_REQUIRE(hasPrivate("iblock"));
				if (name == "libx265")
					TEST_REQUIRE(hasPrivate("x265-params"));
				if (name == "libvpx-vp9") {
					const auto deadline = encoder.Option("deadline");
					TEST_REQUIRE(deadline);
					TEST_REQUIRE(deadline->Default == StormByte::Safe::String("1000000"));
					const auto choice = encoder.OptionChoice("deadline", "1000000");
					TEST_REQUIRE(choice);
					TEST_REQUIRE(*choice == StormByte::Safe::String("good"));
					TEST_REQUIRE(encoder.OptionChoice("deadline", "0") == StormByte::Safe::String("best"));
					TEST_REQUIRE(encoder.OptionChoice("deadline", "1") == StormByte::Safe::String("realtime"));
					TEST_REQUIRE(!encoder.OptionChoice("deadline", "12345"));
					TEST_REQUIRE(!encoder.OptionChoice("deadline", "good"));
					TEST_REQUIRE(!encoder.OptionChoice("error-resilient", "1"));
					TEST_REQUIRE(!encoder.OptionChoice("unknown", "1"));
				}
				++tested;
			}
		}
	}
	TEST_REQUIRE(tested > 0);
	return 0;
}
