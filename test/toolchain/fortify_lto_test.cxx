// SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#if defined(_FORTIFY_HEADERS_H)
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

int lto_probe(int value);

__attribute__((noinline)) int format_to_buffer(bool overflow, const char* format, ...) {
	char buffer[4];
	std::va_list arguments;
	va_start(arguments, format);
	const int result = std::vsnprintf(buffer, overflow ? 8 : sizeof(buffer), format, arguments);
	va_end(arguments);
	return result;
}

int main() {
	if (lto_probe(4) != 27 || std::to_string(0.125) != "0.125000"
			|| std::to_string(-0.0) != "-0.000000"
			|| format_to_buffer(false, "%s", "ok") != 2) {
		std::cerr << "LTO or C++ formatting probe failed\n";
		return 1;
	}

#if defined(_FORTIFY_HEADERS_H)
	const pid_t child = fork();
	if (child < 0) {
		std::perror("fork");
		return 1;
	}
	if (child == 0) {
		const rlimit no_core{0, 0};
		if (setrlimit(RLIMIT_CORE, &no_core) != 0) {
			std::perror("setrlimit");
			_exit(1);
		}
		format_to_buffer(true, "%s", "ok");
		_exit(0);
	}

	int status = 0;
	if (waitpid(child, &status, 0) != child) {
		std::perror("waitpid");
		return 1;
	}
	if (!WIFSIGNALED(status)
			|| (WTERMSIG(status) != SIGILL && WTERMSIG(status) != SIGTRAP
				&& WTERMSIG(status) != SIGABRT)) {
		std::cerr << "Fortify did not trap the oversized vsnprintf call\n";
		return 1;
	}
	std::cout << "Fortify trapped the oversized vsnprintf call\n";
#endif
	return 0;
}
