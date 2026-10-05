#include <StormByte/multimedia/exception.hxx>
#include <StormByte/multimedia/stream.hxx>

#include <utility>

using namespace ::StormByte::Multimedia;

Stream::Stream(): m_index(-1), m_codec(nullptr) {}

Stream::Stream(int index, const class Codec& codec, Metadata::Stream metadata,
	StormByte::Safe::Optional<Property::Duration> duration, Properties properties) noexcept
: m_index(index), m_codec(&codec), m_metadata(std::move(metadata)),
m_duration(std::move(duration)), m_properties(std::move(properties)) {}

Stream::Stream(const Stream& other) = default;

Stream::Stream(Stream&& other) noexcept = default;

Stream::~Stream() noexcept = default;

Stream& Stream::operator=(const Stream& other) {
	if (this != &other) {
		Stream copy(other);
		*this = std::move(copy);
	}
	return *this;
}

Stream& Stream::operator=(Stream&& other) noexcept = default;

const class Codec& Stream::Codec() const {
	if (!m_codec)
		throw Exception("Stream", "empty stream has no codec");
	return *m_codec;
}

StormByte::Multimedia::Type Stream::Type() const noexcept {
	return m_codec ? m_codec->Type() : StormByte::Multimedia::Type::Unknown;
}

const StormByte::Safe::Optional<Property::Duration>& Stream::Duration() const noexcept {
	return m_duration;
}

const StormByte::Safe::Optional<Property::Video>& Stream::Video() const noexcept {
	return m_properties.first;
}

const StormByte::Safe::Optional<Property::Audio>& Stream::Audio() const noexcept {
	return m_properties.second;
}