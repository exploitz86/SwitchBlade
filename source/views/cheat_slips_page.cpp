#include "views/cheat_slips_page.hpp"
#include "views/cheat_slips_download_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include <switch.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace brls::literals;
using json = nlohmann::ordered_json;

constexpr int MaxTitleCount = 64000;

// CheatSlipsGameCell implementation
CheatSlipsGameCell::CheatSlipsGameCell() {
    this->inflateFromXMLRes("xml/cells/cell.xml");
}

CheatSlipsGameCell* CheatSlipsGameCell::create() {
    return new CheatSlipsGameCell();
}

// CheatSlipsGameData implementation
CheatSlipsGameData::CheatSlipsGameData() {
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

    brls::Logger::debug("{} games found for CheatSlips selection", games.size());
}

int CheatSlipsGameData::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int CheatSlipsGameData::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return games.size();
}

std::string CheatSlipsGameData::titleForHeader(brls::RecyclerFrame* recycler, int section) {
    return "";
}

brls::RecyclerCell* CheatSlipsGameData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    auto cell = (CheatSlipsGameCell*)recycler->dequeueReusableCell("Cell");

    const auto& game = games[indexPath.row];

    // Set game name as title
    cell->label->setText(game.name);

    // Set subtitle with TID
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

void CheatSlipsGameData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    const auto& game = games[indexPath.row];

    brls::Logger::debug("Opening CheatSlips download for {} (TID: {:016X})", game.name, game.tid);
    
    // Create a new activity with the download view
    auto* downloadView = new CheatSlipsDownloadView(game.tid, game.name);
    
    auto* activity = new brls::Activity(downloadView);
    brls::Application::pushActivity(activity);
}

// CheatSlipsPage implementation
CheatSlipsPage::CheatSlipsPage() {
    // Create the box with recycler from XML
    brls::Box* container = (brls::Box*)brls::View::createFromXMLResource("tabs/cheat_app_page.xml");

    // Get the recycler from the container
    brls::RecyclerFrame* recycler = (brls::RecyclerFrame*)container->getView("recycler");

    gameData = new CheatSlipsGameData();

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Cell", []() { return CheatSlipsGameCell::create(); });
    recycler->setDataSource(gameData, false);

    // Set the container as the content view
    this->setContentView(container);

    // Set title
    this->setTitle("CheatSlips - " + "menu/cheats_menu/select_game"_i18n);

    // Register back button handler
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);
}

brls::View* CheatSlipsPage::create() {
    return new CheatSlipsPage();
}
