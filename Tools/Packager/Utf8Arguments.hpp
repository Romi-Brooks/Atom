#pragma once

#include <string>
#include <vector>

namespace atom::packager {

auto CollectUtf8Args(int argc, char** argv) -> std::vector<std::string>;

}
