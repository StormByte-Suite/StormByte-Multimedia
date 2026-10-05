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

#include <StormByte/multimedia/registry.hxx>
#include <tables/codec/catalog.hxx>
#include <tables/container/catalog.hxx>
#include <tables/container/table.hxx>

#include <string>
#include <utility>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

using namespace StormByte::Multimedia;

namespace {
	Access ProbeAccess(const StormByte::Multimedia::Tables::Codec::CodecDef& def) noexcept {
		Access access(Operation::Read);
		for (std::size_t i = 0; i < def.FfmpegIdCount(); ++i) {
			const char* id = def.FfmpegId(i);
			if (!id)
				continue;
			if (avcodec_find_encoder_by_name(id) != nullptr) {
				access |= Access(Operation::Write);
				break;
			}

			const AVCodecDescriptor* desc = avcodec_descriptor_get_by_name(id);
			if (desc && avcodec_find_encoder(desc->id) != nullptr) {
				access |= Access(Operation::Write);
				break;
			}
		}

		return access;
	}
}

Registry::Registry() noexcept {
	Initialize();
}

Registry::~Registry() noexcept = default;

Registry& Registry::Instance() noexcept {
	static Registry instance;
	return instance;
}

CodecRefs Registry::CodecList(Type type) const noexcept {
	CodecRefs out;
	const auto it = m_by_type.find(type);
	if (it == m_by_type.end())
		return out;

	out.reserve(it->second.size());
	for (const std::size_t i : it->second)
		out.emplace_back(m_codecs[i]);
	return out;
}

ContainerRefs Registry::ContainerList() const noexcept {
	ContainerRefs out;
	out.reserve(m_containers.size());
	for (const Container& container : m_containers)
		out.emplace_back(container);
	return out;
}

ExpectedCodec Registry::FindCodec(std::string_view name) const noexcept {
	const auto it = m_by_name.find(name);
	if (it == m_by_name.end())
		return Unexpected<CodecNotFoundException>(std::string(name));

	return CodecRef(m_codecs[it->second]);
}

ExpectedContainer Registry::FindContainer(std::string_view name) const noexcept {
	const auto it = m_container_by_name.find(name);
	if (it == m_container_by_name.end())
		return Unexpected<ContainerNotFoundException>(std::string(name));

	return ContainerRef(m_containers[it->second]);
}

void Registry::Add(Type type, const Tables::Codec::CodecDef& def) noexcept {
	const std::size_t i = m_codecs.size();
	m_codecs.push_back(Codec(type, def.name, def.description, ProbeAccess(def)));
	const Codec& stored = m_codecs[i];

	m_by_name.emplace(stored.Name(), i);
	for (std::size_t n = 0; n < def.FfmpegIdCount(); ++n)
		m_by_name.emplace(def.FfmpegId(n), i);
	m_by_type[type].push_back(i);
}

void Registry::LoadCodecs(Type type, std::span<const Tables::Codec::CodecDef> table) noexcept {
	for (const Tables::Codec::CodecDef& def : table)
		Add(type, def);
}

void Registry::LoadCodecs() noexcept {
	const auto& codecs 		= Tables::Codec::Catalog::Instance();
	const auto video		= codecs.All(Type::Video);
	const auto audio		= codecs.All(Type::Audio);
	const auto subtitle		= codecs.All(Type::Subtitle);
	const auto attachment	= codecs.All(Type::Attachment);

	m_codecs.reserve(video.size() + audio.size() + subtitle.size() + attachment.size());
	m_by_name.reserve((video.size() + audio.size() + subtitle.size() + attachment.size()) * 2);
	m_by_type.reserve(4);

	LoadCodecs(Type::Video, video);
	LoadCodecs(Type::Audio, audio);
	LoadCodecs(Type::Subtitle, subtitle);
	LoadCodecs(Type::Attachment, attachment);
}

Access Registry::ProbeContainer(const Tables::Container::ContainerDef& def) const noexcept {
	Access access;
	for (std::size_t i = 0; i < def.FfmpegIdCount(); ++i) {
		const char* id = def.FfmpegId(i);
		if (!id)
			break;
		if (av_find_input_format(id) != nullptr)
			access |= Access(Operation::Read);
		if (av_guess_format(id, nullptr, nullptr) != nullptr)
			access |= Access(Operation::Write);
	}

	if (def.attachments)
		access |= Access(Operation::Attach);
	return access;
}

void Registry::Add(const Tables::Container::ContainerDef& def) noexcept {
	const char* ext = def.PrimaryExtension();
	const std::size_t i = m_containers.size();
	m_containers.push_back(Container(
		def.name,
		def.description,
		ext ? std::string_view{ext} : std::string_view{},
		ProbeContainer(def)
	));
	Container& stored = m_containers[i];

	m_container_by_name.emplace(stored.Name(), i);
	if (ext && ext[0] != '\0')
		m_container_by_name.emplace(ext, i);
	for (std::size_t n = 0; n < def.FfmpegIdCount(); ++n)
		m_container_by_name.emplace(def.FfmpegId(n), i);

	const auto rows = Tables::Container::Catalog::Instance().Compat(def);
	stored.m_allowed.reserve(rows.size());
	for (const auto& row : rows) {
		if (!row.codec)
			continue;
		auto found = FindCodec(row.codec);
		if (!found.has_value())
			continue;
		const Codec& codec = found.value().get();
		bool dup = false;
		for (const CodecRef existing : std::as_const(stored.m_allowed)) {
			if (existing.get() == codec) {
				dup = true;
				break;
			}
		}

		if (!dup)
			stored.m_allowed.emplace_back(codec);
	}
}

void Registry::LoadContainers() noexcept {
	const auto& catalog = Tables::Container::Catalog::Instance();
	const auto all = catalog.All();
	m_containers.reserve(all.size());
	m_container_by_name.reserve(all.size() * 3);
	for (const auto& def : all)
		Add(def);
}

void Registry::Load() noexcept {
	LoadCodecs();
	LoadContainers();
}

void Registry::Initialize() noexcept {
	Load();
}
