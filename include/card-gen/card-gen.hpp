//! @file
//! @copyright See <a href="LICENSE.txt">LICENSE.txt</a>.

#include "detail/rich_text.hpp"
#include "detail/visitation.hpp"

#include <SFML/Graphics.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <fstream>
#include <variant>

namespace cg {
	struct text {
		sf::String markup;
		unsigned size;

		text(sf::String markup, std::string font, unsigned size) : markup{markup}, size{size} {}

		text(nlohmann::json const& j) : markup{j.at("markup").get<std::string>()}, size{j.at("size")} {}
	};

	struct image {
		std::string path;
		sf::Vector2f size;

		image(std::string path, sf::Vector2f size = {1, 1}) : path{path}, size{size} {}

		image(nlohmann::json const& j) : path{j.at("path").get<std::string>()} {
			auto size_it = j.find("size");
			size = size_it == j.end() ? sf::Vector2f{1, 1} : sf::Vector2f{size_it->at(0), size_it->at(1)};
		}
	};

	struct element {
		std::variant<text, image> text_or_image;
		sf::Vector2f pos{0, 0};
		sf::Vector2f origin{0, 0};
	};

	struct card {
		sf::Vector2i size;
		std::vector<element> elements;

		card(sf::Vector2i size) : size{size} {}

		card(nlohmann::json const& j) {
			// Get card size.
			auto j_size = j.at("size");
			size = {j_size.at(0), j_size.at(1)};

			// Get card elements.
			for (auto const& j_element : j["elements"]) {
				// Get position; default to top-left.
				auto const pos_it = j_element.find("pos");
				sf::Vector2f const pos = pos_it == j_element.end() //
					? sf::Vector2f{0, 0}
					: sf::Vector2f{pos_it->at(0), pos_it->at(1)};

				// Get origin; default to top-left.
				auto const origin_it = j_element.find("origin");
				sf::Vector2f const origin = origin_it == j_element.end() //
					? sf::Vector2f{0, 0}
					: sf::Vector2f{origin_it->at(0), origin_it->at(1)};

				// An element must have text or an image but not both.
				auto const text_or_image = [&]() -> std::variant<text, image> {
					auto const image_it = j_element.find("image");
					if (image_it != j_element.end()) {
						assert(!j_element.contains("text"));
						return image{*image_it};
					} else {
						return text{j_element.at("text")};
					}
				}();

				elements.push_back({text_or_image, pos, origin});
			}
		}

		auto render(std::string const& output_path) const -> bool {
			sf::RenderTexture card_texture;
			card_texture.create(size.x, size.y);
			card_texture.clear();

			for (auto const& element : elements) {
				sf::Vector2f const rounded_pos{std::roundf(size.x * element.pos.x), std::roundf(size.y * element.pos.y)};
				match(
					element.text_or_image,
					[&](text t) {
						sfe::rich_text rich_text{t.markup, t.size};
						rich_text.setPosition(rounded_pos);
						auto const bounds = rich_text.get_local_bounds();
						rich_text.setOrigin( //
							std::roundf(bounds.width * element.origin.x),
							std::roundf(bounds.height * element.origin.y));
						card_texture.draw(rich_text);
					},
					[&](image i) {
						sf::Texture image_texture;
						image_texture.loadFromFile(i.path);
						sf::Sprite image_sprite{image_texture};
						image_sprite.setPosition(rounded_pos);
						auto const image_texture_size = image_texture.getSize();
						image_sprite.setScale(
							i.size.x * size.x / image_texture_size.x, i.size.y * size.y / image_texture_size.y);
						image_sprite.setOrigin( //
							std::roundf(image_texture_size.x * element.origin.x),
							std::roundf(image_texture_size.y * element.origin.y));
						card_texture.draw(image_sprite);
					});
			}
			card_texture.display();
			return card_texture.getTexture().copyToImage().saveToFile(output_path);
		}
	};
}
