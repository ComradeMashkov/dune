#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace dune::notebook {

int run_cli(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& error);
void print_usage(std::ostream& output);

} // namespace dune::notebook
