#include "ValidationSystem.hpp"
#include <format>
#include <cctype>
#include <algorithm>
#include <string>

using namespace FindTheBug;

static std::string normalizeString(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        result += static_cast<char>(std::tolower(c));
    }
    return result;
}

bool ValidationSystem::suggestMatch(const std::string& submitted, const std::string& expected) const {
    std::string s1 = normalizeString(submitted);
    std::string s2 = normalizeString(expected);

    return s1.find(s2) != std::string::npos || s2.find(s1) != std::string::npos;
}

ValidationResult ValidationSystem::prepareForMaster(
    const std::vector<std::string>& playerAnswers,
    const BugCase& bugCase) const {

    ValidationResult result;
    result.isCorrect = false;
    result.score = 0;
    result.generalMessage = "Aguardando validacao do Mestre";

    const auto& expected = bugCase.correctAnswers;
    const auto& questions = bugCase.solutionQuestions;

    size_t count = std::max(questions.size(), playerAnswers.size());

    for (size_t i = 0; i < count; ++i) {
        std::string question = (i < questions.size()) ? questions[i] : "Pergunta Extra";
        std::string submitted = (i < playerAnswers.size()) ? playerAnswers[i] : "[SEM RESPOSTA]";
        std::string gabarito = (i < expected.size()) ? expected[i] : "[GABARITO INDEFINIDO]";

        bool autoMatch = suggestMatch(submitted, gabarito);

        std::string feedback = std::format(
            "Pergunta: {}\nResposta Equipe: {}\nGabarito: {}\nSugestao do Sistema: {}",
            question,
            submitted,
            gabarito,
            (autoMatch ? "Parece Correto" : "Parece Incorreto")
        );

        result.feedbackPerQuestion.push_back(feedback);
    }

    return result;
}