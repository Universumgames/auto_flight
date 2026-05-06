#include "serializer.hpp"
#include <iomanip>

void serialize(const Route& route, std::ostream& os) {
    for (const auto& point : route) {
        os << std::fixed << std::setw(11) << std::setprecision(9) << point.longitude << "," << point.latitude << "\n";
    }
}