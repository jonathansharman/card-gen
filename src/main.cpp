//! @file
//! @copyright See <a href="LICENSE.txt">LICENSE.txt</a>.

#include <card-gen/card-gen.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>

auto main(int argc, char* argv[]) -> int {
	if (argc != 3) {
		fmt::print("Usage: card-gen input-filename output-filename\n");
		return 0;
	}

	std::ifstream fin{argv[1]};
	if (!fin.is_open()) {
		fmt::print("Could not open card specification file \"{}\".", argv[1]);
		return 0;
	}

	try {
		nlohmann::json j;
		fin >> j;
		cg::card const c{j};
		if (!c.render(argv[2])) { fmt::print("Failed to save card image to \"{}\".", argv[2]); }
	} catch (std::exception& ex) { fmt::print("Error: {}", ex.what()); }
}
