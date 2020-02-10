//! @file
//! @copyright See <a href="RichText-LICENSE.txt">RichText-LICENSE.txt</a>.

#include "rich_text.hpp"

#include <SFML/Graphics.hpp>

#include <sstream>

namespace {
	struct chunk {
		sf::Uint32 style_flags = sf::Text::Regular;
		sf::Color color = sf::Color::White;
		sf::String text;
		bool ends_in_newline = false;
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

		std::vector<chunk> chunks{{}};

		chunk* current_chunk = &(chunks.front());
		bool escaped = false;

		for (auto it = source.begin(); it != source.end(); ++it) {
			auto new_chunk = [&] {
				chunks.push_back(chunk{chunks.back().style_flags, chunks.back().color});
				current_chunk = &chunks.back();
			};

			auto new_formatted_chunk = [&](sf::Text::Style style) {
				if (escaped) {
					current_chunk->text += *it;
					escaped = false;
					return;
				}
				chunks.push_back(chunk{chunks.back().style_flags ^ style, chunks.back().color});
				current_chunk = &chunks.back();
			};

			switch (*it) {
				case '~':
					new_formatted_chunk(sf::Text::Italic);
					break;
				case '*':
					new_formatted_chunk(sf::Text::Bold);
					break;
				case '_':
					new_formatted_chunk(sf::Text::Underlined);
					break;
				case '#': { // Color
					if (escaped) {
						current_chunk->text += *it;
						escaped = false;
						break;
					}
					auto color_end = std::find_if(it + 1, source.end(), [](auto c) { return isspace(c); });
					new_chunk();
					// A properly formatted hex string is safely convertible to ANSI.
					current_chunk->color = color_from_string(sf::String::fromUtf32(it + 1, color_end).toAnsiString());
					it = color_end;
					break;
				}
				case '\\': // Escape sequence for escaping formatting characters
					if (it != source.end()) {
						switch (*(it + 1)) {
							case '~':
							case '*':
							case '_':
							case '#':
								escaped = true;
								break;
							default:
								break;
						}
					}
					if (!escaped) { current_chunk->text += *it; }
					break;
				case '\n': // Make a new chunk in the case of a newline.
					current_chunk->ends_in_newline = true;
					new_chunk();
					break;
				default:
					escaped = false;
					current_chunk->text += *it;
					break;
			}
		}

		// Build string from source, stripped of formatting characters.
		_string.clear();
		for (auto const& chunk : chunks) {
			if (chunk.ends_in_newline || !chunk.text.isEmpty()) {
				_string += chunk.text + (chunk.ends_in_newline ? "\n" : "");
			}
		}

		// Used to compute bounds and character positions of actual texts
		sf::Text text{_string, *_font, _character_size};
		_bounds = text.getLocalBounds();

		std::size_t cursor = 0;
		chunk const* last_chunk = nullptr;
		for (auto const& chunk : chunks) {
			sf::Text t;
			t.setFillColor(chunk.color);
			t.setString(chunk.text);
			t.setStyle(chunk.style_flags);
			t.setCharacterSize(_character_size);

			t.setFont(*_font);

			t.setPosition(text.findCharacterPos(cursor));

			if (last_chunk != nullptr && last_chunk->ends_in_newline) {
				t.setPosition(0, t.getPosition().y + _font->getLineSpacing(_character_size));
				++cursor;
			}

			_texts.push_back(t);
			cursor += chunk.text.getSize();
			last_chunk = &chunk;
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
