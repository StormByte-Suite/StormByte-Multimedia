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

#include "../helpers.hxx"

#include <StormByte/multimedia/features.hxx>
#include <StormByte/multimedia/file.hxx>
#include <StormByte/multimedia/pipeline/frame.hxx>
#include <StormByte/multimedia/pipeline/filters/video/watermark.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>
#include <tables/decoder/table.hxx>
#include <tables/encoder/table.hxx>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

extern "C" {
	#include <libavutil/dovi_meta.h>
	#include <libavutil/frame.h>
}

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	struct Fixture {
		std::string_view path;
		std::string_view name;
		std::uint8_t profile;
		std::uint8_t compatibility;
		bool hdr10;
	};

	constexpr Fixture dovi_only{"video/dovi_only.mkv", "dovi-only", 5, 0, false};
	constexpr Fixture hdr10_only{"video/hdr10_metadata_source.mkv", "hdr10-only", 0, 0, true};
	constexpr Fixture dovi_hdr10{"video/dovi_hdr10.mkv", "dovi-hdr10", 8, 1, true};

	enum class DoviComparison { Exact, Regenerated };

	struct NativeMetadata {
		AVDOVIMetadata metadata{};
		AVDOVIRpuDataHeader header{};
		AVDOVIDataMapping mapping{};
		AVDOVIColorMetadata color{};
		std::array<AVDOVIDmData, AV_DOVI_MAX_EXT_BLOCKS> extensions{};
	};

	NativeMetadata make_metadata() {
		NativeMetadata native;
		native.metadata.header_offset = offsetof(NativeMetadata, header);
		native.metadata.mapping_offset = offsetof(NativeMetadata, mapping);
		native.metadata.color_offset = offsetof(NativeMetadata, color);
		native.metadata.ext_block_offset = offsetof(NativeMetadata, extensions);
		native.metadata.ext_block_size = sizeof(AVDOVIDmData);
		native.metadata.num_ext_blocks = 1;
		native.header.rpu_type = 2;
		native.header.bl_bit_depth = 10;
		native.header.el_bit_depth = 10;
		native.header.vdr_bit_depth = 12;
		native.mapping.nlq_method_idc = AV_DOVI_NLQ_NONE;
		for (auto& curve : native.mapping.curves) {
			curve.num_pivots = 2;
			curve.pivots[1] = 1023;
		}
		native.color.signal_bit_depth = 10;
		native.color.signal_eotf = 65535;
		native.color.source_min_pq = 7;
		native.color.source_max_pq = 3079;
		for (auto& extension : native.extensions) {
			extension.level = 1;
			extension.l1.min_pq = 7;
			extension.l1.max_pq = 3079;
			extension.l1.avg_pq = 1024;
		}
		return native;
	}

	int check_dovi(const Property::DOVI& dovi, const Fixture& fixture) {
		TEST_REQUIRE(dovi.Present());
		TEST_REQUIRE(dovi.ConfigurationPresent());
		TEST_REQUIRE(dovi.VersionMajor() == 1);
		TEST_REQUIRE(dovi.VersionMinor() == 0);
		TEST_REQUIRE(dovi.Profile() == fixture.profile);
		TEST_REQUIRE(dovi.Level() == 1);
		TEST_REQUIRE(dovi.CompatibilityId() == fixture.compatibility);
		TEST_REQUIRE(dovi.Compression() == 0);
		TEST_REQUIRE(dovi.RpuPresent());
		TEST_REQUIRE(!dovi.ElPresent());
		TEST_REQUIRE(dovi.BlPresent());
		return 0;
	}

	int check_same_dovi(const Property::DOVI& before, const Property::DOVI& after,
		DoviComparison comparison = DoviComparison::Exact) {
		TEST_REQUIRE(after.Present() == before.Present());
		TEST_REQUIRE(after.ConfigurationPresent() == before.ConfigurationPresent());
		TEST_REQUIRE(after.MetadataPresent() == before.MetadataPresent());
		if (comparison == DoviComparison::Exact) {
			TEST_REQUIRE(after.Profile() == before.Profile());
			TEST_REQUIRE(after.Level() == before.Level());
		}
		TEST_REQUIRE(after.VersionMajor() == before.VersionMajor());
		TEST_REQUIRE(after.VersionMinor() == before.VersionMinor());
		TEST_REQUIRE(after.CompatibilityId() == before.CompatibilityId());
		TEST_REQUIRE(after.RpuPresent() == before.RpuPresent());
		TEST_REQUIRE(after.ElPresent() == before.ElPresent());
		TEST_REQUIRE(after.BlPresent() == before.BlPresent());
		TEST_REQUIRE(after.Compression() == before.Compression());
		TEST_REQUIRE(after.BlBitDepth() == before.BlBitDepth());
		TEST_REQUIRE(after.ElBitDepth() == before.ElBitDepth());
		TEST_REQUIRE(after.VdrBitDepth() == before.VdrBitDepth());
		TEST_REQUIRE(after.MappingColorSpace() == before.MappingColorSpace());
		TEST_REQUIRE(after.MappingChromaFormat() == before.MappingChromaFormat());
		TEST_REQUIRE(after.SignalBitDepth() == before.SignalBitDepth());
		TEST_REQUIRE(after.SignalColorSpace() == before.SignalColorSpace());
		TEST_REQUIRE(after.SignalChromaFormat() == before.SignalChromaFormat());
		TEST_REQUIRE(after.SignalFullRange() == before.SignalFullRange());
		TEST_REQUIRE(after.SignalEotf() == before.SignalEotf());
		TEST_REQUIRE(after.SourceMinPq() == before.SourceMinPq());
		TEST_REQUIRE(after.SourceMaxPq() == before.SourceMaxPq());
		if (comparison == DoviComparison::Exact)
			TEST_REQUIRE(std::ranges::equal(after.Rpu(), before.Rpu()));
		else {
			TEST_REQUIRE(before.MetadataPresent());
			TEST_REQUIRE(!before.Rpu().empty());
			TEST_REQUIRE(!after.Rpu().empty());
			TEST_REQUIRE(after.Compression() == 0);
		}
		return 0;
	}

	int check_invalid_metadata(const NativeMetadata& native, std::size_t size = sizeof(NativeMetadata)) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{FixturePath(dovi_only.path).string()});
		TEST_REQUIRE(opened);
		const auto video = opened.value().Streams()[0].Video();
		TEST_REQUIRE(video && video.value().DOVI());
		auto dovi = video.value().DOVI().value();
		TEST_REQUIRE(dovi.MetadataPresent());
		TEST_REQUIRE(!dovi.Rpu().empty());
		const auto saved = dovi;
		TEST_REQUIRE(!dovi.LoadMetadata(std::as_bytes(std::span{&native, 1}).first(size)));
		TEST_REQUIRE(check_same_dovi(saved, dovi) == 0);
		return 0;
	}

	int check_valid_metadata(std::span<const std::byte> bytes) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{FixturePath(dovi_only.path).string()});
		TEST_REQUIRE(opened);
		const auto video = opened.value().Streams()[0].Video();
		TEST_REQUIRE(video && video.value().DOVI());
		auto dovi = video.value().DOVI().value();
		const auto saved = dovi;
		TEST_REQUIRE(dovi.LoadMetadata(bytes));
		TEST_REQUIRE(check_dovi(dovi, dovi_only) == 0);
		TEST_REQUIRE(std::ranges::equal(dovi.Rpu(), saved.Rpu()));
		TEST_REQUIRE(dovi.MetadataPresent());
		TEST_REQUIRE(dovi.BlBitDepth() == 10);
		TEST_REQUIRE(dovi.ElBitDepth() == 10);
		TEST_REQUIRE(dovi.VdrBitDepth() == 12);
		TEST_REQUIRE(dovi.MappingColorSpace() == 0);
		TEST_REQUIRE(dovi.MappingChromaFormat() == 0);
		TEST_REQUIRE(dovi.SignalBitDepth() == 10);
		TEST_REQUIRE(dovi.SignalColorSpace() == 0);
		TEST_REQUIRE(dovi.SignalChromaFormat() == 0);
		TEST_REQUIRE(dovi.SignalFullRange() == 0);
		TEST_REQUIRE(dovi.SignalEotf() == 65535);
		TEST_REQUIRE(dovi.SourceMinPq() == 7);
		TEST_REQUIRE(dovi.SourceMaxPq() == 3079);
		return 0;
	}

	bool same_point(const Property::Point& before, const Property::Point& after) {
		return before.X() == after.X() && before.Y() == after.Y();
	}

	int check_same_video(const Property::Video& before, const Property::Video& after,
		DoviComparison comparison = DoviComparison::Exact) {
		TEST_REQUIRE(after.Resolution().Width() == before.Resolution().Width());
		TEST_REQUIRE(after.Resolution().Height() == before.Resolution().Height());
		TEST_REQUIRE(after.Color().PixelFormat() == before.Color().PixelFormat());
		TEST_REQUIRE(after.Color().Space() == before.Color().Space());
		TEST_REQUIRE(after.Color().Range() == before.Color().Range());
		TEST_REQUIRE(after.Color().Primaries() == before.Color().Primaries());
		TEST_REQUIRE(after.Color().Transfer() == before.Color().Transfer());
		TEST_REQUIRE(after.DOVI().has_value() == before.DOVI().has_value());
		TEST_REQUIRE(after.HDR10().has_value() == before.HDR10().has_value());
		if (before.DOVI())
			TEST_REQUIRE(check_same_dovi(before.DOVI().value(), after.DOVI().value(), comparison) == 0);
		if (before.HDR10()) {
			const auto original = before.HDR10().value();
			const auto actual = after.HDR10().value();
			TEST_REQUIRE(actual.Origin() == original.Origin());
			TEST_REQUIRE(actual.IsHDR10Plus() == original.IsHDR10Plus());
			TEST_REQUIRE(same_point(original.Red(), actual.Red()));
			TEST_REQUIRE(same_point(original.Green(), actual.Green()));
			TEST_REQUIRE(same_point(original.Blue(), actual.Blue()));
			TEST_REQUIRE(same_point(original.White(), actual.White()));
			TEST_REQUIRE(same_point(original.Luminance(), actual.Luminance()));
			TEST_REQUIRE(actual.LightLevel().has_value() == original.LightLevel().has_value());
			if (original.LightLevel())
				TEST_REQUIRE(same_point(original.LightLevel().value(), actual.LightLevel().value()));
		}
		return 0;
	}

	int check_fixture(const Fixture& fixture) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{FixturePath(fixture.path).string()});
		TEST_REQUIRE(opened);
		TEST_REQUIRE(opened.value().Container().Name() == "Matroska");
		TEST_REQUIRE(opened.value().Streams().size() == 1);
		const auto stream = opened.value().Streams()[0];
		TEST_REQUIRE(stream.Index() == 0);
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(stream.Codec().Name() == "H.265");
		const auto video = stream.Video();
		TEST_REQUIRE(video);
		TEST_REQUIRE(video.value().DOVI().has_value() == (fixture.profile != 0));
		TEST_REQUIRE(video.value().HDR10().has_value() == fixture.hdr10);
		TEST_REQUIRE(video.value().Color().PixelFormat() == Property::PixelFormat::YUV420P10);
		if (fixture.profile == 5)
			TEST_REQUIRE(!video.value().HDR10());
		if (fixture.profile != 0) {
			TEST_REQUIRE(video.value().Resolution().Width() == 320);
			TEST_REQUIRE(video.value().Resolution().Height() == 192);
			TEST_REQUIRE(check_dovi(video.value().DOVI().value(), fixture) == 0);
			TEST_REQUIRE(video.value().DOVI()->MetadataPresent());
			TEST_REQUIRE(!video.value().DOVI()->Rpu().empty());
			const auto rate = video.value().FrameRate();
			TEST_REQUIRE(rate && rate.value().num == 24 * rate.value().den);
		}
		if (fixture.hdr10) {
			TEST_REQUIRE(video.value().HDR10()->Origin() == Property::HDR10::Source::Metadata);
			TEST_REQUIRE(video.value().Color().Primaries() == Property::Primaries::BT2020);
			TEST_REQUIRE(video.value().Color().Transfer() == Property::Transfer::SMPTE2084);
			if (fixture.profile != 0) {
				const auto hdr = video.value().HDR10().value();
				TEST_REQUIRE(same_point(hdr.Red(), Property::Point{34000, 16000}));
				TEST_REQUIRE(same_point(hdr.Green(), Property::Point{13250, 34500}));
				TEST_REQUIRE(same_point(hdr.Blue(), Property::Point{7500, 3000}));
				TEST_REQUIRE(same_point(hdr.White(), Property::Point{15635, 16450}));
				TEST_REQUIRE(same_point(hdr.Luminance(), Property::Point{50, 40000000}));
				TEST_REQUIRE(hdr.LightLevel());
				TEST_REQUIRE(same_point(hdr.LightLevel().value(), Property::Point{636, 103}));
			}
		}
		else if (fixture.profile == 5) {
			TEST_REQUIRE(video.value().Color().Space() == Property::Space::IPTC2);
			TEST_REQUIRE(video.value().Color().Range() == Property::Range::Full);
			TEST_REQUIRE(video.value().Color().Primaries() == Property::Primaries::BT2020);
			TEST_REQUIRE(video.value().Color().Transfer() == Property::Transfer::SMPTE2084);
		}
		const auto duration = opened.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		return 0;
	}

	struct FrameAudit {
		StormByte::Safe::Vector<Property::Video> videos;
		StormByte::Safe::Vector<int> side_data_counts;
		StormByte::Safe::Vector<int> dovi_metadata_counts;
		StormByte::Safe::Vector<int> dovi_rpu_counts;
	};

	int CountSideData(const StormByte::Multimedia::FFmpeg::AVFrame& frame,
		AVFrameSideDataType type) noexcept {
		int count = 0;
		for (int index = 0; index < frame.SideDataCount(); ++index) {
			const AVFrameSideData* side = frame.SideDataAt(index);
			if (side && side->type == type)
				++count;
		}
		return count;
	}

	struct EncodedAudit {
		FrameAudit source;
		FrameAudit destination;
	};

	class AuditEncodedFrames final: public Filter::Analytics {
		public:
			AuditEncodedFrames(StormByte::Safe::Shared<StormByte::Logger::Log> logger,
				StormByte::Safe::Shared<EncodedAudit> audit)
			: Filter::Analytics(std::move(logger), "audit-encoded-dovi"), m_audit(std::move(audit)) {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

		protected:
			void Clean() noexcept override {}
			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				if (!frame.Video()) {
					Fail("Decoded DOVI test frame has no video properties");
					return;
				}
				if (frame.Producer() == Producer::Decoder) {
					m_audit->source.videos.push_back(frame.Video().value());
					const auto& handle = AVFrame();
					m_audit->source.side_data_counts.push_back(handle.SideDataCount());
					m_audit->source.dovi_metadata_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_METADATA));
					m_audit->source.dovi_rpu_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_RPU_BUFFER));
				}
				else if (frame.Producer() == Producer::Encoder || frame.Producer() == Producer::Remuxer) {
					m_audit->destination.videos.push_back(frame.Video().value());
					const auto& handle = AVFrame();
					m_audit->destination.side_data_counts.push_back(handle.SideDataCount());
					m_audit->destination.dovi_metadata_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_METADATA));
					m_audit->destination.dovi_rpu_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_RPU_BUFFER));
				}
			}

		private:
			StormByte::Safe::Shared<EncodedAudit> m_audit;
	};

	class AuditFrames final: public Filter::Process {
		public:
			AuditFrames(StormByte::Safe::Shared<StormByte::Logger::Log> logger,
				StormByte::Safe::Shared<FrameAudit> audit)
			: Filter::Process(std::move(logger), "audit-dovi"), m_audit(std::move(audit)) {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

		protected:
			void Clean() noexcept override {}
			void Setup() noexcept override {}

			void Process(const Frame& frame) noexcept override {
				if (!frame.Video()) {
					Fail("DOVI test frame has no video properties");
					return;
				}
				Frame copy{frame};
				Frame moved{std::move(copy)};
				copy = moved;
				Frame assigned;
				assigned = std::move(copy);
				m_audit->videos.push_back(assigned.Video().value());
				const auto& handle = AVFrame();
				m_audit->side_data_counts.push_back(handle.SideDataCount());
				m_audit->dovi_metadata_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_METADATA));
				m_audit->dovi_rpu_counts.push_back(CountSideData(handle, AV_FRAME_DATA_DOVI_RPU_BUFFER));
			}

		private:
			StormByte::Safe::Shared<FrameAudit> m_audit;
	};

	class ReplacePixels final: public Filter::Process {
		public:
			explicit ReplacePixels(StormByte::Safe::Shared<StormByte::Logger::Log> logger)
			: Filter::Process(std::move(logger), "replace-dovi-pixels") {}

			StormByte::Multimedia::Type Media() const noexcept override {
				return StormByte::Multimedia::Type::Video;
			}

		protected:
			void Clean() noexcept override {}
			void Setup() noexcept override {}

			void Process(const Frame&) noexcept override {
				StormByte::Multimedia::FFmpeg::AVFrame replacement;
				const auto& source = AVFrame();
				if (!replacement.AllocVideo(source.Width(), source.Height(), source.Format())
					|| !replacement.Copy(source)) {
					Fail("DOVI test could not replace video planes");
					return;
				}
				Save(std::move(replacement));
			}
	};

	int check_decoded_frames(const FrameAudit& before, const FrameAudit& after, const Fixture& fixture,
		DoviComparison comparison = DoviComparison::Exact) {
		TEST_PHASE("checking decoded frame counts and HDR/DOVI side data");
		TEST_REQUIRE(!before.videos.empty());
		TEST_REQUIRE(after.videos.size() == before.videos.size());
		TEST_REQUIRE(before.side_data_counts.size() == before.videos.size());
		TEST_REQUIRE(after.side_data_counts.size() == after.videos.size());
		TEST_REQUIRE(before.dovi_metadata_counts.size() == before.videos.size());
		TEST_REQUIRE(before.dovi_rpu_counts.size() == before.videos.size());
		TEST_REQUIRE(after.dovi_metadata_counts.size() == after.videos.size());
		TEST_REQUIRE(after.dovi_rpu_counts.size() == after.videos.size());
		if (fixture.profile != 0)
			TEST_REQUIRE(before.videos.size() == 48);
		for (std::size_t index = 0; index < before.videos.size(); ++index) {
			const auto original = before.videos[index];
			const auto actual = after.videos[index];
			TEST_REQUIRE(original.DOVI().has_value() == (fixture.profile != 0));
			TEST_REQUIRE(original.HDR10().has_value() == fixture.hdr10);
			const int expectedDoviSideData = fixture.profile != 0 ? 1 : 0;
			TEST_REQUIRE(before.dovi_metadata_counts[index] == expectedDoviSideData);
			TEST_REQUIRE(before.dovi_rpu_counts[index] == expectedDoviSideData);
			TEST_REQUIRE(after.dovi_metadata_counts[index] == expectedDoviSideData);
			TEST_REQUIRE(after.dovi_rpu_counts[index] == expectedDoviSideData);
			if (original.DOVI()) {
				const auto dovi = original.DOVI().value();
				TEST_REQUIRE(check_dovi(dovi, fixture) == 0);
				TEST_REQUIRE(dovi.MetadataPresent());
				TEST_REQUIRE(!dovi.Rpu().empty());
				TEST_REQUIRE(dovi.BlBitDepth() == 10);
				TEST_REQUIRE(dovi.VdrBitDepth() > 0);
				TEST_REQUIRE(dovi.SignalBitDepth() > 0);
				TEST_REQUIRE(dovi.SourceMinPq() <= dovi.SourceMaxPq());
				TEST_REQUIRE(dovi.SourceMaxPq() <= 4095);
				TEST_REQUIRE(before.side_data_counts[index] >= 2);
				TEST_REQUIRE(after.side_data_counts[index] >= 2);
			}
			TEST_REQUIRE(check_same_video(original, actual, comparison) == 0);
			if (actual.DOVI())
				TEST_REQUIRE(check_dovi(actual.DOVI().value(), fixture) == 0);
		}
		return 0;
	}

	enum class Transform { Remux, Encode, Watermark, Replace };

	int check_pipeline(const Fixture& fixture, Transform transform, std::string_view operation) {
		TEST_PHASE("preparing DOVI pipeline outputs");
		const auto comparison = transform == Transform::Remux
			? DoviComparison::Exact : DoviComparison::Regenerated;
		const auto output = OutputPath("pipeline/dovi/" + std::string{fixture.name}
			+ "-" + std::string{operation} + ".mkv");
		auto before = StormByte::Safe::MakeShared<FrameAudit>();
		auto after = StormByte::Safe::MakeShared<FrameAudit>();
		auto encoded_frames = StormByte::Safe::MakeShared<EncodedAudit>();
		{
			auto logger = MakeLogger();
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(FixturePath(fixture.path)), TestLocation(output), logger, 2000000000LL};
			auto track = job.Video(0);
			if (transform == Transform::Remux)
				track.Remux();
			else {
				auto codec = Registry::Instance().FindCodec("H.265");
				TEST_REQUIRE(codec);
				if (!codec.value().get().HasAccess(Access{Operation::Write}))
					return TEST_SKIP;
				track.Codec(codec.value().get())
					.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libx265"})
					.Preset(StormByte::Safe::String{"ultrafast"});
				track.Filter<AuditFrames>(logger, before);
				if (transform == Transform::Watermark)
					track.Filter<Filter::Video::Watermark>(logger,
						StormByte::Safe::String{FixturePath("logo/watermark-noise-logo.png").string()},
						Filter::Video::Anchor::TopRight, 30u, 16);
				else if (transform == Transform::Replace)
					track.Filter<ReplacePixels>(logger);
				track.Filter<AuditFrames>(logger, after);
			}
			job.Filter<AuditEncodedFrames>(logger, encoded_frames);
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("starting transcoder run");
			job.Run();
			const int result = transform == Transform::Remux ? WaitForTranscoder(job) : WaitForOptionalCodec(job);
			if (result != 0)
				return result;
		}
		if (transform != Transform::Remux) {
			TEST_PHASE("checking frames around the transform");
			TEST_REQUIRE(check_decoded_frames(*before, *after, fixture) == 0);
		}
		TEST_PHASE("checking encoder output audit");
		TEST_REQUIRE(check_decoded_frames(encoded_frames->source, encoded_frames->destination, fixture, comparison) == 0);
		TEST_PHASE("opening encoded output for stream inspection");
		TEST_PHASE("opening media file for inspection");
		auto source = File::Open(StormByte::Safe::String{FixturePath(fixture.path).string()});
		TEST_PHASE("opening media file for inspection");
		auto encoded = File::Open(StormByte::Safe::String{output.string()});
		TEST_REQUIRE(source && encoded);
		TEST_REQUIRE(encoded.value().Container().Name() == "Matroska");
		TEST_REQUIRE(encoded.value().Streams().size() == 1);
		const auto stream = encoded.value().Streams()[0];
		TEST_REQUIRE(stream.Index() == 0);
		TEST_REQUIRE(stream.Type() == StormByte::Multimedia::Type::Video);
		TEST_REQUIRE(stream.Codec().Name() == "H.265");
		const auto original = source.value().Streams()[0].Video();
		const auto actual = stream.Video();
		TEST_REQUIRE(original && actual);
		TEST_REQUIRE(check_same_video(original.value(), actual.value(), comparison) == 0);
		if (comparison == DoviComparison::Regenerated && fixture.profile == 5) {
			TEST_REQUIRE(actual.value().Color().Space() == Property::Space::IPTC2);
			TEST_REQUIRE(actual.value().Color().Primaries() == Property::Primaries::BT2020);
			TEST_REQUIRE(actual.value().Color().Transfer() == Property::Transfer::SMPTE2084);
			TEST_REQUIRE(!actual.value().HDR10());
		}
		if (actual.value().DOVI())
			TEST_REQUIRE(check_dovi(actual.value().DOVI().value(), fixture) == 0);
		const auto duration = encoded.value().Duration();
		TEST_REQUIRE(duration && duration.value().Nanoseconds().count() > 0);
		auto reopened_frames = StormByte::Safe::MakeShared<EncodedAudit>();
		{
			auto logger = MakeLogger();
			const auto verification = OutputPath("pipeline/dovi/" + std::string{fixture.name}
				+ "-" + std::string{operation} + "-verified.mkv");
			TEST_PHASE("creating verification remux from encoded output");
					TEST_PHASE("creating transcoder");
					Transcoder job{TestLocation(output), TestLocation(verification), logger, 2000000000LL};
			job.Video(0).Remux();
			job.Filter<AuditEncodedFrames>(logger, reopened_frames);
			TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
			TEST_PHASE("running verification remux");
			TEST_PHASE("starting transcoder run");
			job.Run();
			TEST_REQUIRE(WaitForTranscoder(job) == 0);
		}
		TEST_PHASE("checking reopened encoded frames");
		TEST_REQUIRE(check_decoded_frames(encoded_frames->source, reopened_frames->source, fixture, comparison) == 0);
		TEST_REQUIRE(check_decoded_frames(reopened_frames->source, reopened_frames->destination, fixture) == 0);
		return 0;
	}

	int check_unsupported_encoder(const Fixture& fixture, std::string_view codec_name,
		std::string_view implementation) {
		auto codec = Registry::Instance().FindCodec(codec_name);
		TEST_REQUIRE(codec);
		if (!codec.value().get().HasAccess(Access{Operation::Write}))
			return TEST_SKIP;
		const auto output = OutputPath("pipeline/dovi/" + std::string{fixture.name}
			+ "-unsupported-" + std::string{implementation} + ".mkv");
		auto logger = MakeLogger();
			TEST_PHASE("creating transcoder");
			Transcoder job{TestLocation(FixturePath(fixture.path)), TestLocation(output), logger, 2000000000LL};
		job.Video(0).Codec(codec.value().get())
			.Implementation(ImplementationSide::Encoder, StormByte::Safe::String{implementation});
		TEST_REQUIRE(CheckTranscoderConfigured(job) == 0);
		TEST_PHASE("starting transcoder run");
		job.Run();
		TEST_REQUIRE(WaitForTranscoderFailure(job) == 0);
		const auto error = job.Error();
		TEST_REQUIRE(error);
		const auto message = TestView(error.value());
		if (message.find("encoder implementation is unavailable") != std::string_view::npos)
			return TEST_SKIP;
		TEST_REQUIRE(message.find("Dolby Vision") != std::string_view::npos
			|| message.find("DOVI") != std::string_view::npos);
		return 0;
	}
}

int test_file_dovi_only_configuration_and_metadata() {
	return check_fixture(dovi_only);
}

int test_file_hdr10_only_without_dovi() {
	return check_fixture(hdr10_only);
}

int test_file_dovi_hdr10_configuration_and_metadata() {
	return check_fixture(dovi_hdr10);
}

int test_remux_dovi_only() { return check_pipeline(dovi_only, Transform::Remux, "remux"); }
int test_remux_hdr10_only() { return check_pipeline(hdr10_only, Transform::Remux, "remux"); }
int test_remux_dovi_hdr10() { return check_pipeline(dovi_hdr10, Transform::Remux, "remux"); }
int test_reencode_dovi_only() { return check_pipeline(dovi_only, Transform::Encode, "reencode"); }
int test_reencode_hdr10_only() { return check_pipeline(hdr10_only, Transform::Encode, "reencode"); }
int test_reencode_dovi_hdr10() { return check_pipeline(dovi_hdr10, Transform::Encode, "reencode"); }
int test_watermark_dovi_only() { return check_pipeline(dovi_only, Transform::Watermark, "watermark"); }
int test_watermark_hdr10_only() { return check_pipeline(hdr10_only, Transform::Watermark, "watermark"); }
int test_watermark_dovi_hdr10() { return check_pipeline(dovi_hdr10, Transform::Watermark, "watermark"); }
int test_replace_frame_dovi_only() { return check_pipeline(dovi_only, Transform::Replace, "replace"); }
int test_replace_frame_hdr10_only() { return check_pipeline(hdr10_only, Transform::Replace, "replace"); }
int test_replace_frame_dovi_hdr10() { return check_pipeline(dovi_hdr10, Transform::Replace, "replace"); }
int test_unsupported_vp9_dovi_only() { return check_unsupported_encoder(dovi_only, "VP9", "libvpx-vp9"); }
int test_unsupported_vp9_dovi_hdr10() { return check_unsupported_encoder(dovi_hdr10, "VP9", "libvpx-vp9"); }
int test_unsupported_hevc_dovi_only() { return check_unsupported_encoder(dovi_only, "H.265", "libkvazaar"); }
int test_unsupported_hevc_dovi_hdr10() { return check_unsupported_encoder(dovi_hdr10, "H.265", "libkvazaar"); }

int test_video_and_frame_copy_move_preserve_dovi() {
	for (const auto& fixture : {dovi_only, hdr10_only, dovi_hdr10}) {
		TEST_PHASE("opening media file for inspection");
		auto opened = File::Open(StormByte::Safe::String{FixturePath(fixture.path).string()});
		TEST_REQUIRE(opened);
		const auto video = opened.value().Streams()[0].Video();
		TEST_REQUIRE(video);
		Property::Video copied{video.value()};
		Property::Video moved{std::move(copied)};
		TEST_REQUIRE(check_same_video(video.value(), moved) == 0);
		copied = video.value();
		moved = std::move(copied);
		TEST_REQUIRE(check_same_video(video.value(), moved) == 0);
		Frame source{0, StormByte::Multimedia::Type::Video, Producer::Decoder, {},
			std::nullopt, std::nullopt, moved, {}, std::nullopt, 7, 0};
		Frame copy{source};
		Frame destination{std::move(copy)};
		TEST_REQUIRE(copy.Type() == StormByte::Multimedia::Type::Unknown);
		TEST_REQUIRE(copy.Track() == -1);
		TEST_REQUIRE(destination.Video());
		TEST_REQUIRE(check_same_video(video.value(), destination.Video().value()) == 0);
		copy = source;
		destination = std::move(copy);
		TEST_REQUIRE(destination.Serial() && destination.Serial().value() == 7);
		TEST_REQUIRE(check_same_video(video.value(), destination.Video().value()) == 0);
	}
	return 0;
}

int test_dovi_owned_rpu_and_invalid_loads_preserve_configuration() {
	TEST_PHASE("opening media file for inspection");
	auto opened = File::Open(StormByte::Safe::String{FixturePath(dovi_only.path).string()});
	TEST_REQUIRE(opened);
	const auto video = opened.value().Streams()[0].Video();
	TEST_REQUIRE(video && video.value().DOVI());
	auto dovi = video.value().DOVI().value();
	std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}};
	const auto expected = bytes;
	TEST_REQUIRE(dovi.LoadRpu(bytes));
	bytes.fill(std::byte{0});
	TEST_REQUIRE(std::ranges::equal(dovi.Rpu(), expected));
	Property::DOVI copy{dovi};
	Property::DOVI moved{std::move(copy)};
	TEST_REQUIRE(check_same_dovi(dovi, moved) == 0);
	copy = dovi;
	moved = std::move(copy);
	TEST_REQUIRE(check_same_dovi(dovi, moved) == 0);
	const auto saved = dovi;
	TEST_REQUIRE(!dovi.LoadConfiguration({}));
	TEST_REQUIRE(!dovi.LoadConfiguration(std::span{bytes}.first(1)));
	TEST_REQUIRE(!dovi.LoadMetadata({}));
	TEST_REQUIRE(!dovi.LoadMetadata(std::span{bytes}.first(1)));
	TEST_REQUIRE(!dovi.LoadRpu({}));
	TEST_REQUIRE(check_same_dovi(saved, dovi) == 0);
	TEST_REQUIRE(check_dovi(dovi, dovi_only) == 0);
	dovi = Property::DOVI{};
	TEST_REQUIRE(!dovi.Present());
	TEST_REQUIRE(std::ranges::equal(moved.Rpu(), expected));
	return 0;
}

int test_dovi_metadata_rejects_invalid_extension_offsets() {
	for (const auto offset : {std::size_t{0}, sizeof(AVDOVIMetadata) - 1,
		sizeof(NativeMetadata), sizeof(NativeMetadata) + 1,
		std::numeric_limits<std::size_t>::max()}) {
		auto native = make_metadata();
		native.metadata.ext_block_offset = offset;
		TEST_REQUIRE(check_invalid_metadata(native) == 0);
	}
	return 0;
}

int test_dovi_metadata_rejects_invalid_extension_strides() {
	for (const auto stride : {std::size_t{0}, sizeof(AVDOVIDmData) - 1,
		std::numeric_limits<std::size_t>::max()}) {
		auto native = make_metadata();
		native.metadata.ext_block_size = stride;
		TEST_REQUIRE(check_invalid_metadata(native) == 0);
	}
	return 0;
}

int test_dovi_metadata_validates_extension_counts_and_extent() {
	for (const auto count : {-1, AV_DOVI_MAX_EXT_BLOCKS + 1, std::numeric_limits<int>::max()}) {
		auto native = make_metadata();
		native.metadata.num_ext_blocks = count;
		TEST_REQUIRE(check_invalid_metadata(native) == 0);
	}
	for (const auto count : {0, AV_DOVI_MAX_EXT_BLOCKS}) {
		auto native = make_metadata();
		native.metadata.num_ext_blocks = count;
		TEST_REQUIRE(check_valid_metadata(std::as_bytes(std::span{&native, 1})) == 0);
	}
	auto native = make_metadata();
	native.metadata.num_ext_blocks = 2;
	TEST_REQUIRE(check_invalid_metadata(native,
		native.metadata.ext_block_offset + native.metadata.ext_block_size) == 0);
	return 0;
}

int test_dovi_metadata_accepts_unaligned_storage_and_offsets() {
	const auto native = make_metadata();
	alignas(NativeMetadata) std::array<std::byte, sizeof(NativeMetadata) + 1> bytes{};
	std::memcpy(bytes.data() + 1, &native, sizeof(native));
	TEST_REQUIRE(check_valid_metadata(std::span{bytes}.subspan(1)) == 0);
	bytes.fill(std::byte{0});
	auto metadata = native.metadata;
	++metadata.header_offset;
	++metadata.mapping_offset;
	++metadata.color_offset;
	++metadata.ext_block_offset;
	std::memcpy(bytes.data(), &metadata, sizeof(metadata));
	std::memcpy(bytes.data() + metadata.header_offset, &native.header, sizeof(native.header));
	std::memcpy(bytes.data() + metadata.mapping_offset, &native.mapping, sizeof(native.mapping));
	std::memcpy(bytes.data() + metadata.color_offset, &native.color, sizeof(native.color));
	std::memcpy(bytes.data() + metadata.ext_block_offset, native.extensions.data(), sizeof(native.extensions));
	TEST_REQUIRE(check_valid_metadata(bytes) == 0);
	return 0;
}

int test_dovi_feature_string_conversion() {
	TEST_REQUIRE(std::string_view{ToString(Feature::DOVI)} == "DOVI");
	const Features dovi{Feature::DOVI};
	TEST_REQUIRE(dovi.Has(Feature::DOVI));
	TEST_REQUIRE(!dovi.Has(Feature::HDR10));
	TEST_REQUIRE(TestView(static_cast<StormByte::Safe::String>(dovi)) == "DOVI");
	TEST_REQUIRE(TestView(static_cast<StormByte::Safe::String>(dovi)) == "DOVI");
	const Features combined = Feature::HDR10 | Feature::DOVI | Feature::SideData;
	TEST_REQUIRE(TestView(static_cast<StormByte::Safe::String>(combined)) == "HDR10 | DOVI | SideData");
	return 0;
}

int test_dovi_decoder_table_capabilities() {
	const auto table = Tables::Decoder::Video();
	constexpr std::array enabled{std::string_view{"hevc"}, std::string_view{"av1"}, std::string_view{"libdav1d"}};
	for (const auto name : enabled) {
		const auto found = std::ranges::find_if(table, [name](const auto& row) {
			return std::string_view{row.name} == name;
		});
		TEST_REQUIRE(found != table.end());
		TEST_REQUIRE(found->features.Has(Feature::DOVI));
	}
	for (const auto& row : table) {
		const bool expected = std::ranges::find(enabled, std::string_view{row.name}) != enabled.end();
		TEST_REQUIRE(row.features.Has(Feature::DOVI) == expected);
		if (expected)
			TEST_REQUIRE(!row.features.Has(Feature::HardwareAcceleration));
	}
	return 0;
}

int test_dovi_encoder_table_capabilities() {
	const auto table = Tables::Encoder::Video();
	constexpr std::array enabled{std::string_view{"libx265"}, std::string_view{"libsvtav1"}, std::string_view{"libaom-av1"}};
	for (const auto name : enabled) {
		const auto found = std::ranges::find_if(table, [name](const auto& row) {
			return std::string_view{row.name} == name;
		});
		TEST_REQUIRE(found != table.end());
		TEST_REQUIRE(found->features.Has(Feature::DOVI));
	}
	for (const auto& row : table) {
		const bool expected = std::ranges::find(enabled, std::string_view{row.name}) != enabled.end();
		TEST_REQUIRE(row.features.Has(Feature::DOVI) == expected);
		if (expected)
			TEST_REQUIRE(!row.features.Has(Feature::HardwareAcceleration));
	}
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_file_dovi_only_configuration_and_metadata", test_file_dovi_only_configuration_and_metadata},
		TestEntry{"test_file_hdr10_only_without_dovi", test_file_hdr10_only_without_dovi},
		TestEntry{"test_file_dovi_hdr10_configuration_and_metadata", test_file_dovi_hdr10_configuration_and_metadata},
		TestEntry{"test_remux_dovi_only", test_remux_dovi_only},
		TestEntry{"test_remux_hdr10_only", test_remux_hdr10_only},
		TestEntry{"test_remux_dovi_hdr10", test_remux_dovi_hdr10},
		TestEntry{"test_reencode_dovi_only", test_reencode_dovi_only},
		TestEntry{"test_reencode_hdr10_only", test_reencode_hdr10_only},
		TestEntry{"test_reencode_dovi_hdr10", test_reencode_dovi_hdr10},
		TestEntry{"test_watermark_dovi_only", test_watermark_dovi_only},
		TestEntry{"test_watermark_hdr10_only", test_watermark_hdr10_only},
		TestEntry{"test_watermark_dovi_hdr10", test_watermark_dovi_hdr10},
		TestEntry{"test_replace_frame_dovi_only", test_replace_frame_dovi_only},
		TestEntry{"test_replace_frame_hdr10_only", test_replace_frame_hdr10_only},
		TestEntry{"test_replace_frame_dovi_hdr10", test_replace_frame_dovi_hdr10},
		TestEntry{"test_unsupported_vp9_dovi_only", test_unsupported_vp9_dovi_only},
		TestEntry{"test_unsupported_vp9_dovi_hdr10", test_unsupported_vp9_dovi_hdr10},
		TestEntry{"test_unsupported_hevc_dovi_only", test_unsupported_hevc_dovi_only},
		TestEntry{"test_unsupported_hevc_dovi_hdr10", test_unsupported_hevc_dovi_hdr10},
		TestEntry{"test_video_and_frame_copy_move_preserve_dovi", test_video_and_frame_copy_move_preserve_dovi},
		TestEntry{"test_dovi_owned_rpu_and_invalid_loads_preserve_configuration", test_dovi_owned_rpu_and_invalid_loads_preserve_configuration},
		TestEntry{"test_dovi_metadata_rejects_invalid_extension_offsets", test_dovi_metadata_rejects_invalid_extension_offsets},
		TestEntry{"test_dovi_metadata_rejects_invalid_extension_strides", test_dovi_metadata_rejects_invalid_extension_strides},
		TestEntry{"test_dovi_metadata_validates_extension_counts_and_extent", test_dovi_metadata_validates_extension_counts_and_extent},
		TestEntry{"test_dovi_metadata_accepts_unaligned_storage_and_offsets", test_dovi_metadata_accepts_unaligned_storage_and_offsets},
		TestEntry{"test_dovi_feature_string_conversion", test_dovi_feature_string_conversion},
		TestEntry{"test_dovi_decoder_table_capabilities", test_dovi_decoder_table_capabilities},
		TestEntry{"test_dovi_encoder_table_capabilities", test_dovi_encoder_table_capabilities},
	};
	return RunSelectedTest(argc, argv, tests);
}
