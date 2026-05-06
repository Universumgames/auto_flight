#pragma once
#include <iosfwd>

#include "route.hpp"

void serialize(const Route& route, std::ostream& os);
