// SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial

#include <string>

int main(int argc, char**) {
	return std::to_string(static_cast<double>(argc) / 7.0).empty();
}
