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
#include <StormByte/multimedia/pipeline/resource_usage.hxx>
#include <StormByte/type_traits.hxx>

#include <array>
#include <string_view>
#include <utility>

using namespace StormByte::Multimedia;
using namespace std::string_view_literals;

static_assert(StormByte::Type::SafeComponent<Pipeline::ResourceUsage>);

namespace {
	int CheckContainerAliases(std::string_view canonical, std::span<const std::string_view> aliases) {
		auto& registry = Registry::Instance();
		auto expected = registry.FindContainer(canonical);
		TEST_REQUIRE(expected);
		for (const auto alias : aliases) {
			auto actual = registry.FindContainer(alias);
			TEST_REQUIRE(actual);
			TEST_REQUIRE(&actual.value().get() == &expected.value().get());
		}
		return 0;
	}
}

int test_matroska_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mkv"sv, "mka"sv, "mks"sv, "mk3d"sv};
	return CheckContainerAliases("Matroska", aliases);
}

int test_mov_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mov"sv, "qt"sv};
	return CheckContainerAliases("MOV", aliases);
}

int test_mp4_extensions_resolve_to_same_container() {
	constexpr std::array aliases{"mp4"sv, "m4a"sv, "m4b"sv, "m4r"sv};
	return CheckContainerAliases("MP4", aliases);
}

int test_mp4_registry_exposes_read_and_write_access() {
	auto container = Registry::Instance().FindContainer("MP4");
	TEST_REQUIRE(container);
	TEST_REQUIRE(container.value().get().HasAccess(Access{Operation::Read}));
	TEST_REQUIRE(container.value().get().HasAccess(Access{Operation::Write}));
	return 0;
}

int test_codec_names_resolve_by_public_identity() {
	auto& registry = Registry::Instance();
	auto canonical = registry.FindCodec("H.265");
	auto ffmpegName = registry.FindCodec("hevc");
	TEST_REQUIRE(canonical);
	TEST_REQUIRE(ffmpegName);
	TEST_REQUIRE(&canonical.value().get() == &ffmpegName.value().get());
	TEST_REQUIRE(canonical.value().get().Type() == StormByte::Multimedia::Type::Video);
	return 0;
}

int test_decoder_info_is_a_safe_read_only_value() {
	constexpr DecoderInfo empty;
	static_assert(empty.Name().empty());
	static_assert(empty.Description().empty());
	static_assert(empty.Features() == Features{});
	StormByte::Safe::Optional<DecoderInfo> optional{empty};
	TEST_REQUIRE(optional);
	auto codec = Registry::Instance().FindCodec("H.265");
	TEST_REQUIRE(codec);
	const auto decoders = codec->get().Decoders();
	TEST_REQUIRE(!decoders.empty());
	TEST_REQUIRE(decoders[0].Name() == "hevc");
	TEST_REQUIRE(!decoders[0].Description().empty());
	TEST_REQUIRE(decoders[0].Features().Has(Feature::TenBit | Feature::HDR10));
	DecoderInfo copy{decoders[0]};
	DecoderInfo moved{std::move(copy)};
	optional = moved;
	TEST_REQUIRE(optional->Name() == decoders[0].Name());
	return 0;
}

int test_encoder_hardware_metadata_matches_ffmpeg_and_copies() {
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio,
		StormByte::Multimedia::Type::Subtitle}) {
		for (const auto codec : Registry::Instance().CodecList(type)) {
			for (const auto& info : codec.get().Encoders()) {
				const bool expected = info.Features().Has(Feature::HardwareAcceleration);
				TEST_REQUIRE(info.Hardware() == expected);
				EncoderInfo copied{info};
				TEST_REQUIRE(copied.Hardware() == info.Hardware());
				EncoderInfo moved{std::move(copied)};
				TEST_REQUIRE(moved.Hardware() == info.Hardware());
			}
		}
	}
	for (const auto name : {"H.264", "H.265", "AV1"}) {
		auto codec = Registry::Instance().FindCodec(name);
		TEST_REQUIRE(codec);
		for (const auto& info : codec->get().Encoders()) {
			if (info.Name().starts_with("lib"))
				TEST_REQUIRE(!info.Hardware());
		}
	}
	return 0;
}

int test_decoders_exclude_native_hardware_only_av1() {
	auto codec = Registry::Instance().FindCodec("AV1");
	TEST_REQUIRE(codec);
	for (const auto& need : {Features{}, Features{Feature::TenBit | Feature::HDR10}}) {
		const auto decoders = codec->get().Decoders(need);
		TEST_REQUIRE(!decoders.empty());
		for (const auto& decoder : decoders)
			TEST_REQUIRE(decoder.Name() != "av1");
	}
	return 0;
}

int test_decoders_filter_each_implementation_without_combining_features() {
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio,
		StormByte::Multimedia::Type::Subtitle, StormByte::Multimedia::Type::Attachment}) {
		for (const auto codec : Registry::Instance().CodecList(type)) {
			const auto all = codec.get().Decoders();
			for (std::size_t i = 0; i < all.size(); ++i) {
				TEST_REQUIRE(!all[i].Name().empty());
				TEST_REQUIRE(!all[i].Description().empty());
				for (std::size_t j = 0; j < i; ++j)
					TEST_REQUIRE(all[i].Name() != all[j].Name());
			}
			for (const auto& need : {Features{Feature::SideData}, Features{Feature::TenBit | Feature::HDR10},
				Features{Feature::SurroundSound}, Features{Feature::DOVI | Feature::HardwareAcceleration}}) {
				const auto filtered = codec.get().Decoders(need);
				TEST_REQUIRE(!filtered.empty() == codec.get().HasAccess(Operation::Read, need));
				std::size_t index = 0;
				for (const auto& info : all) {
					if (!info.Features().Has(need))
						continue;
					TEST_REQUIRE(index < filtered.size());
					TEST_REQUIRE(filtered[index].Name() == info.Name());
					TEST_REQUIRE(filtered[index].Features().Has(need));
					++index;
				}
				TEST_REQUIRE(index == filtered.size());
			}
		}
	}
	return 0;
}

int test_decoders_preserve_generic_and_subtitle_fallbacks() {
	for (const auto name : {"PCM S16 LE", "SRT", "SubRip"}) {
		auto codec = Registry::Instance().FindCodec(name);
		TEST_REQUIRE(codec);
		const auto decoders = codec->get().Decoders();
		TEST_REQUIRE(decoders.size() == 1);
		TEST_REQUIRE(decoders[0].Name() == (name == "PCM S16 LE"sv ? "pcm_s16le"sv : "srt"sv));
		TEST_REQUIRE(decoders[0].Features() == Features{});
		TEST_REQUIRE(codec->get().Decoders(Feature::SideData).empty());
		TEST_REQUIRE(codec->get().Decoders(Feature::Lossless).empty());
	}
	for (const auto codec : Registry::Instance().CodecList(StormByte::Multimedia::Type::Attachment))
		TEST_REQUIRE(codec.get().Decoders().empty());
	return 0;
}

int test_surround_decoder_features_describe_input_channels() {
	for (const auto name : {"AAC", "Vorbis", "Opus", "FLAC", "ALAC"}) {
		auto codec = Registry::Instance().FindCodec(name);
		TEST_REQUIRE(codec);
		const auto decoders = codec->get().Decoders(Feature::SurroundSound);
		TEST_REQUIRE(!decoders.empty());
		for (const auto& decoder : decoders)
			TEST_REQUIRE(decoder.Features().Has(Feature::SurroundSound));
	}
	auto mp3 = Registry::Instance().FindCodec("MP3");
	TEST_REQUIRE(mp3);
	TEST_REQUIRE(mp3->get().Decoders(Feature::SurroundSound).empty());
	return 0;
}

int test_empty_codec_features_preserve_generic_access() {
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio,
		StormByte::Multimedia::Type::Subtitle, StormByte::Multimedia::Type::Attachment}) {
		for (const auto codec : Registry::Instance().CodecList(type)) {
			for (const auto& access : {Access{}, Access{Operation::Read}, Access{Operation::Write},
				Access{Operation::Read | Operation::Write}, Access{Operation::Attach}}) {
				TEST_REQUIRE(codec.get().HasAccess(access, Features{})
					== codec.get().HasAccess(access));
				TEST_REQUIRE(codec.get().HasAccess(access, Feature::None)
					== codec.get().HasAccess(access));
			}
		}
	}
	return 0;
}

int test_codec_features_do_not_combine_unrelated_capabilities() {
	auto hevc = Registry::Instance().FindCodec("H.265");
	TEST_REQUIRE(hevc);
	const auto& codec = hevc->get();
	TEST_REQUIRE(!codec.HasAccess(Operation::Write, Feature::SurroundSound));
	TEST_REQUIRE(!codec.HasAccess(Operation::Read, Feature::SurroundSound));
	TEST_REQUIRE(!codec.HasAccess(Operation::Read | Operation::Write, Feature::SurroundSound));
	TEST_REQUIRE(!codec.HasAccess(Operation::Write, Feature::HDR10 | Feature::SurroundSound));
	TEST_REQUIRE(!codec.HasAccess(Operation::Write, Feature::DOVI | Feature::HardwareAcceleration));
	TEST_REQUIRE(!codec.HasAccess(Operation::Read, Feature::DOVI | Feature::HardwareAcceleration));
	TEST_REQUIRE(!codec.HasAccess(Operation::Attach, Feature::HDR10));
	TEST_REQUIRE(codec.HasAccess(Operation::None, Feature::HDR10));
	return 0;
}

int test_eac3_encoder_features_preserve_surround_sound() {
	auto canonical = Registry::Instance().FindCodec("E-AC3");
	auto alias = Registry::Instance().FindCodec("eac3");
	TEST_REQUIRE(canonical);
	TEST_REQUIRE(alias);
	const auto& codec = canonical->get();
	TEST_REQUIRE(codec.HasAccess(Operation::Write, Feature::SurroundSound)
		== codec.HasAccess(Operation::Write));
	TEST_REQUIRE(alias->get().HasAccess(Operation::Write, Feature::SurroundSound)
		== codec.HasAccess(Operation::Write, Feature::SurroundSound));
	TEST_REQUIRE(!codec.HasAccess(Operation::Write, Feature::HDR10));
	return 0;
}

int test_codec_without_implementation_rows_keeps_only_generic_access() {
	auto pcm = Registry::Instance().FindCodec("PCM S16 LE");
	auto srt = Registry::Instance().FindCodec("SRT");
	TEST_REQUIRE(pcm);
	TEST_REQUIRE(srt);
	TEST_REQUIRE(pcm->get().HasAccess(Operation::Read, Feature::None));
	TEST_REQUIRE(!pcm->get().HasAccess(Operation::Read, Feature::Lossless));
	TEST_REQUIRE(!pcm->get().HasAccess(Operation::Write, Feature::Lossless));
	TEST_REQUIRE(!srt->get().HasAccess(Operation::Read, Feature::SideData));
	return 0;
}

int test_subtitle_kind_classifies_text_and_bitmap() {
	auto& registry = Registry::Instance();
	for (const auto name : {"SRT", "SubRip", "ASS", "SSA", "WebVTT", "TTML",
		"3GPP Timed Text", "mov_text", "subrip", "HDMV Text", "EIA-608"}) {
		auto codec = registry.FindCodec(name);
		TEST_REQUIRE(codec);
		const auto kind = codec->get().SubtitleKind();
		TEST_REQUIRE(kind && kind.value() == SubtitleKind::Text);
	}
	for (const auto name : {"PGS", "hdmv_pgs_subtitle", "DVD Subtitle", "DVB Subtitle", "XSUB"}) {
		auto codec = registry.FindCodec(name);
		TEST_REQUIRE(codec);
		const auto kind = codec->get().SubtitleKind();
		TEST_REQUIRE(kind && kind.value() == SubtitleKind::Bitmap);
	}
	return 0;
}

int test_subtitle_kind_distinguishes_unknown_from_not_applicable() {
	auto& registry = Registry::Instance();
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio,
		StormByte::Multimedia::Type::Attachment}) {
		for (const auto codec : registry.CodecList(type))
			TEST_REQUIRE(!codec.get().SubtitleKind());
	}
	for (const auto codec : registry.CodecList(StormByte::Multimedia::Type::Subtitle))
		TEST_REQUIRE(codec.get().SubtitleKind());
	for (const auto name : {"DVB Teletext", "ARIB Caption", "IVTV VBI"}) {
		auto codec = registry.FindCodec(name);
		TEST_REQUIRE(codec);
		const auto kind = codec->get().SubtitleKind();
		TEST_REQUIRE(kind && kind.value() == SubtitleKind::Unknown);
	}
	return 0;
}

int test_codec_access_matches_pipeline_implementations() {
	auto& registry = Registry::Instance();
	for (const auto type : {StormByte::Multimedia::Type::Video, StormByte::Multimedia::Type::Audio,
		StormByte::Multimedia::Type::Subtitle}) {
		for (const auto codec : registry.CodecList(type))
			TEST_REQUIRE(codec.get().HasAccess(Operation::Write) == !codec.get().Encoders().empty());
	}
	// FFmpeg ships a DTS encoder, but the pipeline has no encoder row for it.
	auto dts = registry.FindCodec("DTS");
	// FFmpeg ships a TTML encoder but no TTML decoder.
	auto ttml = registry.FindCodec("TTML");
	auto h265 = registry.FindCodec("H.265");
	TEST_REQUIRE(dts && ttml && h265);
	TEST_REQUIRE(dts->get().HasAccess(Operation::Read));
	TEST_REQUIRE(!dts->get().HasAccess(Operation::Write));
	TEST_REQUIRE(!ttml->get().HasAccess(Operation::Read));
	TEST_REQUIRE(h265->get().HasAccess(Operation::Read));
	return 0;
}

int test_container_compatibility_follows_its_muxer() {
	auto& registry = Registry::Instance();
	auto mp4 = registry.FindContainer("MP4");
	auto mkv = registry.FindContainer("Matroska");
	auto vp8 = registry.FindCodec("VP8");
	auto webvtt = registry.FindCodec("WebVTT");
	auto ttml = registry.FindCodec("TTML");
	auto timedText = registry.FindCodec("3GPP Timed Text");
	auto h265 = registry.FindCodec("H.265");
	auto pgs = registry.FindCodec("PGS");
	TEST_REQUIRE(mp4 && mkv && vp8 && webvtt && ttml && timedText && h265 && pgs);
	TEST_REQUIRE(mp4->get().HasAccess(Operation::Write));
	TEST_REQUIRE(mkv->get().HasAccess(Operation::Write));
	TEST_REQUIRE(!mp4->get().Allows({vp8.value()}));
	TEST_REQUIRE(!mp4->get().Allows({webvtt.value()}));
	TEST_REQUIRE(!mkv->get().Allows({ttml.value()}));
	TEST_REQUIRE(!mkv->get().Allows({timedText.value()}));
	TEST_REQUIRE(mp4->get().Allows({h265.value(), timedText.value()}));
	TEST_REQUIRE(mkv->get().Allows({h265.value(), vp8.value(), webvtt.value(), pgs.value()}));
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_encoder_hardware_metadata_matches_ffmpeg_and_copies", test_encoder_hardware_metadata_matches_ffmpeg_and_copies},
		TestEntry{"test_decoders_exclude_native_hardware_only_av1", test_decoders_exclude_native_hardware_only_av1},
		TestEntry{"test_decoder_info_is_a_safe_read_only_value", test_decoder_info_is_a_safe_read_only_value},
		TestEntry{"test_decoders_filter_each_implementation_without_combining_features", test_decoders_filter_each_implementation_without_combining_features},
		TestEntry{"test_decoders_preserve_generic_and_subtitle_fallbacks", test_decoders_preserve_generic_and_subtitle_fallbacks},
		TestEntry{"test_surround_decoder_features_describe_input_channels", test_surround_decoder_features_describe_input_channels},
		TestEntry{"test_subtitle_kind_classifies_text_and_bitmap", test_subtitle_kind_classifies_text_and_bitmap},
		TestEntry{"test_subtitle_kind_distinguishes_unknown_from_not_applicable", test_subtitle_kind_distinguishes_unknown_from_not_applicable},
		TestEntry{"test_matroska_extensions_resolve_to_same_container", test_matroska_extensions_resolve_to_same_container},
		TestEntry{"test_mov_extensions_resolve_to_same_container", test_mov_extensions_resolve_to_same_container},
		TestEntry{"test_mp4_extensions_resolve_to_same_container", test_mp4_extensions_resolve_to_same_container},
		TestEntry{"test_mp4_registry_exposes_read_and_write_access", test_mp4_registry_exposes_read_and_write_access},
		TestEntry{"test_codec_names_resolve_by_public_identity", test_codec_names_resolve_by_public_identity},
		TestEntry{"test_empty_codec_features_preserve_generic_access", test_empty_codec_features_preserve_generic_access},
		TestEntry{"test_codec_features_do_not_combine_unrelated_capabilities", test_codec_features_do_not_combine_unrelated_capabilities},
		TestEntry{"test_eac3_encoder_features_preserve_surround_sound", test_eac3_encoder_features_preserve_surround_sound},
		TestEntry{"test_codec_without_implementation_rows_keeps_only_generic_access", test_codec_without_implementation_rows_keeps_only_generic_access},
		TestEntry{"test_codec_access_matches_pipeline_implementations", test_codec_access_matches_pipeline_implementations},
		TestEntry{"test_container_compatibility_follows_its_muxer", test_container_compatibility_follows_its_muxer},
	};
	return RunSelectedTest(argc, argv, tests);
}
