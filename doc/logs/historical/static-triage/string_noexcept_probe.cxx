#include <StormByte/safe/exception.hxx>
#include <StormByte/safe/string.hxx>

#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {
	bool fail_large_allocations = false;
}

extern "C" void* __libc_malloc(std::size_t) noexcept;

extern "C" void* malloc(std::size_t size) noexcept {
	if (fail_large_allocations && size >= 1024 * 1024)
		return nullptr;
	return __libc_malloc(size);
}

int main(int argc, char** argv) {
	std::set_terminate([] {
		constexpr char text[] = "TERMINATE: exception crossed noexcept\n";
		write(STDERR_FILENO, text, sizeof(text) - 1);
		std::_Exit(86);
	});
	const std::string source(2 * 1024 * 1024, 'x');
	StormByte::Safe::String text;
	fail_large_allocations = true;
	try {
		if (argc == 2 && std::string_view{argv[1]} == "assign")
			text.assign(std::string_view{source});
		else {
			StormByte::Safe::String copy{std::string_view{source}};
			static_cast<void>(copy);
		}
	} catch (const StormByte::Safe::AllocationError&) {
		constexpr char message[] = "CAUGHT: AllocationError reached caller\n";
		write(STDOUT_FILENO, message, sizeof(message) - 1);
		return 0;
	}
	return 1;
}
