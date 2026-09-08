// SPDX-License-Identifier: GPL-2.0-or-later
#include "help_window.h"
#include "main_window.h"
#include "jwp_editor.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextFragment>
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
    const QString original = jwpqt::qt::document_plain_text(*editor->document());
    const int undo = editor->document()->availableUndoSteps();
    child<QAction>(window, "helpContentsAction")->trigger();
    auto* help = dynamic_cast<jwpqt::qt::HelpWindow*>(child<QDialog>(window, "helpWindow"));
    require(help, "Wrong help window type");
    auto* browser = child<QTextBrowser>(*help, "helpText");
    auto* list = child<QListWidget>(*help, "helpTopics");
    auto* search = child<QLineEdit>(*help, "helpSearch");
    require(list->count() == 11 && browser->toPlainText().contains(QStringLiteral("jwpqt Handbook")),
            "Bundled contents or topic index missing");
    for (int i = 0; i < list->count(); ++i) {
      help->open_topic(list->item(i)->data(Qt::UserRole).toString());
      require(browser->toPlainText().size() > 250, "A shipped topic or license is empty");
      const QUrl source = browser->source();
      QStringList links;
      for (auto block = browser->document()->begin(); block.isValid(); block = block.next())
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
          const QString link = fragment.fragment().charFormat().anchorHref();
          if (!link.isEmpty() && !links.contains(link)) links.push_back(link);
        }
      for (const auto& link : links) {
        const QUrl target = source.resolved(QUrl(link));
        browser->anchorClicked(target);
        require(browser->source() == target && !browser->toPlainText().isEmpty(), "A shipped handbook link is broken");
      }
    }
    require(browser->toPlainText().contains(QStringLiteral("Kyoto University")), "Original notices missing");
    child<QAction>(window, "aboutAction")->trigger();
    require(browser->toPlainText().contains(QStringLiteral("WITHOUT"), Qt::CaseInsensitive) &&
            browser->toPlainText().contains(QStringLiteral("Glenn Rosenthal")), "About credits/warranty missing");
    help->open_topic(QStringLiteral("start.md"));
    browser->anchorClicked(QUrl(QStringLiteral("editing.md")));
    require(browser->source().path().endsWith(QStringLiteral("editing.md")), "Relative handbook link failed");
    child<QPushButton>(*help, "helpBack")->click();
    require(browser->source().path().endsWith(QStringLiteral("start.md")), "Back navigation failed");
    child<QPushButton>(*help, "helpForward")->click();
    const QUrl before = browser->source();
    for (const auto& url : {QStringLiteral("file:///etc/passwd"), QStringLiteral("https://example.com/"),
                           QStringLiteral("qrc:/jwpqt/help/../../outside"), QStringLiteral("gnugpl.txt?x=y")}) {
      browser->anchorClicked(QUrl(url));
      require(browser->source() == before, "Help opened a non-whitelisted resource");
    }
    require(browser->document()->resource(QTextDocument::ImageResource, QUrl("file:///etc/passwd")).toByteArray().isEmpty(),
            "Help loaded an external embedded resource");
    search->setText(QStringLiteral("first-page suppression"));
    require(list->count() == 1, "Full-text handbook search failed");
    search->returnPressed();
    require(browser->source().path().endsWith(QStringLiteral("printing.md")), "Search activation failed");
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
      require(browser->source().path().endsWith(QStringLiteral("dictionary.md")), "Dictionary context help failed");
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
      require(other_browser->source().path().endsWith(QStringLiteral("files.md")) && browser->source() == original_topic,
              "Context help crossed workspace ownership or missed file commands");
    }
    QPointer<jwpqt::qt::MainWindow> disposable = new jwpqt::qt::MainWindow;
    auto* owned_help = dynamic_cast<jwpqt::qt::HelpWindow*>(child<QDialog>(*disposable, "helpWindow"));
    auto* owned_browser = child<QTextBrowser>(*owned_help, "helpText");
    QObject::connect(owned_browser, &QTextBrowser::sourceChanged, &app, [&] { delete disposable.data(); });
    owned_help->open_topic();
    require(!disposable, "Help navigation did not tolerate owner deletion");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    std::cout << "Offline topics, licenses, search, navigation, context, isolation and lifetime passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
