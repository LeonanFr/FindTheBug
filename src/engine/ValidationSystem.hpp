#pragma once

#include "../shared/DTOS.hpp"
#include "Types.hpp"

namespace FindTheBug {
	
	class ValidationSystem {
	public:
		explicit ValidationSystem() = default;

		ValidationResult prepareForMaster(
			const std::vector<std::string>& playerAnswers,
			const BugCase& bugCase
		) const;

	private:
		bool suggestMatch(const std::string& submitted, const std::string& expected) const;
	};

}