#include "views/payload_page.hpp"
#include "utils/payload.hpp"
#include "utils/constants.hpp"
#include <borealis.hpp>
#include <switch.h>
#include <filesystem>

using namespace brls::literals;

PayloadPage::PayloadPage() {
    this->setTitle("menu/payload/title"_i18n);

    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::COLUMN);
    contentBox->setWidth(brls::View::AUTO);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(20, 20, 20, 20);

    // Add description
    auto* description = new brls::Label();
    description->setText("menu/payload/desc"_i18n);
    description->setFontSize(16);
    description->setMarginBottom(20);
    contentBox->addView(description);

    // Fetch all payload files
    auto payloads = Payload::fetchPayloads();

    if (payloads.empty()) {
        auto* noPayloadsLabel = new brls::Label();
        noPayloadsLabel->setText("menu/payload/no_payloads_found"_i18n);
        noPayloadsLabel->setFontSize(16);
        contentBox->addView(noPayloadsLabel);
    } else {
        // Add header
        auto* header = new brls::Header();
        header->setTitle(std::to_string(payloads.size()) + "menu/payload/payload_number"_i18n);
        contentBox->addView(header);

        // Add each payload
        for (const auto& payloadPath : payloads) {
            std::string filename = std::filesystem::path(payloadPath).filename().string();

            auto* item = new brls::DetailCell();
            item->setText(filename);
            item->setDetailText(payloadPath);

            // Click to inject and reboot
            item->registerClickAction([payloadPath, filename](brls::View* view) {
                auto* dialog = new brls::Dialog("menu/payload/reboot_payload_confirm"_i18n + filename);

                dialog->addButton("menu/payload/reboot"_i18n, [payloadPath]() {
                    brls::Logger::info("Rebooting to payload: {}", payloadPath);
                    int result = Payload::rebootToPayload(payloadPath);

                    if (result != 0) {
                        auto* errorDialog = new brls::Dialog("menu/payload/failed_to_inject"_i18n);
                        errorDialog->addButton("hints/ok"_i18n, []() {});
                        errorDialog->open();
                    }
                    // If successful, system will reboot and this won't be reached
                });

                dialog->addButton("hints/cancel"_i18n, []() {});
                dialog->open();

                return true;
            });

            // X button to copy to reboot_payload.bin
            item->registerAction("menu/payload/set_as_reboot_payload"_i18n, brls::ControllerButton::BUTTON_X, [payloadPath](brls::View* view) {
                std::string dest = REBOOT_PAYLOAD_PATH;

                // Create parent directories if needed
                std::filesystem::create_directories(std::filesystem::path(dest).parent_path());

                // Copy file using C FILE API
                FILE* src = fopen(payloadPath.c_str(), "rb");
                if (!src) {
                    auto* dialog = new brls::Dialog("menu/payload/failed_open_source"_i18n);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open();
                    return true;
                }

                FILE* dst = fopen(dest.c_str(), "wb");
                if (!dst) {
                    fclose(src);
                    auto* dialog = new brls::Dialog("menu/payload/failed_create_file"_i18n);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open();
                    return true;
                }

                // Copy in chunks
                char buffer[8192];
                size_t bytes;
                while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                    fwrite(buffer, 1, bytes, dst);
                }

                fclose(src);
                fclose(dst);

                auto* dialog = new brls::Dialog("Successfully copied to:\n" + dest);
                dialog->addButton("hints/ok"_i18n, []() {});
                dialog->open();

                return true;
            });

            // Y button to copy to update.bin
            item->registerAction("menu/payload/set_as_update"_i18n, brls::ControllerButton::BUTTON_Y, [payloadPath](brls::View* view) {
                std::string dest = UPDATE_BIN_PATH;

                // Create parent directories if needed
                std::filesystem::create_directories(std::filesystem::path(dest).parent_path());

                // Copy file using C FILE API
                FILE* src = fopen(payloadPath.c_str(), "rb");
                if (!src) {
                    auto* dialog = new brls::Dialog("menu/payload/failed_open_source"_i18n);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open();
                    return true;
                }

                FILE* dst = fopen(dest.c_str(), "wb");
                if (!dst) {
                    fclose(src);
                    auto* dialog = new brls::Dialog("menu/payload/failed_create_file"_i18n);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open();
                    return true;
                }

                // Copy in chunks
                char buffer[8192];
                size_t bytes;
                while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                    fwrite(buffer, 1, bytes, dst);
                }

                fclose(src);
                fclose(dst);

                auto* dialog = new brls::Dialog("menu/payload/copy_success"_i18n + dest);
                dialog->addButton("hints/ok"_i18n, []() {});
                dialog->open();

                return true;
            });

            contentBox->addView(item);
        }
    }

    // Add reboot option
    auto* rebootHeader = new brls::Header();
    rebootHeader->setTitle("menu/payload/system_options"_i18n);
    contentBox->addView(rebootHeader);

    auto* rebootItem = new brls::RadioCell();
    rebootItem->title->setText("menu/payload/reboot"_i18n);
    rebootItem->registerClickAction([](brls::View* view) {
        auto* dialog = new brls::Dialog("menu/payload/reboot_confirm"_i18n);
        dialog->addButton("menu/payload/reboot"_i18n, []() {
            spsmInitialize();
            spsmShutdown(true);
        });
        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->open();
        return true;
    });
    contentBox->addView(rebootItem);

    auto* shutdownItem = new brls::RadioCell();
    shutdownItem->title->setText("menu/payload/power_off"_i18n);
    shutdownItem->registerClickAction([](brls::View* view) {
        auto* dialog = new brls::Dialog("menu/payload/power_off_confirm"_i18n);
        dialog->addButton("menu/payload/power_off"_i18n, []() {
            spsmInitialize();
            spsmShutdown(false);
        });
        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->open();
        return true;
    });
    contentBox->addView(shutdownItem);

    scrollFrame->setContentView(contentBox);
    this->setContentView(scrollFrame);

    this->setTitle("menu/payload/title"_i18n);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}
