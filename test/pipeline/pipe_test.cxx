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

#include "helpers.hxx"

#include <StormByte/multimedia/backend/pipeline/pipe.hxx>
#include <StormByte/multimedia/pipeline/packet.hxx>

#include <array>

using StormByte::ByteSize;
using StormByte::Multimedia::Backend::Pipeline::Pipe;
using namespace StormByte::Multimedia::Pipeline;

namespace {
	// -------------------
	// Wiring
	// -------------------
	int test_accounting_precedes_first_push() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		policy.QueueBytes(ByteSize{12});
		Pipe producer;
		Pipe consumer;
		consumer.Buffering(policy, StormByte::Size{2});
		producer.To(-1) >> consumer;
		TEST_REQUIRE(consumer.Snapshot(-1).Limits.Elements() == StormByte::Size{2});
		Pipe::Item::PointerType item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{7});
		consumer.Capacity(-1, StormByte::Size{2});
		const auto snapshot = consumer.Snapshot(-1);
		TEST_REQUIRE(snapshot.Elements == StormByte::Size{1});
		TEST_REQUIRE(snapshot.Bytes == ByteSize{7});
		TEST_REQUIRE(snapshot.Limits.Elements() == StormByte::Size{2});
		TEST_REQUIRE(snapshot.Limits.Bytes() == ByteSize{12});
		TEST_REQUIRE(!snapshot.Closed);
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		return 0;
	}

	int test_clone_accounting_precedes_first_push() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe primary;
		Pipe consumer;
		consumer.Buffering(policy);
		producer.To(-1) >> primary;
		producer.CloneTo(-1, consumer);
		producer << StormByte::Safe::MakeShared<Packet>();
		consumer.Capacity(-1, StormByte::Size{2});
		TEST_REQUIRE(consumer.Snapshot(-1).Elements == StormByte::Size{1});
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{1});
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		return 0;
	}

	int test_plan_policy_configures_already_wired_inputs() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe consumer;
		producer.To(-1) >> consumer;
		consumer.Buffering(policy, StormByte::Size{1});
		Pipe::Item::PointerType item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{7});
		item = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(producer.Out().TryPush(-1, item, ByteSize{7}) == StormByte::Buffer::QueueStatus::WouldBlock);
		TEST_REQUIRE(consumer.Snapshot(-1).PeakElements == StormByte::Size{1});
		consumer.Capacity(-1, StormByte::Size{1});
		consumer.Abort();
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}

	int test_redirected_clone_configures_destination() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe producer;
		Pipe primary;
		Pipe previous;
		Pipe consumer;
		consumer.Buffering(policy);
		producer.To(-1) >> primary;
		producer.CloneTo(-1, previous);
		TEST_REQUIRE(producer.RedirectCloneTo(-1, previous, consumer));
		producer << StormByte::Safe::MakeShared<Packet>();
		consumer.Capacity(-1, StormByte::Size{2});
		TEST_REQUIRE(consumer.Snapshot(-1).Elements == StormByte::Size{1});
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{1});
		TEST_REQUIRE(previous.InputEof());
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		producer.Close(nullptr, nullptr);
		TEST_REQUIRE(consumer.InputEof());
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}

	int test_shared_writers_keep_budget_and_eof() {
		StormByte::Buffer::ByteBudget budget{ByteSize{16}};
		BufferingPolicy policy;
		policy.Budget(budget);
		Pipe first;
		Pipe second;
		Pipe consumer;
		consumer.Buffering(policy);
		first.To(-1) >> consumer;
		second.To(-1) >> consumer;
		Pipe::Item::PointerType one = StormByte::Safe::MakeShared<Packet>();
		Pipe::Item::PointerType two = StormByte::Safe::MakeShared<Packet>();
		TEST_REQUIRE(first.Out().TryPush(-1, one, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		first.Close(nullptr, nullptr);
		TEST_REQUIRE(second.Out().TryPush(-1, two, ByteSize{7}) == StormByte::Buffer::QueueStatus::Ok);
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{14});
		consumer.Capacity(-1, StormByte::Size{2});
		second.Close(nullptr, nullptr);
		Pipe::Item::PointerType popped;
		consumer >> popped;
		TEST_REQUIRE(popped);
		consumer >> popped;
		TEST_REQUIRE(popped);
		TEST_REQUIRE(consumer.InputEof());
		TEST_REQUIRE(budget.Snapshot().Reserved == ByteSize{0});
		return 0;
	}
}

int main(int argc, char** argv) {
	static constexpr std::array tests{
		// -------------------
		// Wiring
		// -------------------
		TestEntry{"test_accounting_precedes_first_push", test_accounting_precedes_first_push},
		TestEntry{"test_clone_accounting_precedes_first_push", test_clone_accounting_precedes_first_push},
		TestEntry{"test_plan_policy_configures_already_wired_inputs", test_plan_policy_configures_already_wired_inputs},
		TestEntry{"test_redirected_clone_configures_destination", test_redirected_clone_configures_destination},
		TestEntry{"test_shared_writers_keep_budget_and_eof", test_shared_writers_keep_budget_and_eof}
	};
	return RunSelectedTest(argc, argv, tests);
}
