#include "views/outdated_titles_page.hpp"
#include "api/net.hpp"
#include "utils/constants.hpp"
#include "utils/utils.hpp"
#include <borealis.hpp>
#include <fmt/format.h>

using namespace brls::literals;

// OutdatedTitleCell implementation
OutdatedTitleCell::OutdatedTitleCell() {
    this->inflateFromXMLRes("xml/cells/cell.xml");
}

OutdatedTitleCell* OutdatedTitleCell::create() {
    return new OutdatedTitleCell();
}

// OutdatedTitleDetailsPage implementation
OutdatedTitleDetailsPage::OutdatedTitleDetailsPage(const std::string& gameName,
                                                   u32 currentVersion,
                                                   u32 latestVersion,
                                                   const std::string& latestBuildId,
                                                   const std::vector<std::pair<std::string, std::string>>& missingDlcInfo)
{
    // Load container from XML
    brls::Box* container = (brls::Box*)brls::View::createFromXMLResource("tabs/outdated_title_details_page.xml");

    // Get the details box from the container
    brls::Box* detailsList = (brls::Box*)container->getView("details_box");

    // Add version info if outdated
    bool hasUpdate = (latestVersion > 0 && currentVersion < latestVersion);
    if (hasUpdate) {
        auto* versionHeader = new brls::Header();
        versionHeader->setTitle("menu/outdated_titles/update_available"_i18n);
        detailsList->addView(versionHeader);

        auto* versionCell = new brls::DetailCell();
        versionCell->setText("menu/outdated_titles/version"_i18n);
        versionCell->setDetailText(fmt::format("v{} → v{} ({})",
                                               currentVersion,
                                               latestVersion,
                                               latestBuildId));
        detailsList->addView(versionCell);
    }

    // Add missing DLCs
    if (!missingDlcInfo.empty()) {
        auto* dlcHeader = new brls::Header();
        dlcHeader->setTitle(fmt::format("menu/outdated_titles/missing_dlc"_i18n, missingDlcInfo.size()));
        detailsList->addView(dlcHeader);

        for (const auto& [dlcName, dlcId] : missingDlcInfo) {
            auto* dlcCell = new brls::DetailCell();
            dlcCell->setText(dlcName);
            dlcCell->setDetailText(dlcId);
            detailsList->addView(dlcCell);
        }
    }

    // Set the container as content view
    this->setContentView(container);

    // Set title and register B button AFTER setting content
    this->setTitle(gameName);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}

// OutdatedTitlesData implementation
OutdatedTitlesData::OutdatedTitlesData(const std::vector<OutdatedTitle>& titles)
    : outdatedTitles(titles)
{
}

int OutdatedTitlesData::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int OutdatedTitlesData::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return outdatedTitles.size();
}

brls::RecyclerCell* OutdatedTitlesData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) {
    auto* cell = (OutdatedTitleCell*)recycler->dequeueReusableCell("OutdatedTitleCell");
    const auto& title = outdatedTitles[index.row];

    // Set game name as title
    cell->title->setText(title.name);

    // Build subtitle with version info and DLC count
    std::string subtitle = fmt::format("TID: {}", title.titleId);

    if (title.latestVersion > 0 && title.currentVersion < title.latestVersion) {
        subtitle += fmt::format(" | v{} (local) → v{} (latest)",
                                title.currentVersion, title.latestVersion);
    }

    if (title.missingDlcCount > 0) {
        subtitle += fmt::format(" + {} DLC", title.missingDlcCount);
    }

    cell->subtitle->setText(subtitle);

    // Set game icon
    uint8_t* icon = utils::getIconFromTitleId(title.titleId);
    if (icon != nullptr) {
        cell->image->setImageFromMem(icon, 0x20000);
        cell->image->setVisibility(brls::Visibility::VISIBLE);
    } else {
        cell->image->setVisibility(brls::Visibility::GONE);
    }

    return cell;
}

void OutdatedTitlesData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    const auto& title = outdatedTitles[indexPath.row];

    auto* detailsPage = new OutdatedTitleDetailsPage(
        title.name,
        title.currentVersion,
        title.latestVersion,
        title.latestBuildId,
        title.missingDlcInfo
    );

    brls::Application::pushActivity(new brls::Activity(detailsPage), brls::TransitionAnimation::FADE);
}

std::string OutdatedTitlesData::titleForHeader(brls::RecyclerFrame* recycler, int section) {
    return "";
}

// OutdatedTitlesPage implementation
OutdatedTitlesPage::OutdatedTitlesPage()
{
    // Load versions database and check for outdated titles
    loadVersionsData();
    checkForOutdatedTitles();

    if (versionsData.empty() || outdatedTitles.empty()) {
        // Show error or no outdated titles message
        auto* scrollFrame = new brls::ScrollingFrame();
        auto* list = new brls::Box();
        list->setAxis(brls::Axis::COLUMN);
        list->setMarginTop(20);
        list->setMarginRight(20);
        list->setMarginBottom(20);
        list->setMarginLeft(20);

        auto* notFoundLabel = new brls::Label();
        if (versionsData.empty()) {
            notFoundLabel->setText("menu/outdated_titles/versions_fetch_failed"_i18n);
        } else {
            notFoundLabel->setText("menu/outdated_titles/no_outdated_titles"_i18n);
        }
        list->addView(notFoundLabel);

        scrollFrame->setContentView(list);
        this->setContentView(scrollFrame);
    } else {
        // Load container with recycler from XML
        brls::Box* container = (brls::Box*)brls::View::createFromXMLResource("tabs/outdated_titles_page.xml");

        // Get the recycler from the container
        brls::RecyclerFrame* recycler = (brls::RecyclerFrame*)container->getView("recycler");

        recycler->estimatedRowHeight = 100;
        recycler->registerCell("OutdatedTitleCell", []() { return OutdatedTitleCell::create(); });
        recycler->setDataSource(new OutdatedTitlesData(outdatedTitles), false);

        // Set the container as the content view
        this->setContentView(container);
    }

    // Set title and register B button AFTER setting content
    this->setTitle("menu/tools_tab/outdated_titles"_i18n);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}

void OutdatedTitlesPage::loadVersionsData()
{
    try {
        brls::Logger::info("Fetching versions data from {}", LOOKUP_TABLE_URL);
        versionsData = net::downloadRequest(LOOKUP_TABLE_URL);

        if (!versionsData.empty()) {
            brls::Logger::info("Successfully loaded versions database");
        } else {
            brls::Logger::error("Failed to fetch versions data");
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Exception loading versions data: {}", e.what());
    }
}

void OutdatedTitlesPage::checkForOutdatedTitles()
{
    if (versionsData.empty()) {
        return;
    }

    // Get all installed games
    auto installedGames = utils::getInstalledGames();

    brls::Logger::info("Checking {} installed games for updates", installedGames.size());

    for (const auto& [gameName, titleId] : installedGames) {
        brls::Logger::debug("Checking game: {} ({})", gameName, titleId);

        // Check if this title exists in the versions database
        if (!versionsData.contains(titleId)) {
            brls::Logger::debug("  -> Not found in versions database");
            continue;
        }

        const auto& titleData = versionsData[titleId];
        brls::Logger::debug("  -> Found in database");

        // Get current installed version
        uint64_t tid = std::stoull(titleId, nullptr, 16);
        u32 currentVersion = utils::getInstalledVersion(tid);

        // Get latest version from database
        u32 latestVersion = 0;
        std::string latestBuildId;

        if (titleData.contains("latest")) {
            latestVersion = titleData["latest"].get<u32>();

            // Get the build ID for the latest version
            std::string latestVerStr = std::to_string(latestVersion);
            if (titleData.contains(latestVerStr)) {
                latestBuildId = titleData[latestVerStr].get<std::string>();
            }

            brls::Logger::debug("{} ({}) - Current: v{}, Latest: v{}", gameName, titleId, currentVersion, latestVersion);
        }

        // Check for missing DLC
        int missingDlcCount = 0;
        std::vector<std::pair<std::string, std::string>> missingDlcInfo;

        if (titleData.contains("dlc") && titleData["dlc"].is_array()) {
            for (const auto& dlc : titleData["dlc"]) {
                if (dlc.contains("id") && dlc["id"].is_string()) {
                    std::string dlcId = dlc["id"].get<std::string>();
                    std::string dlcName = dlc.contains("name") ? dlc["name"].get<std::string>() : "Unknown DLC";

                    try {
                        uint64_t dlcTid = std::stoull(dlcId, nullptr, 16);
                        if (!utils::isDlcInstalled(dlcTid)) {
                            missingDlcCount++;
                            missingDlcInfo.push_back({dlcName, dlcId});
                        }
                    } catch (...) {
                        brls::Logger::warning("Invalid DLC ID: {}", dlcId);
                    }
                }
            }
        }

        // Add to list if there's an update OR missing DLC
        bool hasUpdate = (latestVersion > 0 && currentVersion < latestVersion);
        if (hasUpdate || missingDlcCount > 0) {
            OutdatedTitle outTitle;
            outTitle.titleId = titleId;
            outTitle.name = gameName;
            outTitle.currentVersion = currentVersion;
            outTitle.latestVersion = latestVersion;
            outTitle.latestBuildId = latestBuildId;
            outTitle.missingDlcCount = missingDlcCount;
            outTitle.missingDlcInfo = missingDlcInfo;

            outdatedTitles.push_back(outTitle);
        }
    }

    brls::Logger::info("Found {} outdated titles", outdatedTitles.size());
}
