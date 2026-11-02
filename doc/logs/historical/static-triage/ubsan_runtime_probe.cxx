#include <limits>

int main() {
	volatile int maximum = std::numeric_limits<int>::max();
	return maximum + 1;
}
