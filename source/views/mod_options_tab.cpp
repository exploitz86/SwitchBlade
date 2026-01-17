#include "views/mod_options_tab.hpp"
#include "views/mod_batch_operation_view.hpp"
#include "utils/config.hpp"
#include "utils/utils.hpp"

#include <borealis.hpp>
#include <filesystem>

using namespace brls::literals;

ModOptionsTab::ModOptionsTab(const std::string& gameName,
                             const std::string& gamePath,
                             const std::string& titleId,
                             std::function<void()> onModsChanged)
    : gameName(gameName), gamePath(gamePath), titleId(titleId),
      onModsChangedCallback(onModsChanged) {

    brls::Logger::debug("ModOptionsTab constructor START");
    this->inflateFromXMLRes("xml/tabs/mod_options_tab.xml");
    brls::Logger::debug("ModOptionsTab XML inflated");

    populateOptions();

    brls::Logger::debug("ModOptionsTab constructor END");

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif
}

void ModOptionsTab::populateOptions() {
    optionsList->clearViews();

    // Recheck all mods
    auto* recheckItem = new brls::DetailCell();
    recheckItem->setText("menu/mods/recheck_all_button"_i18n);
    recheckItem->registerAction("Select", brls::ControllerButton::BUTTON_A, [this](brls::View* view) {
        recheckAllMods();
        return true;
    });
    optionsList->addView(recheckItem);

    // Description for Recheck all mods
    auto* recheckDesc = new brls::Label();
    recheckDesc->setText("menu/mods/recheck_all_desc"_i18n);
    recheckDesc->setFontSize(16);
    recheckDesc->setTextColor(nvgRGB(150, 150, 150));
    recheckDesc->setMarginLeft(40);
    recheckDesc->setMarginRight(40);
    recheckDesc->setMarginTop(20);
    recheckDesc->setMarginBottom(20);
    optionsList->addView(recheckDesc);

    // Disable all mods
    auto* disableItem = new brls::DetailCell();
    disableItem->setText("menu/mods/disable_all"_i18n);
    disableItem->registerAction("menu/mods/select"_i18n, brls::ControllerButton::BUTTON_A, [this](brls::View* view) {
        disableAllMods();
        return true;
    });
    optionsList->addView(disableItem);

    // Description for Disable all mods
    auto* disableDesc = new brls::Label();
    disableDesc->setText("menu/mods/disable_all_desc"_i18n
    );
    disableDesc->setFontSize(16);
    disableDesc->setTextColor(nvgRGB(150, 150, 150));
    disableDesc->setMarginLeft(40);
    disableDesc->setMarginRight(40);
    disableDesc->setMarginTop(20);
    disableDesc->setMarginBottom(20);
    optionsList->addView(disableDesc);

    // Associated TitleID - custom box with image
    auto* titleIdBox = new brls::Box();
    titleIdBox->setAxis(brls::Axis::ROW);
    titleIdBox->setAlignItems(brls::AlignItems::FLEX_START);
    titleIdBox->setHeight(100);
    titleIdBox->setPaddingTop(12.5f);
    titleIdBox->setPaddingBottom(12.5f);

    // Image
    auto* gameImage = new brls::Image();
    gameImage->setWidth(100);
    gameImage->setHeight(100);

    uint8_t* icon = utils::getIconFromTitleId(titleId);
    if (icon != nullptr) {
        gameImage->setImageFromMem(icon, 0x20000);
    }

    titleIdBox->addView(gameImage);

    // Text box
    auto* textBox = new brls::Box();
    textBox->setAxis(brls::Axis::COLUMN);
    textBox->setAlignItems(brls::AlignItems::FLEX_START);
    textBox->setMarginLeft(20);
    textBox->setGrow(1.0f);

    auto* titleLabel = new brls::Label();
    titleLabel->setText("menu/mods/associated_tid"_i18n);
    titleLabel->setFontSize(24);
    titleLabel->setMarginTop(10);
    textBox->addView(titleLabel);

    auto* valueLabel = new brls::Label();
    valueLabel->setText(!titleId.empty() ? titleId : "menu/mods/not_found"_i18n);
    valueLabel->setFontSize(16);
    valueLabel->setMarginTop(10);
    textBox->addView(valueLabel);

    titleIdBox->addView(textBox);

    optionsList->addView(titleIdBox);
}

void ModOptionsTab::recheckAllMods() {
    brls::Dialog* dialog = new brls::Dialog("menu/mods/recheck_all_confirm"_i18n);

    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("menu/mods/recheck"_i18n, [this]() {
        this->present(new ModBatchOperationView(
            ModBatchOperationType::RECHECK_ALL,
            gamePath,
            "sdmc:/atmosphere",
            [this]() {
                // Notify parent to refresh
                if (onModsChangedCallback) {
                    onModsChangedCallback();
                }
            }
        ));
    });

    dialog->open();
}

void ModOptionsTab::disableAllMods() {
    brls::Dialog* dialog = new brls::Dialog("menu/mods/disable_all_confirm"_i18n);

    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("menu/mods/disable"_i18n, [this]() {
        this->present(new ModBatchOperationView(
            ModBatchOperationType::REMOVE_ALL,
            gamePath,
            "sdmc:/atmosphere",
            [this]() {
                // Notify parent to refresh
                if (onModsChangedCallback) {
                    onModsChangedCallback();
                }
            }
        ));
    });

    dialog->open();
}

void ModOptionsTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    brls::sync([this]() {
        this->getAppletFrame()->setTitle(this->gameName);
    });
}

brls::View* ModOptionsTab::create() {
    // This won't be used since we need to pass parameters
    return nullptr;
}
