// SPDX-License-Identifier: GPL-2.0-or-later
#include "application_settings.h"
#include "application_settings_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"
#include "page_layout_dialog.h"
#include "file_io.h"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabWidget>
#include <QDir>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace jwpqt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F call) { bool failed = false; try { call(); } catch (const std::exception&) { failed = true; } require(failed, "Invalid page accepted"); }

int main(int argc, char** argv) {
  QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
  try {
    core::JwpPageDefaults page;
    auto bytes = core::encode_page_defaults(page);
    require(bytes.size() == 20 && bytes[0] == 0 && bytes[2] == 0x80 && bytes[3] == 0x3f, "Not little-endian float layout");
    page.margins = {0.125F, 2.5F, 0.75F, 1.25F}; page.landscape = true; page.vertical = true; page.padding = {0xfe, 0x93};
    bytes = core::encode_page_defaults(page);
    require(core::encode_page_defaults(core::decode_page_defaults(bytes)) == bytes, "Page/padding changed");
    for (std::size_t size = 0; size < 20; ++size) rejects([&] { core::decode_page_defaults({bytes.begin(), bytes.begin() + size}); });
    for (auto index : {16, 17}) { auto bad = bytes; bad[index] = 2; rejects([&] { core::decode_page_defaults(bad); }); }
    for (float bad : {-1.F, 11.F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
      auto invalid = page; invalid.margins[0] = bad; rejects([&] { core::encode_page_defaults(invalid); });
    }
    auto settings = qt::ApplicationSettings{}; settings.default_page = page;
    settings.source = "Unknown=opaque\n";
    auto text = qt::write_application_settings(settings);
    require(text.find("Printing_DefaultLayout = ") != std::string::npos && text.find("Unknown=opaque") != std::string::npos, "Missing canonical defaults");
    require(core::encode_page_defaults(qt::read_application_settings(text).default_page) == bytes, "Settings roundtrip changed page");
    rejects([&] { qt::read_application_settings("page=00\n" + text); });
    auto metric_settings = settings;
    metric_settings.metric_units = true;
    qt::ApplicationSettingsDialog metric_options(metric_settings);
    auto* metric_check = metric_options.findChild<QCheckBox*>("settingsMetricUnits");
    auto* metric_margin = metric_options.findChild<QDoubleSpinBox*>("settingsDefaultMargin0");
    require(metric_check && metric_margin && metric_check->isChecked() &&
                metric_margin->suffix() == " cm" &&
                std::abs(metric_margin->value() - page.margins[0] * 2.54) < 0.000001,
            "Options did not display stored page defaults in centimeters");
    metric_check->setChecked(false);
    require(metric_margin->suffix() == " in" &&
                std::abs(metric_margin->value() - page.margins[0]) < 0.000001,
            "Options did not convert centimeters back to inches");
    metric_check->setChecked(true);
    metric_margin->setValue(5.08);
    metric_options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(metric_options.settings().metric_units &&
                std::abs(metric_options.settings().default_page.margins[0] - 2.0F) < 0.000001,
            "Options did not convert a changed centimeter default back to inches");
    auto precise_settings = metric_settings;
    precise_settings.default_page.margins[0] = 1.0e-20F;
    const auto precise_bytes = core::encode_page_defaults(precise_settings.default_page);
    qt::ApplicationSettingsDialog precise_options(precise_settings);
    auto* precise_units = precise_options.findChild<QCheckBox*>("settingsMetricUnits");
    precise_units->setChecked(false);
    precise_units->setChecked(true);
    precise_options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(core::encode_page_defaults(precise_options.settings().default_page) ==
                precise_bytes,
            "Toggling display units rounded an untouched page default");
    qt::ApplicationSettingsDialog cancel(settings);
    cancel.findChild<QDoubleSpinBox*>("settingsDefaultMargin0")->setValue(4);
    cancel.reject(); require(cancel.settings().default_page.margins[0] == page.margins[0], "Cancelled defaults changed");
    qt::ApplicationSettingsDialog options(settings);
    options.findChild<QDoubleSpinBox*>("settingsDefaultMargin0")->setValue(1.5);
    options.findChild<QCheckBox*>("settingsDefaultVertical")->setChecked(false);
    options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(options.result() == QDialog::Accepted && options.settings().default_page.margins[0] == 1.5F && !options.settings().default_page.vertical, "Options did not apply");
    QTemporaryDir temporary; require(temporary.isValid(), "No temporary directory");
    const auto cfg = temporary.filePath("settings.cfg");
    qt::write_application_settings_file(cfg, settings);
    qt::MainWindow window;
    require(window.load_application_settings(cfg), "Load failed");
    require(window.current_jwp_document()->margins == page.margins && window.current_jwp_document()->vertical && !window.active_editor()->document()->isModified(), "Initial document ignored defaults");
    window.active_editor()->insertPlainText("keep");
    auto altered = settings; altered.default_page.margins.fill(2);
    require(window.apply_application_settings(altered), "Changed defaults rejected");
    require(window.current_jwp_document()->margins == page.margins && window.active_editor()->document()->toRawText() == "keep", "Preferences changed existing document");
    window.findChild<QAction*>("newDocumentAction")->trigger();
    require(window.current_jwp_document()->margins == altered.default_page.margins && !window.active_editor()->document()->isModified(), "New document ignored defaults");
    window.active_editor()->insertPlainText("body");
    const auto previous = *window.current_jwp_document();
    const auto stage_layout = [&](bool accept, bool new_default) {
      QTimer::singleShot(0, &window, [&window, accept, new_default] {
        auto* dialog = dynamic_cast<qt::PageLayoutDialog*>(window.findChild<QDialog*>("pageLayoutDialog"));
        require(dialog, "Page dialog missing");
        dialog->findChild<QDoubleSpinBox*>("leftMargin")->setValue(3);
        if (new_default) dialog->findChild<QPushButton*>("layoutSetDefault")->click();
        dialog->findChild<QDialogButtonBox*>()->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
      });
      window.findChild<QAction*>("pageLayoutAction")->trigger();
    };
    stage_layout(false, true);
    require(*window.current_jwp_document() == previous && window.application_settings().default_page.margins[0] == 2, "Cancelled page staged changes leaked");
    stage_layout(true, true);
    require(window.current_jwp_document()->margins[0] == 3 && window.application_settings().default_page.margins[0] == 3, "Page defaults not accepted");
    window.findChild<QAction*>("undoAction")->trigger();
    require(*window.current_jwp_document() == previous && window.application_settings().default_page.margins[0] == 3, "Undo lost document or changed global defaults");
    qt::PageLayoutDialog from(previous, core::kDefaultLegacyCodePage, nullptr, &page);
    from.findChild<QPushButton*>("layoutFromDefault")->click(); require(from.apply_changes(), "Default transfer failed");
    require(from.document().margins == page.margins && from.document().paragraphs == previous.paragraphs, "Default transfer changed body");
    auto precise = previous; precise.margins[0] = 1.0e-20F;
    qt::PageLayoutDialog unchanged(precise, core::kDefaultLegacyCodePage);
    require(unchanged.apply_changes() && unchanged.document().margins == precise.margins, "Untouched margin was rounded");
    qt::PageLayoutDialog metric(previous, core::kDefaultLegacyCodePage,
                                nullptr, &page, true);
    auto* metric_page_margin = metric.findChild<QDoubleSpinBox*>("leftMargin");
    require(metric_page_margin && metric_page_margin->suffix() == " cm" &&
                std::abs(metric_page_margin->value() - previous.margins[0] * 2.54) < 0.000001,
            "Page Layout did not display document margins in centimeters");
    require(metric.apply_changes() && metric.document().margins == previous.margins,
            "Unchanged metric display changed stored document margins");
    qt::PageLayoutDialog precise_metric(precise, core::kDefaultLegacyCodePage,
                                        nullptr, nullptr, true);
    require(precise_metric.apply_changes() &&
                precise_metric.document().margins == precise.margins,
            "Unchanged metric display rounded a stored document margin");
    qt::PageLayoutDialog changed_metric(previous, core::kDefaultLegacyCodePage,
                                        nullptr, &page, true);
    changed_metric.findChild<QDoubleSpinBox*>("leftMargin")->setValue(5.08);
    changed_metric.findChild<QPushButton*>("layoutSetDefault")->click();
    require(changed_metric.apply_changes() &&
                std::abs(changed_metric.document().margins[0] - 2.0F) < 0.000001 &&
                changed_metric.default_page().has_value() &&
                std::abs(changed_metric.default_page()->margins[0] - 2.0F) < 0.000001,
            "Page Layout did not convert changed centimeter margins to inches");
    const auto file = temporary.filePath("body.txt");
    qt::write_text_file(file, {U"text", core::TextEncoding::kUtf8, false});
    require(window.open_path(file, core::TextEncoding::kUtf8, qt::OpenMode::kNonInteractive, true), "Text import failed");
    require(window.current_jwp_document()->margins[0] == 3, "Text import did not use defaults");
    const auto jwp = temporary.filePath("existing.jwp");
    qt::write_jwp_file(jwp, previous);
    require(window.open_jwp_path(jwp, core::kDefaultLegacyCodePage, qt::OpenMode::kNonInteractive, true) &&
        *window.current_jwp_document() == previous, "File layout was replaced with defaults");
    QTimer::singleShot(0, &window, [&] {
      auto* dialog = window.findChild<QDialog*>("pageLayoutDialog");
      dialog->findChild<QDoubleSpinBox*>("leftMargin")->setValue(4);
      dialog->findChild<QPushButton*>("layoutSetDefault")->click();
      window.active_editor()->insertPlainText("newer");
      dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    window.findChild<QAction*>("pageLayoutAction")->trigger();
    require(window.current_jwp_document()->margins == previous.margins && window.application_settings().default_page.margins[0] == 3,
        "Stale page dialog replaced newer document/defaults");
    QPointer<qt::MainWindow> doomed = new qt::MainWindow;
    QTimer::singleShot(0, &app, [&] { delete doomed.data(); });
    doomed->findChild<QAction*>("pageLayoutAction")->trigger();
    require(!doomed, "Page owner deletion fixture failed");
    app.sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qt::write_application_settings_file(cfg, metric_settings);
    qt::MainWindow metric_restored;
    require(metric_restored.load_application_settings(cfg) &&
                metric_restored.application_settings().metric_units,
            "Settings restart lost the measurement unit");
    QTimer::singleShot(0, &metric_restored, [&] {
      auto* dialog = metric_restored.findChild<QDialog*>("pageLayoutDialog");
      require(dialog &&
                  dialog->findChild<QDoubleSpinBox*>("leftMargin")->suffix() == " cm",
              "Main window did not apply the configured unit to Page Layout");
      dialog->reject();
    });
    metric_restored.findChild<QAction*>("pageLayoutAction")->trigger();
    qt::MainWindow project;
    require(project.apply_application_settings(metric_settings), "Project settings failed");
    const auto jpr = temporary.filePath("page.jpr"); require(project.save_project_path(jpr, false), "Project save failed");
    qt::MainWindow restored; qt::ProjectOpenOptions consent; consent.allow_unapplied_settings = true;
    require(restored.open_project_path(jpr, consent) &&
                restored.application_settings().metric_units &&
                core::encode_page_defaults(restored.application_settings().default_page) == bytes,
            "Project lost page defaults or their display unit");
    require(restored.current_jwp_document()->margins == page.margins && restored.current_jwp_document()->vertical,
        "Empty project did not create a document from restored defaults");
    qt::MainWindow unicode;
    require(unicode.apply_application_settings(settings), "Unicode defaults rejected");
    unicode.findChild<QAction*>("newTextDocumentAction")->trigger();
    unicode.active_editor()->insertPlainText("plain");
    const auto converted = temporary.filePath("unicode.jwp");
    require(unicode.save_as_path(converted, std::nullopt), "Unicode JWP save failed");
    require(qt::read_jwp_file(converted).margins == page.margins, "Unicode JWP save ignored defaults");
    require(unicode.apply_application_settings(altered) && unicode.save_as_path(converted, std::nullopt), "Repeat Unicode JWP save failed");
    require(qt::read_jwp_file(converted).margins == page.margins, "New defaults changed an existing Unicode JWP file");
    options.show();
    auto* tabs = options.findChild<QTabWidget*>();
    for (int i = 0; i < tabs->count(); ++i) if (tabs->tabText(i) == "Default Page") tabs->setCurrentIndex(i);
    app.processEvents(); require(options.grab().save(QDir::current().filePath("default-page-options.png")), "Capture failed");
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
  return 0;
}
