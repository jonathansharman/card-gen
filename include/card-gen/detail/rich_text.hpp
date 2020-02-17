//! @file
//! @copyright See <a href="RichText-LICENSE.txt">RichText-LICENSE.txt</a>.
//! @brief Extends SFML with rich text support. Adapted from https://bitbucket.org/jacobalbano/sfml-richtext/src/default/.

#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/String.hpp>

#include <vector>

namespace sfe {
	struct rich_text
		: sf::Drawable
		, sf::Transformable //
	{
		// Set names for color substitutions (for example, ff0000 would be substituted for "red")
		static auto add_color(sf::String const& name, sf::Color const& color) -> void;
		static auto add_color(sf::String const& name, unsigned argb_hex) -> void;

		rich_text(sf::String const& source, unsigned character_size = 30);

		auto get_source() const -> sf::String;
		auto set_source(sf::String const& source) -> void;
		auto clear() -> void;

		auto get_character_size() const -> unsigned;
		auto set_character_size(unsigned size) -> void;

		auto get_local_bounds() const -> sf::FloatRect;
		auto get_global_bounds() const -> sf::FloatRect;

	private:
		auto draw(sf::RenderTarget& target, sf::RenderStates states) const -> void;

		std::vector<sf::Text> _texts;

		unsigned _character_size;

		sf::String _source;
		sf::FloatRect _bounds;
	};
}
