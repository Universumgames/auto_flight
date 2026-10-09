#pragma once
#include <iosfwd>

#include "../flight_com/types.hpp"

void simpleSerialize(const Route& route, std::ostream& os);
