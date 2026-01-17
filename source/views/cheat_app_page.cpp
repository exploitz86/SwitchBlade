#include "views/cheat_app_page.hpp"
#include "views/cheat_selection_page.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include <switch.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace brls::literals;
using json = nlohmann::json;

constexpr int MaxTitleCount = 64000;

// CheatGameCell implementation
CheatGameCell::CheatGameCell() {
    this->inflateFromXMLRes("xml/cells/cell.xml");
}

CheatGameCell* CheatGameCell::create() {
    return new CheatGameCell();
}

// CheatGameData implementation
CheatGameData::CheatGameData(bool isGfxMode) : isGfxMode(isGfxMode) {
    NsApplicationRecord* records = new NsApplicationRecord[MaxTitleCount]();
    NsApplicationControlData* controlData = nullptr;

    s32 recordCount = 0;
    u64 controlSize = 0;

    if (R_SUCCEEDED(nsListApplicationRecord(records, MaxTitleCount, 0, &recordCount))) {
        for (s32 i = 0; i < recordCount; i++) {
            controlSize = 0;
            free(controlData);
            controlData = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
            if (controlData == nullptr) {
                break;
            }
            memset(controlData, 0, sizeof(NsApplicationControlData));

            if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage,
                records[i].application_id, controlData, sizeof(NsApplicationControlData), &controlSize))) {
                continue;
            }

            if (controlSize < sizeof(controlData->nacp)) {
                continue;
            }

            NacpLanguageEntry* langEntry = nullptr;
            nacpGetLanguageEntry(&controlData->nacp, &langEntry);

            std::string name = "Unknown";
            if (langEntry != nullptr) {
                name = std::string(langEntry->name);
            }

            GameInfo gameInfo;
            gameInfo.name = name;
            gameInfo.tid = records[i].application_id;
            gameInfo.tidFormatted = utils::formatApplicationId(records[i].application_id);

            games.push_back(gameInfo);
        }
        free(controlData);
    }
    delete[] records;

    brls::Logger::debug("{} games found for cheat selection", games.size());
}

int CheatGameData::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int CheatGameData::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return games.size();
}

std::string CheatGameData::titleForHeader(brls::RecyclerFrame* recycler, int section) {
    return "";
}

brls::RecyclerCell* CheatGameData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    auto cell = (CheatGameCell*)recycler->dequeueReusableCell("Cell");

    const auto& game = games[indexPath.row];

    // Set game name as title
    cell->label->setText(game.name);

    // Set subtitle with just TID (Build ID shown on selection page)
    std::string subtitleText = "TID: " + game.tidFormatted;
    cell->subtitle->setText(subtitleText);

    // Set game icon
    uint8_t* icon = utils::getIconFromTitleId(game.tidFormatted);
    if (icon != nullptr) {
        cell->image->setImageFromMem(icon, 0x20000);
        cell->image->setVisibility(brls::Visibility::VISIBLE);
    } else {
        cell->image->setVisibility(brls::Visibility::GONE);
    }

    return cell;
}

void CheatGameData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    const auto& game = games[indexPath.row];

    // Get the parent CheatAppPage to check if we're in GFX mode
    // We need to pass this through somehow - for now, store it in CheatGameData
    brls::Logger::debug("Pushing CheatSelectionPage for {} (TID: {:016X})", game.name, game.tid);
    auto* activity = new brls::Activity(new CheatSelectionPage(game.tid, game.name, this->isGfxMode));
    brls::Application::pushActivity(activity);
}

// CheatAppPage implementation
CheatAppPage::CheatAppPage(bool isGfxCheats) : isGfxCheats(isGfxCheats) {
    // Create the box with recycler from XML
    brls::Box* container = (brls::Box*)brls::View::createFromXMLResource("tabs/cheat_app_page.xml");

    // Get the recycler from the container
    brls::RecyclerFrame* recycler = (brls::RecyclerFrame*)container->getView("recycler");

    gameData = new CheatGameData(isGfxCheats);

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Cell", []() { return CheatGameCell::create(); });
    recycler->setDataSource(gameData, false);

    // Set the container as the content view FIRST
    this->setContentView(container);

    // Then set title and icon AFTER content is set
    std::string title = isGfxCheats ? "menu/cheats_menu/select_game_gfx"_i18n : "menu/cheats_menu/select_game"_i18n;
    this->setTitle(title);

    // Register back button handler
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);
}

brls::View* CheatAppPage::create() {
    return new CheatAppPage();
}
