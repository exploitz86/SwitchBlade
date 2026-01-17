#include "views/installed_cheats_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include <filesystem>
#include <fstream>
#include <regex>

using namespace brls::literals;

InstalledCheatsView::InstalledCheatsView(const std::string& gameName, const std::string& buildId,
                                         const std::string& cheatFilePath, u64 titleId) {
    // Create scrolling list with cheats
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    auto* cheatsList = new brls::Box();
    cheatsList->setAxis(brls::Axis::COLUMN);
    cheatsList->setWidth(brls::View::AUTO);
    cheatsList->setHeight(brls::View::AUTO);
    cheatsList->setPadding(20, 20, 20, 20);

    // Add file info label
    brls::Label* fileLabel = new brls::Label();
    fileLabel->setText("menu/cheats_menu/cheat_file"_i18n + buildId + ".txt");
    fileLabel->setFontSize(14);
    fileLabel->setMarginBottom(20);
    fileLabel->setFocusable(false);
    cheatsList->addView(fileLabel);

    // Read and parse cheat file
    std::regex cheatsExpr(R"(\[.+\]|\{.+\})"); // Match lines with [cheat name] or {code}
    std::ifstream file(cheatFilePath);
    std::string line;
    int cheatCount = 0;

    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line.size() > 0) {
                // Check if line matches cheat pattern
                if (std::regex_search(line, cheatsExpr)) {
                    brls::Label* cheatLabel = new brls::Label();
                    cheatLabel->setText(line);
                    cheatLabel->setFontSize(16);
                    cheatLabel->setMarginBottom(5);
                    cheatLabel->setFocusable(false);
                    cheatsList->addView(cheatLabel);
                    cheatCount++;
                }
            }
        }
        file.close();
    }

    if (cheatCount == 0) {
        brls::Label* emptyLabel = new brls::Label();
        emptyLabel->setText("menu/cheats_menu/no_cheats_in_file"_i18n);
        emptyLabel->setFontSize(18);
        emptyLabel->setFocusable(false);
        cheatsList->addView(emptyLabel);
    }

    scrollFrame->setContentView(cheatsList);
    scrollFrame->setFocusable(true);
    this->setContentView(scrollFrame);

    // Set title and icon AFTER content is set
    this->setTitle("menu/cheats_menu/installed_cheats"_i18n + " - " + gameName);
    // this->setIcon("romfs:/gui/app_icon.png");

    // Give focus to the scrolling frame explicitly to prevent focus bleeding
    brls::Application::giveFocus(scrollFrame);

    // Register X button to delete cheat file
    this->registerAction("menu/cheats_menu/delete_cheat_file"_i18n, brls::ControllerButton::BUTTON_X,
        [cheatFilePath, buildId](brls::View* view) {
            // Confirmation dialog
            brls::Dialog* confirmDialog = new brls::Dialog("menu/cheats_menu/delete_cheat_file_confirm"_i18n + buildId + ".txt");

            confirmDialog->addButton("hints/cancel"_i18n, []() {});

            confirmDialog->addButton("hints/delete"_i18n, [cheatFilePath]() {
                try {
                    if (std::filesystem::remove(cheatFilePath)) {
                        brls::Dialog* successDialog = new brls::Dialog("menu/cheats_menu/cheat_files_deleted"_i18n);
                        successDialog->addButton("hints/ok"_i18n, []() {
                            // Pop the installed cheats view
                            brls::Application::popActivity();
                        });
                        successDialog->open();
                    } else {
                        brls::Dialog* errorDialog = new brls::Dialog("menu/cheats_menu/failed_to_delete_some"_i18n);
                        errorDialog->addButton("hints/ok"_i18n, []() {});
                        errorDialog->open();
                    }
                } catch (const std::exception& e) {
                    brls::Dialog* errorDialog = new brls::Dialog("menu/cheats_menu/error_deleting"_i18n + std::string(e.what()));
                    errorDialog->addButton("hints/ok"_i18n, []() {});
                    errorDialog->open();
                }
            });

            confirmDialog->open();
            return true;
        });

    // Register back button handler
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);
}
