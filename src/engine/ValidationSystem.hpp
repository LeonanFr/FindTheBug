#pragma once

#include <vector>
#include <string>
#include "Types.hpp"
#include "../shared/DTOs.hpp"

namespace FindTheBug {

    class ValidationSystem {
    public:
        ValidationResult prepareForMaster(
            const std::vector<std::string>& playerAnswers,
            const BugCase& bugCase
        ) const;

    private:
        bool suggestMatch(const std::string& submitted, const std::string& expected) const;
    };
}