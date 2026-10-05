#include <StormByte/multimedia/pipeline/config/attachment.hxx>

#include <utility>

using namespace StormByte::Multimedia::Pipeline::Config;

Attachment::Attachment(std::string_view mime_type)
: Base(StormByte::Multimedia::Type::Attachment), m_mimeType(mime_type) {}

Attachment::Attachment(StormByte::Safe::String mime_type)
: Base(StormByte::Multimedia::Type::Attachment), m_mimeType(std::move(mime_type)) {}

Attachment::Attachment(const Attachment& other) = default;

Attachment::Attachment(Attachment&& other) noexcept = default;

Attachment::~Attachment() noexcept = default;

Attachment& Attachment::operator=(const Attachment& other) = default;

Attachment& Attachment::operator=(Attachment&& other) noexcept = default;

Attachment::PointerType Attachment::Clone() const {
	return MakePointer<Attachment>(*this);
}

Attachment::PointerType Attachment::Move() {
	return MakePointer<Attachment>(std::move(*this));
}

void Attachment::MimeType(StormByte::Safe::String mime_type) noexcept {
	m_mimeType = std::move(mime_type);
}

void Attachment::MimeType(std::string_view mime_type) noexcept {
	m_mimeType = StormByte::Safe::String{mime_type};
}