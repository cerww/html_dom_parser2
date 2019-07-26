#include "html_parser.h"
#include <fstream>
#include <range/v3/all.hpp>
#include <iostream>


template<typename fn>
void visit_nodes(html::node& n,fn&& f) {
	f(n);
	if (n.is_element_node()) {
		for (auto& child_node : n.children()) {
			visit_nodes(child_node, f);
		}
	}
}

void test_github() {
	std::ifstream file("githubtest.html", std::ios::in);
	std::string text;
	text.reserve(10000);
	while(!file.eof()) {
		std::string temp;
		std::getline(file, temp);
		text += temp;
	}

	auto t = html_parser()(text);
	int q = 0;
	auto html_dom = std::move(t.value());

	visit_nodes(html_dom.root, [](html::node& node) {
		if (node.is_element_node()) {
			std::cout << node.tag_name() << ' ';
			for (auto&& attribute : node.attributes()) {
				std::cout << attribute.first << "=" << attribute.second << ' ';
			}
			std::cout << std::endl;
		}
	});


	int i = 0;
}

int main(){

	std::string text = R"(
	
<html>
	<head>
	</head>
	<body>
		<div>
			<p id = "n" class = "bonk">
				<c>

				</c>
				<p>

			
		</div>
		<div>
		</div>

		<div class = "bop" id = "lop">
			blarg
		</div>

		<img src = "rawr"/>

		
	</body>

</html>



	)";

	


	auto t = html_parser()(text);
	auto html_dom = std::move(t.value());

	visit_nodes(html_dom.root,[](html::node& node) {
		if(node.is_element_node()) {
			std::cout << node.tag_name()<<' ';
			for(auto&& attribute:node.attributes()) {
				std::cout << attribute.first << "=" << attribute.second << ' ';
			}
			std::cout << std::endl;
		}
	});
	int i = 0;

	test_github();

	while (1);


	return 0;
}