// Intentionally compiled as C++ (see CMake) to use SimpleBLE C++ API.
#include <iostream>
#include <string>
#include <vector>

#include <simpleble/SimpleBLE.h>

int main(int argc, char** argv) {
	try {
		auto adapters = SimpleBLE::Adapter::get_adapters();
		if (adapters.empty()) {
			std::cout << "No Bluetooth adapters found." << std::endl;
			return 0;
		}

		for (auto& adapter : adapters) {
			std::cout << "Adapter: " << adapter.identifier() << " (" << adapter.address() << ")" << std::endl;

			// Scan for a short period
			adapter.scan_for(3000);

			auto peripherals = adapter.scan_get_results();
			if (peripherals.empty()) {
				std::cout << "  No devices found." << std::endl;
				continue;
			}

			for (auto& peripheral : peripherals) {
				std::string name = peripheral.identifier();
				std::string addr = peripheral.address();
				bool connected = false;
				// Query connection status (may be false unless connected elsewhere)
				try {
					connected = peripheral.is_connected();
				} catch (...) {
					connected = false;
				}
				std::cout << "  - " << (name.empty() ? std::string("<unknown>") : name)
						  << " [" << addr << "]"
						  << "  status: " << (connected ? "connected" : "disconnected")
						  << std::endl;
			}
		}

		return 0;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unknown error." << std::endl;
		return 1;
	}
}
