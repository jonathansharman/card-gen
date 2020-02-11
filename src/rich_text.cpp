//! @file
//! @copyright See <a href="RichText-LICENSE.txt">RichText-LICENSE.txt</a>.

#include "rich_text.hpp"

#include <SFML/Graphics.hpp>

#include <fmt/format.h>

namespace {
	struct chunk {
		sf::Uint32 style_flags = sf::Text::Regular;
		sf::Color color = sf::Color::White;
		sf::String text;
	};

	struct line {
		std::vector<chunk> chunks;
	};

	std::map<sf::String, sf::Color> _colors = { //
		{"default", sf::Color::White},
		{"black", sf::Color::Black},
		{"blue", sf::Color::Blue},
		{"cyan", sf::Color::Cyan},
		{"green", sf::Color::Green},
		{"magenta", sf::Color::Magenta},
		{"red", sf::Color::Red},
		{"white", sf::Color::White},
		{"yellow", sf::Color::Yellow}};

	auto color_from_hex(unsigned argb_hex) -> sf::Color {
		argb_hex |= 0xff000000;
		return sf::Color(argb_hex >> 16 & 0xff, argb_hex >> 8 & 0xff, argb_hex >> 0 & 0xff, argb_hex >> 24 & 0xff);
	}

	auto color_from_string(std::string const& source) -> sf::Color {
		auto result = _colors.find(source);
		if (result != _colors.end()) { return result->second; }
		try {
			return color_from_hex(std::stoi(source, 0, 16));
		} catch (...) {
			// Conversion failed; use default color.
			return sf::Color::White;
		}
	}
}

namespace sfe {
	auto rich_text::add_color(sf::String const& name, sf::Color const& color) -> void {
		_colors[name] = color;
	}

	auto rich_text::add_color(sf::String const& name, unsigned argb_hex) -> void {
		_colors[name] = color_from_hex(argb_hex);
	}

	rich_text::rich_text(sf::String const& source, sf::Font const& font, unsigned character_size)
		: _character_size(character_size), _font(&font) {
		set_string(source);
	}

	auto rich_text::get_string() const -> sf::String {
		return _string;
	}

	auto rich_text::get_source() const -> sf::String {
		return _source;
	}

	auto rich_text::set_string(sf::String const& source) -> void {
		_source = source;

		clear();

		std::vector<line> lines{line{{chunk{}}}};

		sf::Uint32 current_style_flags = sf::Text::Regular;
		sf::Color current_color = sf::Color::White;

		for (auto it = source.begin(); it != source.end(); ++it) {
			switch (*it) {
				case '~':
					current_style_flags ^= sf::Text::Italic;
					lines.back().chunks.push_back(chunk{current_style_flags, current_color});
					break;
				case '*':
					current_style_flags ^= sf::Text::Bold;
					lines.back().chunks.push_back(chunk{current_style_flags, current_color});
					break;
				case '_':
					current_style_flags ^= sf::Text::Underlined;
					lines.back().chunks.push_back(chunk{current_style_flags, current_color});
					break;
				case '#': { // Color
					// Find the end of the color string.
					auto color_end = std::find_if(it + 1, source.end(), [](auto c) { return isspace(c); });
					// Convert string to color. A properly formatted hex string is safely convertible to ANSI.
					current_color = color_from_string(sf::String::fromUtf32(it + 1, color_end).toAnsiString());
					// Advance the iterator past the color string.
					it = color_end;

					lines.back().chunks.push_back(chunk{current_style_flags, current_color});
					break;
				}
				case '\\': // Escape sequence
					++it;
					if (it == source.end()) {
						throw std::domain_error{"Expected formatting control character after '\\'."};
					}
					switch (*it) {
						case '~':
						case '*':
						case '_':
						case '#':
						case '\\':
							lines.back().chunks.back().text += *it;
							break;
						default:
							throw std::domain_error{
								fmt::format("Cannot escape non-control character '{}'.", static_cast<char>(*it))};
					}
					break;
				case '\n': // New line
					lines.push_back(line{{chunk{}}});
					break;
				default:
					lines.back().chunks.back().text += *it;
					break;
			}
		}

		// Build texts and formatting-stripped string and compute bounds.
		sf::Vector2f next_position{};
		_bounds = {0, 0, 0, 0};
		for (auto const& line : lines) {
			for (auto const& chunk : line.chunks) {
				// Construct text.
				_texts.push_back({chunk.text, *_font, _character_size});
				_texts.back().setFillColor(chunk.color);
				_texts.back().setStyle(chunk.style_flags);
				_texts.back().setPosition(next_position);
				// Move next position to the end of the text.
				next_position = _texts.back().findCharacterPos(chunk.text.getSize());
				// Add chunk's text to the string.
				_string += chunk.text;
				// Extend bounds.
				auto const text_bounds = _texts.back().getGlobalBounds();
				auto const right = text_bounds.left + text_bounds.width;
				_bounds.width = std::max(_bounds.width, right - _bounds.left);
				auto const bottom = text_bounds.top + text_bounds.height;
				_bounds.height = std::max(_bounds.height, bottom - _bounds.top);
			}
			if (&line != &lines.back()) {
				// Handle new lines.
				next_position = {0, next_position.y + _font->getLineSpacing(_character_size)};
				_string += '\n';
			}
		}
	}

	auto rich_text::clear() -> void {
		_texts.clear();
		_bounds = sf::FloatRect{};
	}

	auto rich_text::get_character_size() const -> unsigned {
		return _character_size;
	}

	auto rich_text::set_character_size(unsigned size) -> void {
		_character_size = std::max(size, 1u);
		set_string(_source);
	}

	auto rich_text::get_font() const -> sf::Font const* {
		return _font;
	}

	auto rich_text::set_font(sf::Font const& font) -> void {
		_font = &font;
		set_string(_source);
	}

	auto rich_text::get_local_bounds() const -> sf::FloatRect {
		return _bounds;
	}

	auto rich_text::get_global_bounds() const -> sf::FloatRect {
		return getTransform().transformRect(_bounds);
	}

	auto rich_text::draw(sf::RenderTarget& target, sf::RenderStates states) const -> void {
		states.transform *= getTransform();
		for (auto const& text : _texts) {
			target.draw(text, states);
		}
	}
}
