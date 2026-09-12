// SPDX-License-Identifier: GPL-2.0-or-later
#include "help_window.h"
#include "application_settings_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"
#include "page_layout_dialog.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T* child(QObject& object, const char* name) {
  auto* result = object.findChild<T*>(QString::fromLatin1(name));
  require(result, name); return result;
}
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);
  try {
    jwpqt::qt::MainWindow window;
    window.show();
    auto* editor = window.active_editor();
    editor->insertPlainText(QStringLiteral("Private document"));
    QCoreApplication::processEvents();
    const QString original = jwpqt::qt::document_plain_text(*editor->document());
    const int undo = editor->document()->availableUndoSteps();
    child<QAction>(window, "helpContentsAction")->trigger();
    auto* help = dynamic_cast<jwpqt::qt::HelpWindow*>(child<QDialog>(window, "helpWindow"));
    require(help, "Wrong help window type");
    auto* browser = child<QTextBrowser>(*help, "helpText");
    auto* list = child<QListWidget>(*help, "helpTopics");
    auto* search = child<QLineEdit>(*help, "helpSearch");
    QFile manifest(QStringLiteral(":/jwpqt/help/manifest.json"));
    require(manifest.open(QIODevice::ReadOnly), "Handbook manifest is unavailable");
    const auto manifest_root = QJsonDocument::fromJson(manifest.readAll()).object();
    require(manifest_root.value(QStringLiteral("legacy_source_sha256")).toString() ==
                QStringLiteral("436f07a67fa27dcdedd67f95c81d9ce5e169aba4d8e2594e80cf92dcb7c4ba3d"),
            "Handbook legacy source identity changed");
    QByteArray legacy_contract;
    for (const auto& entry : manifest_root.value(QStringLiteral("topics")).toArray()) {
      const auto topic = entry.toObject();
      legacy_contract += topic.value(QStringLiteral("legacy_id")).toString().toUtf8();
      legacy_contract += '\t';
      legacy_contract += topic.value(QStringLiteral("legacy_title")).toString().toUtf8();
      legacy_contract += '\n';
    }
    require(QString::fromLatin1(QCryptographicHash::hash(legacy_contract, QCryptographicHash::Sha256).toHex()) ==
                manifest_root.value(QStringLiteral("legacy_contract_sha256")).toString(),
            "Handbook legacy topic contract changed");
    QSet<QString> topic_ids;
    int chapter_count = 0;
    int topic_count = 0;
    for (int i = 0; i < list->count(); ++i) {
      auto* item = list->item(i);
      const QString file = item->data(Qt::UserRole).toString();
      if (file.isEmpty()) {
        ++chapter_count;
        require(item->flags() == Qt::NoItemFlags && item->font().bold(),
                "Handbook chapter heading is interactive or unstyled");
        continue;
      }
      const QString id = item->data(Qt::UserRole + 1).toString();
      require(id.startsWith(QStringLiteral("IDH_")) && !topic_ids.contains(id),
              "Handbook topic ID is missing or duplicated");
      require(!item->text().contains(QLatin1Char('\n')), "Handbook title contains article text");
      topic_ids.insert(id);
      ++topic_count;
      help->open_topic(file);
      require(browser->toPlainText().size() > 250, "A shipped topic or license is empty");
      const QUrl source = browser->source();
      QStringList links;
      for (auto block = browser->document()->begin(); block.isValid(); block = block.next())
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
          const QString link = fragment.fragment().charFormat().anchorHref();
          if (!link.isEmpty() && !links.contains(link)) links.push_back(link);
        }
      for (const auto& link : links) {
        browser->anchorClicked(QUrl(link));
        require(browser->source().scheme() == QStringLiteral("qrc") &&
                    browser->source().path().contains(QStringLiteral("/topics/")) &&
                    !browser->toPlainText().isEmpty(),
                "A shipped handbook link is broken");
        help->open_topic(file);
      }
    }
    require(chapter_count == 12 && topic_count == 125 && topic_ids.size() == 125,
            "The complete legacy handbook topic inventory is missing");
    require(topic_ids.contains(QStringLiteral("IDH_OPTIONS_FILECLIPBOARD")),
            "The repaired legacy duplicate topic ID is missing");
    help->open_topic(QStringLiteral("_cpright.txt"));
    require(browser->toPlainText().contains(QStringLiteral("Kyoto University")), "Original notices missing");
    help->open_topic(QStringLiteral("gnugpl.txt"));
    require(browser->toPlainText().contains(QStringLiteral("WITHOUT"), Qt::CaseInsensitive),
            "GNU license text missing");
    child<QAction>(window, "aboutAction")->trigger();
    require(browser->source().path().endsWith(QStringLiteral("idh_intro_copyrights.md")) &&
                browser->toPlainText().contains(QStringLiteral("Glenn Rosenthal")),
            "About credits topic missing");
    bool about_qt_seen = false;
    QTimer::singleShot(0, &app, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      QString text;
      for (auto* label : dialog->findChildren<QLabel*>()) text += label->text();
      about_qt_seen = dialog->windowTitle().contains(QStringLiteral("Qt")) &&
                      text.contains(QStringLiteral("Qt"));
      dialog->accept();
    });
    child<QAction>(window, "aboutQtAction")->trigger();
    require(about_qt_seen, "About Qt action did not open the Qt information dialog");

    const QString resource_report = window.resource_report();
    const auto inspect_resources = [&](bool use_action, const char* failure) {
      bool seen = false;
      QTimer::singleShot(0, &app, [&] {
        auto* dialog = window.findChild<QMessageBox*>(
            QStringLiteral("resourceStatusDialog"), Qt::FindDirectChildrenOnly);
        if (!dialog) return;
        seen = dialog->textFormat() == Qt::PlainText &&
               dialog->text() == resource_report;
        dialog->accept();
      });
      if (use_action)
        child<QAction>(window, "resourceStatusAction")->trigger();
      else
        child<QToolButton>(window, "resourceStatus")->click();
      require(seen, failure);
    };
    inspect_resources(true, "Runtime Resources action showed the wrong report");
    inspect_resources(false, "Runtime resource button showed the wrong report");
    require(jwpqt::qt::document_plain_text(*editor->document()) == original &&
                editor->document()->availableUndoSteps() == undo,
            "Help or resource actions changed document content or undo");
    help->open_topic(QStringLiteral("IDH_INTRO_WHATISJWPCE"));
    browser->anchorClicked(QUrl(QStringLiteral("help:IDH_EDIT_SEARCH")));
    require(browser->source().path().endsWith(QStringLiteral("idh_edit_search.md")), "Topic-ID handbook link failed");
    child<QPushButton>(*help, "helpBack")->click();
    require(browser->source().path().endsWith(QStringLiteral("idh_intro_whatisjwpce.md")), "Back navigation failed");
    child<QPushButton>(*help, "helpForward")->click();
    const QUrl before = browser->source();
    for (const auto& url : {QStringLiteral("file:///etc/passwd"), QStringLiteral("https://example.com/"),
                           QStringLiteral("qrc:/jwpqt/help/../../outside"), QStringLiteral("gnugpl.txt?x=y")}) {
      browser->anchorClicked(QUrl(url));
      require(browser->source() == before, "Help opened a non-whitelisted resource");
    }
    require(browser->document()->resource(QTextDocument::ImageResource, QUrl("file:///etc/passwd")).toByteArray().isEmpty(),
            "Help loaded an external embedded resource");
    search->setText(QStringLiteral("IDH_PRINT_HEADSTRINGS"));
    int matching_topic = -1;
    for (int i = 0; i < list->count(); ++i) {
      if (list->item(i)->data(Qt::UserRole + 1).toString() == QStringLiteral("IDH_PRINT_HEADSTRINGS")) {
        matching_topic = i;
        break;
      }
    }
    require(matching_topic >= 0, "Full-text handbook search failed");
    list->setCurrentRow(matching_topic);
    search->returnPressed();
    require(browser->source().path().endsWith(QStringLiteral("idh_print_headstrings.md")), "Search activation failed");
    search->setText(QStringLiteral("no_such_topic_8297"));
    require(list->count() == 0, "No-match search showed unrelated topics");
    search->clear();
    {
      QDialog tool(&window);
      tool.setObjectName(QStringLiteral("edictLookupDialog"));
      QLineEdit query(&tool);
      QKeyEvent shortcut(QEvent::ShortcutOverride, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&query, &shortcut);
      require(shortcut.isAccepted(), "Context F1 did not own its shortcut");
      QKeyEvent key(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&query, &key);
      require(browser->source().path().endsWith(QStringLiteral("idh_dict_general.md")), "Dictionary context help failed");
    }
    for (const auto& context : {
             std::pair{QStringLiteral("kanjiReadingLookupDialog"), QStringLiteral("idh_kanji_readlookup.md")},
             std::pair{QStringLiteral("kanjiCountDialog"), QStringLiteral("idh_kanji_countkanji.md")},
             std::pair{QStringLiteral("jisTableDialog"), QStringLiteral("idh_kanji_jistable.md")},
             std::pair{QStringLiteral("edictRegistryDialog"), QStringLiteral("idh_dict_dictionaries.md")},
             std::pair{QStringLiteral("edictUserDictionaryDialog"), QStringLiteral("idh_dict_useredit.md")},
             std::pair{QStringLiteral("wnnUserDictionaryDialog"), QStringLiteral("idh_text_userkanji.md")}}) {
      QDialog tool(&window);
      tool.setObjectName(context.first);
      QKeyEvent key(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&tool, &key);
      require(browser->source().path().endsWith(context.second), "Exact dialog context help failed");
    }
    {
      jwpqt::qt::ApplicationSettingsDialog options(
          window.application_settings(), &window);
      options.show();
      QCoreApplication::processEvents();
      auto* options_tabs = child<QTabWidget>(options, "applicationSettingsTabs");
      const QStringList option_topics = {
          QStringLiteral("idh_options_general.md"), QStringLiteral("idh_options_font.md"),
          QStringLiteral("idh_dict_options.md"), QStringLiteral("idh_options_advanced.md"),
          QStringLiteral("idh_print_layout.md"), QStringLiteral("idh_print_options.md"),
          QStringLiteral("idh_kanji_lookup.md"), QStringLiteral("idh_options_display.md")};
      for (int index = 0; index < option_topics.size(); ++index) {
        options_tabs->setCurrentIndex(index);
        child<QPushButton>(options, "applicationSettingsHelp")->click();
        require(options.isVisible() && browser->source().path().endsWith(option_topics.at(index)),
                "Options Help did not follow the active settings page");
      }
      QKeyEvent options_help(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&options, &options_help);
      require(browser->source().path().endsWith(QStringLiteral("idh_options_display.md")),
              "Options F1 did not follow the active settings page");
      options.reject();
      require(jwpqt::qt::document_plain_text(*editor->document()) == original &&
                  editor->document()->availableUndoSteps() == undo,
              "Options Help changed document content or undo");
    }
    {
      const auto* document = window.current_jwp_document();
      require(document, "Page Layout Help requires a JWP document");
      jwpqt::qt::PageLayoutDialog layout(
          *document, jwpqt::core::kDefaultLegacyCodePage, &window);
      layout.show();
      QCoreApplication::processEvents();
      auto* layout_tabs = child<QTabWidget>(layout, "pageLayoutTabs");
      const QStringList layout_topics = {
          QStringLiteral("idh_print_margins.md"), QStringLiteral("idh_print_headers.md"),
          QStringLiteral("idh_print_summary.md")};
      for (int index = 0; index < layout_topics.size(); ++index) {
        layout_tabs->setCurrentIndex(index);
        child<QPushButton>(layout, "pageLayoutHelp")->click();
        require(layout.isVisible() && browser->source().path().endsWith(layout_topics.at(index)),
                "Page Layout Help did not follow the active page");
      }
      QKeyEvent layout_help(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&layout, &layout_help);
      require(browser->source().path().endsWith(QStringLiteral("idh_print_summary.md")),
              "Page Layout F1 did not follow the active page");
      layout.reject();
      require(jwpqt::qt::document_plain_text(*editor->document()) == original &&
                  editor->document()->availableUndoSteps() == undo,
              "Page Layout Help changed document content or undo");
    }
    help->close();
    child<QAction>(window, "helpContentsAction")->trigger();
    require(child<QDialog>(window, "helpWindow") == help, "Help did not reuse its owning window");
    require(jwpqt::qt::document_plain_text(*editor->document()) == original &&
            editor->document()->availableUndoSteps() == undo, "Help changed document content or undo");
    help->grab().save(QStringLiteral("native-handbook.png"));
    const auto original_palette = app.palette();
    auto dark = original_palette;
    dark.setColor(QPalette::Window, QColor(35, 35, 35));
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::Button, QColor(45, 45, 45));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
      dark.setColor(role, QColor(235, 235, 235));
    dark.setColor(QPalette::Link, QColor(130, 185, 255));
    dark.setColor(QPalette::PlaceholderText, QColor(165, 165, 165));
    const auto theme_source = browser->source();
    const auto theme_text = browser->toPlainText();
    app.setPalette(dark);
    QCoreApplication::processEvents();
    for (auto block = browser->document()->begin(); block.isValid(); block = block.next())
      for (auto it = block.begin(); !it.atEnd(); ++it)
        if (!it.fragment().charFormat().anchorHref().isEmpty())
          require(it.fragment().charFormat().foreground().color() == dark.color(QPalette::Link),
                  "Markdown link did not follow the dark palette");
    require(browser->source() == theme_source && browser->toPlainText() == theme_text,
            "Theme switch changed handbook navigation or text");
    help->grab().save(QStringLiteral("native-handbook-dark.png"));
    app.setPalette(original_palette);
    {
      jwpqt::qt::MainWindow other;
      QDialog files(&other);
      files.setObjectName(QStringLiteral("filesDialog"));
      QLineEdit field(&files);
      const QUrl original_topic = browser->source();
      QKeyEvent key(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
      QApplication::sendEvent(&field, &key);
      auto* other_browser = child<QTextBrowser>(other, "helpText");
      require(other_browser->source().path().endsWith(QStringLiteral("idh_file_types.md")) && browser->source() == original_topic,
              "Context help crossed workspace ownership or missed file commands");
    }
    QPointer<jwpqt::qt::MainWindow> disposable = new jwpqt::qt::MainWindow;
    auto* owned_help = dynamic_cast<jwpqt::qt::HelpWindow*>(child<QDialog>(*disposable, "helpWindow"));
    auto* owned_browser = child<QTextBrowser>(*owned_help, "helpText");
    QObject::connect(owned_browser, &QTextBrowser::sourceChanged, &app, [&] { delete disposable.data(); });
    owned_help->open_topic();
    require(!disposable, "Help navigation did not tolerate owner deletion");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QPointer<jwpqt::qt::MainWindow> resource_owner = new jwpqt::qt::MainWindow;
    bool owned_resource_seen = false;
    QTimer::singleShot(0, &app, [&] {
      owned_resource_seen = resource_owner &&
          resource_owner->findChild<QMessageBox*>(
              QStringLiteral("resourceStatusDialog"), Qt::FindDirectChildrenOnly);
      delete resource_owner.data();
    });
    child<QAction>(*resource_owner, "resourceStatusAction")->trigger();
    require(owned_resource_seen && !resource_owner,
            "Runtime Resources did not tolerate owner deletion");
    std::cout << "All 125 offline topics, links, licenses, search, exact context, isolation and lifetime passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
