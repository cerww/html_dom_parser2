#pragma once
#include "parsing_stuff.h"
#include <vector>
#include <unordered_map>
#include <fmt/core.h>
#include <variant>

namespace html {

	struct text_node {
		text_node() = default;
		explicit text_node(std::string t) :text(std::move(t)) {}
		std::string text;
	};

	struct node_header {
		std::string name;
		std::unordered_map<std::string, std::string> attributes;
	};

	struct node;

	struct element_node {
		element_node() = default;
		explicit element_node(node_header h) :
			tag_name(std::move(h.name)),
			attributes(std::move(h.attributes)) {}

		std::vector<node> children;
		std::string tag_name;
		std::unordered_map<std::string, std::string> attributes;
	};

	struct node {
		node() = default;
		explicit node(element_node e) :m_data(std::move(e)) {}
		explicit node(text_node e) :m_data(std::move(e)) {}
		explicit node(node_header h) :m_data(std::in_place_type<element_node>, std::move(h)) {}


		std::string& text() {
			return std::get<text_node>(m_data).text;
		}

		const std::string& text() const {
			return std::get<text_node>(m_data).text;
		}

		std::string& tag_name() {
			return std::get<element_node>(m_data).tag_name;
		}

		const std::string& tag_name() const {
			return std::get<element_node>(m_data).tag_name;
		}

		auto& attributes() {
			return std::get<element_node>(m_data).attributes;
		}

		const auto& attributes() const {
			return std::get<element_node>(m_data).attributes;
		}

		auto& children() {
			return std::get<element_node>(m_data).children;
		}

		const auto& children() const {
			return std::get<element_node>(m_data).children;
		}

		bool is_text_node()const noexcept {
			return std::holds_alternative<text_node>(m_data);
		}

		bool is_element_node()const noexcept {
			return std::holds_alternative<element_node>(m_data);
		}

	private:
		std::variant<element_node, text_node> m_data;
	};

	struct document {
		node root;
	};
};



struct attribute_parser {
	parse_result<std::optional<std::pair<std::string_view, std::string_view>>> operator()(std::string_view s) const {
		auto attribute = parse_multi_consecutive(
			s,
			whitespace_parser,
			alpha_word_parser{},
			whitespace_parser,
			char_parser<true>{'='},
			whitespace_parser,
			quote_string_parser{}
		);

		if (!attribute) {
			auto result2 = parse_multi_consecutive(s,whitespace_parser,char_parser<true>{'>'});
			if(!result2) {
				return parse_fail();				
			}else {
				return parse_result(std::nullopt, result2.rest());
			}
		}

		auto [____, name, _, __, ___, value] = attribute.value();

		return parse_result(std::optional(std::make_pair(name, value)), attribute.rest());
	}
};

struct attributes_parser {
	parse_result<std::vector<std::pair<std::string_view, std::string_view>>> operator()(std::string_view s)const {
		std::vector<std::pair<std::string_view, std::string_view>> attribute_list;
		while (true) {
			auto result = attribute_parser{}(s);
			if (!result) {
				return parse_fail();
			}

			auto& t = result.value();
			if(!t) {
				break;
			}

			auto [name, value] = *t;
			s = result.rest();
			attribute_list.emplace_back(name, value);
		}

		return parse_result(std::move(attribute_list), s);
	}
};

struct node_header_parser {
	parse_result<std::pair<html::node_header, bool>> operator()(std::string_view s)const {
		auto parse_result1 = parse_multi_consecutive(
			s, char_parser<true>{'<'}, alpha_numeric_word_parser{}, whitespace_parser, attributes_parser{}, whitespace_parser, char_parser<false>{'/'}, char_parser<true>{'>'});
		if (!parse_result1)
			return parse_fail();
		auto [_, name, _____, attributes, __, is_done_node, ___] = parse_result1.value();//rename some stuff

		html::node_header ret;
		ret.name = std::string(name);
		ret.attributes.reserve(attributes.size());
		for (auto& attribute : attributes) {
			ret.attributes.insert(std::move(attribute));
		}
		return parse_result(std::make_pair(std::move(ret), is_done_node), parse_result1.rest());//;-;
	}
};

struct node_end_parser {
	parse_result<std::string_view> operator()(std::string_view s) const {
		if (s.size() >= 3 && s.substr(0, 2) == "</") {
			s.remove_prefix(2);
			const auto gt_pos = s.find_first_of("> \t\n");//spaces aren't' allowed in closing tags
			if (gt_pos != std::string_view::npos && s[gt_pos] == '>') {//found > before a space
				const auto end_tag_name = s.substr(0, gt_pos);
				s.remove_prefix(gt_pos + 1);
				return parse_result(end_tag_name, s);
			}
		}
		return parse_fail();
	}
};

struct html_text_node_parser {
	parse_result<std::string_view> operator()(std::string_view data)const {
		const auto next_pos = std::max(1ll, std::distance(data.begin(),std::find(data.begin(), data.end(),'<')));//always parses at least 1 char
		return parse_result(data.substr(0, next_pos), data.substr(next_pos));
	}
};

struct comment_parser {
	parse_result<std::string_view> operator()(std::string_view data) const noexcept {
		if (data.size() >= 6 && data.substr(0, 4) == "<!--") {
			const size_t where = data.find("-->");
			if (where == std::string_view::npos) {
				return parse_fail();
			}
			else {
				auto ret = data.substr(4, where - 4);
				data.remove_prefix(where + 3);
				return parse_result(ret, data);
			}

		}return parse_fail();
	}
};

struct legacy_string_doctype_parser {
	parse_result<bool> operator()(std::string_view data) const noexcept {
		auto result = parse_multi_consecutive(
			data,
			whitespace_parser,
			ci_str_parser<true>("SYSTEM"),
			whitespace_parser,
			quote_parser{},
			str_parser<true>("about:legacy-compat"),
			quote_parser{}
		);
		if (!result) {
			return parse_fail();
		}
		auto [num_ws1, _, num_ws2, quote1, __, quote2] = result.value();

		if (num_ws1 == 0 ||
			num_ws2 == 0 ||
			quote1 != quote2)
		{
			return parse_fail();
		}


		return parse_result(true, result.rest());
	}
};

struct doctype_parser {
	parse_result<std::string_view> operator()(std::string_view data)const {
		auto parse_result_ = parse_multi_consecutive(
			data,
			ci_str_parser("<!doctype"),
			whitespace_parser,
			ci_str_parser("html"),
			optional_parser(legacy_string_doctype_parser()),
			whitespace_parser,
			char_parser{'>'}
		);
		if (parse_result_) {
			auto [_, __, ______, is_legacy, ____, _____] = parse_result_.value();
			return parse_result(std::string_view(""), data);
		}
		else {
			return parse_fail();
		}

	}
};

struct html_parser {
	static bool is_self_closing_tag(std::string_view name) {
		using namespace std::literals;
		static constexpr std::array self_closing_tags = {//excluding br,hr
			"area"sv,"base"sv,"col"sv,"embed"sv,
			"img"sv,"input"sv,"link"sv,"meta"sv,"param"sv,
			"source"sv,"track"sv,"wbr"sv,"keygen"sv,"command"sv,"menuitem"sv
		};

		if(name == "div"sv || name == "ul"sv || name == "span"sv|| name.size() >=9 || name.size() == 1) {//check common non-selfclosing tags first to avoid checking the whole list for them
			return false;
		}
		if(name.size() == 2) {
			return name != "hr"sv && name != "br"sv;
		}

		return std::find(self_closing_tags.begin(), self_closing_tags.end(), name)!= self_closing_tags.end();
		
	}

	parse_result<html::document> operator()(std::string_view html_document_text) const {

		//document.
		std::vector<html::node> node_stack;
		node_stack.emplace_back(html::node_header{ "root$$$",{} });

		auto add_text = [&](auto&& ... strs) {
			//if last thing is a text node, append text to it instead of adding a new node
			if (!node_stack.back().children().empty() &&
				node_stack.back().children().back().is_text_node())
			{
				((node_stack.back().children().back().text() += strs, 0), ...);
			}
			else {
				auto str = (std::string(strs) + ...);
				node_stack.back().children().emplace_back(html::text_node(std::move(str)));
			}
		};

		while (!html_document_text.empty()) {
			auto t = parse_first_of(
				html_document_text,
				node_header_parser(),
				node_end_parser(),
				comment_parser(),
				html_text_node_parser()
			);
			//t is always valid
			auto&& [result, rest] = *t;
			html_document_text = rest;

			if (result.index() == 0) {//node start
				auto& [node, is_immediate_close] = result.get<0>();
				//node_stack.push_back();
				if (is_immediate_close || is_self_closing_tag(node.name)) {
					node_stack.back().children().emplace_back(std::move(node));
				}
				else {
					node_stack.emplace_back(std::move(node));
				}
			}
			else if (result.index() == 1) {//node end
				const auto tag_name = result.get<std::string_view>();
				const auto ending_node_it = std::find_if(node_stack.rbegin(), node_stack.rend(), [&](const html::node& n) {
					return n.tag_name() == tag_name;
				});
				//no node with same tag name, treat it as text
				if (ending_node_it == node_stack.rend()) {
					add_text("</", tag_name, ">");
				}
				else {
					const size_t num_nodes_to_close = std::distance(node_stack.rbegin(), ending_node_it) + 1;//+1 for [) range to []

					for (auto _ = 0; _ < num_nodes_to_close; ++_) {
						auto temp_node = std::move(node_stack.back());
						node_stack.pop_back();
						node_stack.back().children().push_back(std::move(temp_node));
					}
				}
			}
			else if (result.index() == 2) {//comment
			   //do nothing
			}
			else if (result.index() == 3) {//text
				add_text(result.get<std::string_view>());
			}else {
				//unreachable
			}
		}

		while (node_stack.size() != 1) {
			auto temp_node = std::move(node_stack.back());
			node_stack.pop_back();
			node_stack.back().children().push_back(std::move(temp_node));
		}

		return parse_result(html::document{ std::move(node_stack.front()) }, html_document_text);
	}
};
