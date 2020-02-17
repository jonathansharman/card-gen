//! @file
//! @copyright See <a href="RichText-LICENSE.txt">RichText-LICENSE.txt</a>.

#include "rich_text.hpp"

#include <SFML/Graphics.hpp>

#include <fmt/format.h>

#include <map>

namespace {
	struct format {
		sf::Font* font = nullptr;
		sf::Uint32 style_flags = sf::Text::Regular;
		sf::Color fill_color = sf::Color::White;
		sf::Color outline_color = sf::Color::White;
		float outline_thickness = 0;
	};

	struct chunk {
		format format;
		sf::String text;
	};

	enum class align { left, center, right };

	struct line {
		std::vector<chunk> chunks;
		align alignment = align::left;
	};

	std::map<std::string, sf::Font> _fonts;

	std::map<std::string, sf::Color> _colors = { //
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

	rich_text::rich_text(sf::String const& source, unsigned character_size) : _character_size(character_size) {
		set_source(source);
	}

	auto rich_text::get_source() const -> sf::String {
		return _source;
	}

	auto rich_text::set_source(sf::String const& source) -> void {
		_source = source;

		clear();

		format current_format;
		std::vector<line> lines{line{{chunk{current_format}}}};

		for (auto it = source.begin(); it != source.end(); ++it) {
			static auto apply_formatting = [&] {
				if (!lines.back().chunks.back().text.isEmpty()) {
					// Start a new chunk if the current chunk has text.
					lines.back().chunks.push_back(chunk{current_format});
				} else {
					// Otherwise, update current chunk.
					lines.back().chunks.back().format = current_format;
				}
			};
			switch (*it) {
				case '/': // Italic
					current_format.style_flags ^= sf::Text::Italic;
					apply_formatting();
					break;
				case '*': // Bold
					current_format.style_flags ^= sf::Text::Bold;
					apply_formatting();
					break;
				case '_': // Underline
					current_format.style_flags ^= sf::Text::Underlined;
					apply_formatting();
					break;
				case '~': // Strikethrough
					current_format.style_flags ^= sf::Text::StrikeThrough;
					apply_formatting();
					break;
				case '[': { // Tag
					++it;
					// Find the end of the tag and advance the iterator.
					auto const tag_end = std::find(it, source.end(), sf::Uint32{']'});
					if (tag_end == source.end()) { throw std::domain_error{"Missing ']' in tag."}; }
					// Split into command and argument.
					auto const command_end = std::find(it, tag_end, sf::Uint32{' '});
					auto const command = sf::String::fromUtf32(it, command_end);
					auto const arg = sf::String::fromUtf32(command_end + 1, tag_end).toAnsiString();
					// Handle the tag.
					if (command == "fill-color") {
						current_format.fill_color = color_from_string(arg);
						apply_formatting();
					} else if (command == "outline-color") {
						current_format.outline_color = color_from_string(arg);
						apply_formatting();
					} else if (command == "outline-thickness") {
						current_format.outline_thickness = std::stof(arg);
						apply_formatting();
					} else if (command == "font") {
						// First = (font name, font) pair; second = whether insertion occurred.
						auto result = _fonts.try_emplace(arg);
						if (result.second) {
							// Cache miss. Need to load font.
							if (!result.first->second.loadFromFile(arg)) {
								throw std::runtime_error{fmt::format("Could not load font from \"{}\".", arg)};
							}
						}
						current_format.font = &result.first->second;
						apply_formatting();
					} else if (command == "align") {
						if (arg == "left") {
							lines.back().alignment = align::left;
						} else if (arg == "center") {
							lines.back().alignment = align::center;
						} else if (arg == "right") {
							lines.back().alignment = align::right;
						} else {
							throw std::domain_error{fmt::format("Invalid alignment: {}.", arg)};
						}
					}
					// Advance the iterator to the end of the tag (it will be incremented past at the end by the
					// loop).
					it = tag_end;
					break;
				}
				case '\\': // Escape sequence
					++it;
					if (it == source.end()) {
						throw std::domain_error{"Expected formatting control character after '\\'."};
					}
					switch (*it) {
						case '/':
						case '*':
						case '_':
						case '~':
						case '[':
						case '\\':
							lines.back().chunks.back().text += *it;
							break;
						default:
							throw std::domain_error{
								fmt::format("Cannot escape non-control character '{}'.", static_cast<char>(*it))};
					}
					break;
				case '\n': // New line
					lines.push_back(line{{chunk{current_format}}});
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
			float line_spacing = 0;
			for (auto const& chunk : line.chunks) {
				// Construct text.
				if (chunk.format.font == nullptr) { throw std::domain_error{"Text missing font specification."}; }
				line_spacing = std::max(line_spacing, chunk.format.font->getLineSpacing(_character_size));
				_texts.push_back({chunk.text, *chunk.format.font, _character_size});
				_texts.back().setStyle(chunk.format.style_flags);
				_texts.back().setFillColor(chunk.format.fill_color);
				_texts.back().setOutlineColor(chunk.format.outline_color);
				_texts.back().setOutlineThickness(chunk.format.outline_thickness);
				// Round next_position to avoid text blurriness.
				_texts.back().setPosition(std::roundf(next_position.x), std::roundf(next_position.y));
				// Move next position to the end of the text.
				next_position = _texts.back().findCharacterPos(chunk.text.getSize());
				// Extend bounds.
				auto const text_bounds = _texts.back().getGlobalBounds();
				auto const right = text_bounds.left + text_bounds.width;
				_bounds.width = std::max(_bounds.width, right - _bounds.left);
				auto const bottom = text_bounds.top + text_bounds.height;
				_bounds.height = std::max(_bounds.height, bottom - _bounds.top);
			}
			//! @todo Align line.
			if (&line != &lines.back()) {
				// Handle new lines.
				next_position = {0, next_position.y + line_spacing};
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
		set_source(_source);
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
