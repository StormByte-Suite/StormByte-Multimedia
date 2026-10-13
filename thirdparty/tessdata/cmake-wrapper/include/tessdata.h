#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace tessdata {
	std::span<const std::uint8_t> blob(std::string_view lang);
}
