#include "views/exclude_titles_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>

using namespace brls::literals;

ExcludeTitlesView::ExcludeTitlesView() {
    loadExcludedTitles();

    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setWidth(brls::View::AUTO);
    list->setHeight(brls::View::AUTO);
    list->setPadding(20, 20, 20, 20);

    // Add instruction label
    auto* infoLabel = new brls::Label();
    infoLabel->setText("menu/cheats_menu/exclude_desc"_i18n);
    infoLabel->setFontSize(16);
    infoLabel->setTextColor(nvgRGB(150, 150, 150));
    infoLabel->setMarginBottom(20);
    infoLabel->setFocusable(false);
    list->addView(infoLabel);

    populateList();

    scrollFrame->setContentView(list);
    scrollFrame->setFocusable(true);
    this->setContentView(scrollFrame);

    this->setTitle("menu/cheats_menu/exclude_title"_i18n);
    brls::Application::giveFocus(scrollFrame);

    this->registerAction("menu/cheats_menu/save_exit"_i18n, brls::ControllerButton::BUTTON_B,
        [this](brls::View* view) {
            saveExcludedTitles();
            brls::Application::popActivity();
            return true;
        },
        false);
}

void ExcludeTitlesView::loadExcludedTitles() {
    excludedTitles.clear();
    
    if (!std::filesystem::exists(CHEATS_EXCLUDE)) {
        return;
    }

    std::ifstream file(CHEATS_EXCLUDE);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            if (!line.empty()) {
                std::transform(line.begin(), line.end(), line.begin(), ::toupper);
                excludedTitles.insert(line);
            }
        }
        file.close();
    }
}

void ExcludeTitlesView::saveExcludedTitles() {
    // Ensure parent directory exists to avoid silent write failures
    try {
        auto parent = std::filesystem::path(CHEATS_EXCLUDE).parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }
    } catch (...) {
        brls::Logger::warning("Failed to create directory for exclusions: {}", CHEATS_EXCLUDE);
    }

    std::ofstream file(CHEATS_EXCLUDE);
    if (file.is_open()) {
        for (const auto& tid : excludedTitles) {
            file << tid << "\n";
        }
        file.close();
    }
}

void ExcludeTitlesView::populateList() {
    auto games = utils::getInstalledGames();
    
    if (games.empty()) {
        auto* emptyLabel = new brls::Label();
        emptyLabel->setText("menu/cheats_menu/no_installed_games"_i18n);
        emptyLabel->setFontSize(18);
        emptyLabel->setFocusable(false);
        list->addView(emptyLabel);
        return;
    }

    // Sort by game name
    std::sort(games.begin(), games.end(), 
        [](const auto& a, const auto& b) { return a.first < b.first; });

    int count = 0;
    for (const auto& game : games) {
        std::string gameName = game.first;
        std::string titleId = game.second;

        auto* cell = new brls::RadioCell();
        cell->title->setText(gameName);
        cell->subtitle->setText(titleId);
        // Add spacing between title and subtitle (Title ID)
        cell->subtitle->setMarginTop(10);
        
        // Check if currently excluded
        bool isExcluded = excludedTitles.count(titleId) > 0;
        cell->setSelected(isExcluded);

        // Toggle exclusion on click
        cell->registerClickAction([this, titleId, cell](brls::View* view) {
            bool currentlyExcluded = excludedTitles.count(titleId) > 0;
            
            if (currentlyExcluded) {
                excludedTitles.erase(titleId);
                cell->setSelected(false);
            } else {
                excludedTitles.insert(titleId);
                cell->setSelected(true);
            }
            
            return true;
        });

        list->addView(cell);
        count++;
    }

    brls::Logger::info("Added {} games to exclusion list", count);
}
