#include "ValidationSystem.hpp"

using namespace FindTheBug;

bool ValidationSystem::suggestMatch(const std::string& submitted, const std::string& expected) const {
    auto normalize = [](std::string s) {
        std::string result;
        result.reserve(s.size());
        for (unsigned char c : s) {
            result += static_cast<char>(std::tolower(c));
        }
        return result;
        };

    std::string s1 = normalize(submitted);
    std::string s2 = normalize(expected);

    return s1.find(s2) != std::string::npos || s2.find(s1) != std::string::npos;
}

ValidationResult ValidationSystem::prepareForMaster(
    const std::vector<std::string>& playerAnswers,
    const BugCase& bugCase) const {

    ValidationResult result;
    result.isCorrect = false;
    result.score = 0;

    const auto& expected = bugCase.correctAnswers;
    const auto& questions = bugCase.solutionQuestions;

    for (size_t i = 0; i < questions.size(); ++i) {
        std::string submitted = (i < playerAnswers.size()) ? playerAnswers[i] : "[SEM RESPOSTA]";
        std::string expectedText = (i < expected.size()) ? expected[i] : "[GABARITO NÃO DEFINIDO]";

        bool autoMatch = suggestMatch(submitted, expectedText);

        std::string feedback = std::format(
            "Pergunta: {}\nResposta Equipe: {}\nGabarito: {}\nSugestão do Sistema: {}",
            questions[i],
            submitted,
            expectedText,
            autoMatch ? "CORRESPONDÊNCIA PROVÁVEL" : "REVISÃO MANUAL NECESSÁRIA"
        );

        result.feedbackPerQuestion.push_back(feedback);
    }

    result.generalMessage = "Aguardando decisão do Mestre do Jogo...";

    return result;
}