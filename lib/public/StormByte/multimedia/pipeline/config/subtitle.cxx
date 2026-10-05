#include <StormByte/multimedia/pipeline/config/subtitle.hxx>

#include <utility>

using namespace StormByte::Multimedia::Pipeline::Config;

Subtitle::Subtitle(): Base(StormByte::Multimedia::Type::Subtitle), m_codec(nullptr) {}

Subtitle::Subtitle(const Subtitle& other) = default;

Subtitle::Subtitle(Subtitle&& other) noexcept = default;

Subtitle::~Subtitle() noexcept = default;

Subtitle& Subtitle::operator=(const Subtitle& other) = default;

Subtitle& Subtitle::operator=(Subtitle&& other) noexcept = default;

Subtitle::PointerType Subtitle::Clone() const {
	return MakePointer<Subtitle>(*this);
}

Subtitle::PointerType Subtitle::Move() {
	return MakePointer<Subtitle>(std::move(*this));
}

void Subtitle::FineTune(StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String> options) noexcept {
	m_fineTune = std::move(options);
}