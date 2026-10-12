#pragma once

#include <StormByte/safe/vector.hxx>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

/**
 * @namespace StormByte::Multimedia::Pipeline::Filter::Video::Detail
 * @brief Private video filter plane algorithms.
 */
namespace StormByte::Multimedia::Pipeline::Filter::Video::Detail {
	/**
	 * @brief Apply NLMeans using rolling integer patch sums for each displacement.
	 * @param source Source plane with native-endian samples.
	 * @param destination Non-overlapping destination plane.
	 * @param sourceStride Source row stride in bytes, including negative strides.
	 * @param destinationStride Destination row stride in bytes.
	 * @param width Positive plane width in samples.
	 * @param height Positive plane height in samples.
	 * @param bitsPerComponent Sample depth: 8, 10 or 12 bits.
	 * @param research Nonnegative candidate search radius, restricted to the image.
	 * @param patch Nonnegative patch radius with squared patch sums fitting uint64_t.
	 * @param strength Normalized distance bandwidth, squared and floored at 1e-8.
	 * @throws StormByte::Safe::AllocationError Scratch allocation failed.
	 *
	 * Both patches clamp each sample independently at image borders. For a fixed
	 * displacement, vertical column sums and a horizontal rolling sum give the
	 * same squared distance as visiting the entire patch. Scratch is reused for
	 * all displacements; per-pixel weights retain row-major candidate order.
	 */
	inline void FilterPlane(const std::uint8_t* source, std::uint8_t* destination,
		int sourceStride, int destinationStride, int width, int height,
		int bitsPerComponent, int research, int patch, double strength) {
		const int peak = (1 << bitsPerComponent) - 1;
		const double inverseStrength = 1.0 / std::max(1e-8, strength * strength);
		const std::int64_t patchRadius = patch;
		const std::int64_t patchSide = 2 * patchRadius + 1;
		const double normalization = static_cast<double>(peak) * static_cast<double>(peak)
			* static_cast<double>(patchSide * patchSide);
		const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
		const std::size_t columnCount = static_cast<std::size_t>(width) + 2 * patchRadius;
		StormByte::Safe::Vector<double> weightedPixels(pixelCount, 0.0);
		StormByte::Safe::Vector<double> weights(pixelCount, 0.0);
		StormByte::Safe::Vector<std::uint64_t> columns(columnCount, 0);
		const auto sample = [&](int column, int row) noexcept {
			const std::uint8_t* samples = source + static_cast<std::ptrdiff_t>(row) * sourceStride;
			return bitsPerComponent <= 8 ? static_cast<int>(samples[column])
				: static_cast<int>(reinterpret_cast<const std::uint16_t*>(samples)[column]);
		};
		const int verticalRadius = std::min(research, height - 1);
		const int horizontalRadius = std::min(research, width - 1);
		for (int offsetY = -verticalRadius; offsetY <= verticalRadius; ++offsetY) {
			for (int offsetX = -horizontalRadius; offsetX <= horizontalRadius; ++offsetX) {
				const auto squaredDifference = [&](std::int64_t column, std::int64_t row) noexcept {
					const int sourceX = static_cast<int>(std::clamp<std::int64_t>(column, 0, width - 1));
					const int sourceY = static_cast<int>(std::clamp<std::int64_t>(row, 0, height - 1));
					const int candidateX = static_cast<int>(std::clamp<std::int64_t>(column + offsetX, 0, width - 1));
					const int candidateY = static_cast<int>(std::clamp<std::int64_t>(row + offsetY, 0, height - 1));
					const int difference = sample(sourceX, sourceY) - sample(candidateX, candidateY);
					return static_cast<std::uint64_t>(difference * difference);
				};
				for (std::size_t column = 0; column < columnCount; ++column) {
					std::uint64_t columnSum = 0;
					for (std::int64_t patchY = -patchRadius; patchY <= patchRadius; ++patchY)
						columnSum += squaredDifference(static_cast<std::int64_t>(column) - patchRadius, patchY);
					columns[column] = columnSum;
				}
				const int firstX = std::max(0, -offsetX);
				const int lastX = std::min(width, width - offsetX);
				const int firstY = std::max(0, -offsetY);
				const int lastY = std::min(height, height - offsetY);
				for (int row = 0; row < lastY; ++row) {
					if (row >= firstY) {
						std::uint64_t patchSum = 0;
						for (std::int64_t patchX = 0; patchX < patchSide; ++patchX)
							patchSum += columns[static_cast<std::size_t>(firstX + patchX)];
						for (int column = firstX; column < lastX; ++column) {
							const double distance = static_cast<double>(patchSum) / normalization;
							const double weight = std::exp(-distance * inverseStrength);
							const std::size_t index = static_cast<std::size_t>(row) * width + column;
							weightedPixels[index] += static_cast<double>(sample(column + offsetX, row + offsetY)) * weight;
							weights[index] += weight;
							if (column + 1 < lastX) {
								patchSum -= columns[column];
								patchSum += columns[static_cast<std::size_t>(column + patchSide)];
							}
						}
					}
					if (row + 1 < lastY) {
						for (std::size_t column = 0; column < columnCount; ++column) {
							const std::int64_t sampleX = static_cast<std::int64_t>(column) - patchRadius;
							columns[column] -= squaredDifference(sampleX, row - patchRadius);
							columns[column] += squaredDifference(sampleX, row + patchRadius + 1);
						}
					}
				}
			}
		}
		for (int row = 0; row < height; ++row) {
			std::uint8_t* samples = destination + static_cast<std::ptrdiff_t>(row) * destinationStride;
			for (int column = 0; column < width; ++column) {
				const std::size_t index = static_cast<std::size_t>(row) * width + column;
				const int value = std::clamp(static_cast<int>(weightedPixels[index] / std::max(weights[index], 1e-8) + 0.5), 0, peak);
				if (bitsPerComponent <= 8)
					samples[column] = static_cast<std::uint8_t>(value);
				else
					reinterpret_cast<std::uint16_t*>(samples)[column] = static_cast<std::uint16_t>(value);
			}
		}
	}
}