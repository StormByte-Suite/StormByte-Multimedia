#include "../helpers.hxx"

#include <StormByte/multimedia/pipeline/filters/video/nlmeans_plane.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
	void ReferencePlane(const std::uint8_t* source, std::uint8_t* destination,
		int stride, int width, int height, int depth, int research, int patch, double strength) {
		const int peak = (1 << depth) - 1;
		const double inverse = 1.0 / std::max(1e-8, strength * strength);
		const auto sample = [&](int column, int row) {
			const auto* pixels = source + row * stride;
			return depth == 8 ? static_cast<int>(pixels[column])
				: static_cast<int>(reinterpret_cast<const std::uint16_t*>(pixels)[column]);
		};
		for (int row = 0; row < height; ++row) {
			for (int column = 0; column < width; ++column) {
				double weighted = 0.0;
				double weights = 0.0;
				for (int candidateRow = std::max(0, row - research); candidateRow <= std::min(height - 1, row + research); ++candidateRow) {
					for (int candidateColumn = std::max(0, column - research); candidateColumn <= std::min(width - 1, column + research); ++candidateColumn) {
						double distance = 0.0;
						unsigned count = 0;
						for (int patchRow = -patch; patchRow <= patch; ++patchRow) {
							for (int patchColumn = -patch; patchColumn <= patch; ++patchColumn) {
								const int difference = sample(std::clamp(column + patchColumn, 0, width - 1), std::clamp(row + patchRow, 0, height - 1))
									- sample(std::clamp(candidateColumn + patchColumn, 0, width - 1), std::clamp(candidateRow + patchRow, 0, height - 1));
								distance += static_cast<double>(difference) * difference;
								++count;
							}
						}
						distance /= static_cast<double>(peak) * peak * count;
						const double weight = std::exp(-distance * inverse);
						weighted += static_cast<double>(sample(candidateColumn, candidateRow)) * weight;
						weights += weight;
					}
				}
				const int value = std::clamp(static_cast<int>(weighted / std::max(weights, 1e-8) + 0.5), 0, peak);
				auto* pixels = destination + row * stride;
				if (depth == 8)
					pixels[column] = static_cast<std::uint8_t>(value);
				else
					reinterpret_cast<std::uint16_t*>(pixels)[column] = static_cast<std::uint16_t>(value);
			}
		}
	}
}

int test_nlmeans_rolling_sums_match_reference() {
	static constexpr std::array dimensions{
		std::array{1, 1}, std::array{1, 7}, std::array{7, 1}, std::array{9, 6}
	};
	static constexpr std::array radii{
		std::array{0, 0}, std::array{1, 0}, std::array{3, 2}, std::array{7, 4}
	};
	for (const auto& shape : dimensions) {
		const int width = shape[0];
		const int height = shape[1];
		for (const int depth : {8, 10, 12}) {
			const int strideBytes = ((width * (depth == 8 ? 1 : 2) + 7) / 2) * 2;
			const auto wordCount = static_cast<std::size_t>(strideBytes / 2 * height);
			std::vector<std::uint16_t> source(wordCount, 0xA5A5);
			auto* sourceBytes = reinterpret_cast<std::uint8_t*>(source.data());
			for (int row = 0; row < height; ++row) {
				for (int column = 0; column < width; ++column) {
					const auto value = static_cast<std::uint16_t>((row * 149 + column * 71 + row * column * 19) & ((1 << depth) - 1));
					if (depth == 8)
						sourceBytes[row * strideBytes + column] = static_cast<std::uint8_t>(value);
					else
						reinterpret_cast<std::uint16_t*>(sourceBytes + row * strideBytes)[column] = value;
				}
			}
			for (const auto& radius : radii) {
				for (const double strength : {0.0, 0.05, 0.8, 1.0}) {
					for (const bool reversed : {false, true}) {
						std::vector<std::uint16_t> expected(wordCount, 0xA5A5);
						std::vector<std::uint16_t> actual(wordCount, 0xA5A5);
						const int offset = reversed ? (height - 1) * strideBytes : 0;
						const int stride = reversed ? -strideBytes : strideBytes;
						auto* actualBytes = reinterpret_cast<std::uint8_t*>(actual.data()) + offset;
						auto* expectedBytes = reinterpret_cast<std::uint8_t*>(expected.data()) + offset;
						ReferencePlane(sourceBytes + offset, expectedBytes, stride, width, height, depth, radius[0], radius[1], strength);
						StormByte::Multimedia::Pipeline::Filter::Video::Detail::FilterPlane(
							sourceBytes + offset, actualBytes, stride, stride, width, height, depth, radius[0], radius[1], strength);
						TEST_REQUIRE(actual == expected);
					}
				}
			}
		}
	}
	return 0;
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		TestEntry{"test_nlmeans_rolling_sums_match_reference", test_nlmeans_rolling_sums_match_reference},
	};
	return RunSelectedTest(argc, argv, tests);
}