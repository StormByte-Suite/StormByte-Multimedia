#include "test_helpers.hxx"

#include <StormByte/multimedia/log.hxx>

#include <sstream>

int main() {
	using StormByte::Logger::Level;
	using StormByte::Logger::Log;
	using StormByte::Multimedia::UseLog;
	std::ostringstream firstOutput, secondOutput;
	auto first = StormByte::Safe::Shared<Log>::MakePointer<Log>(firstOutput, Level::LowLevel);
	auto second = StormByte::Safe::Shared<Log>::MakePointer<Log>(secondOutput, Level::LowLevel);
	auto duration = UseLog(first, "Duration");
	auto encoder = UseLog(first, "Encoder");
	auto decoder = UseLog(encoder, "Decoder");
	auto other = UseLog(second, "Encoder");
	*duration << Level::Error << "first-duration" << std::endl;
	*encoder << Level::Error << "first-encode" << std::endl;
	*decoder << Level::Error << "first-decode" << std::endl;
	*other << Level::Error << "second-encode" << std::endl;
	TEST_REQUIRE(firstOutput.str().find("first-duration") != std::string::npos);
	TEST_REQUIRE(firstOutput.str().find("first-encode") != std::string::npos);
	TEST_REQUIRE(firstOutput.str().find("second-encode") == std::string::npos);
	TEST_REQUIRE(secondOutput.str().find("second-encode") != std::string::npos);
	TEST_REQUIRE(secondOutput.str().find("first-") == std::string::npos);
	TEST_REQUIRE(firstOutput.str().find("StormByte/Multimedia/Decoder") != std::string::npos);
	TEST_REQUIRE(firstOutput.str().find("Encoder/StormByte") == std::string::npos);
	auto module = UseLog(second, {});
	auto nested = UseLog(module, "Decoder");
	*nested << Level::Error << "second-decode" << std::endl;
	TEST_REQUIRE(secondOutput.str().find("StormByte/Multimedia/Decoder") != std::string::npos);
	TEST_REQUIRE(secondOutput.str().find("Multimedia/StormByte") == std::string::npos);
	TEST_REQUIRE(!UseLog({}, "Decoder"));
	return 0;
}
