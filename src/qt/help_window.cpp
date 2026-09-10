// SPDX-License-Identifier: GPL-2.0-or-later
#include "help_window.h"
#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QResource>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextFragment>
#include <QSignalBlocker>
#include <QVBoxLayout>

static void initialize_handbook() { Q_INIT_RESOURCE(handbook_assets); Q_INIT_RESOURCE(notice_assets); }

namespace jwpqt::qt {
namespace {
struct Topic { const char* file; const char* title; };
constexpr Topic topics[] = {
    {"start.md", "Getting Started"}, {"files.md", "Documents and Projects"},
    {"editing.md", "Japanese Input, Find and Replace"}, {"dictionary.md", "Dictionary Search"},
    {"kanji.md", "Character Information and Kanji Lookup"}, {"printing.md", "Printing and PDF"},
    {"settings.md", "Settings and Histories"}, {"installation.md", "Installation and Runtime Data"},
    {"about.md", "About, Credits and Licenses"}, {"gnugpl.txt", "GNU General Public License"},
    {"_cpright.txt", "Original Copyright and Data Notices"}};
bool allowed(const QUrl& url) {
  if (url.scheme() != QStringLiteral("qrc") || !url.host().isEmpty() || url.hasQuery()) return false;
  for (const auto& topic : topics)
    if (url.path() == QStringLiteral("/jwpqt/help/") + QLatin1String(topic.file)) return true;
  return false;
}
class HandbookBrowser final : public QTextBrowser {
 public:
  explicit HandbookBrowser(QWidget* parent) : QTextBrowser(parent) {
    connect(this, &QTextBrowser::sourceChanged, this, [this] { refresh_links(); });
  }
  protected:
  void changeEvent(QEvent* event) override {
    QTextBrowser::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) refresh_links();
  }
  QVariant loadResource(int, const QUrl& url) override {
    // A valid empty result prevents QTextDocument's local-file fallback.
    if (!allowed(url)) return QByteArray{};
    QFile file(QLatin1Char(':') + url.path());
    if (!file.open(QIODevice::ReadOnly)) return QByteArray{};
    const QByteArray bytes = file.readAll();
    return url.path().endsWith(QStringLiteral(".txt"))
               ? QVariant(QByteArray("```text\n") + bytes + "\n```\n") : QVariant(bytes);
  }
 private:
  void refresh_links() {
    QColor color = palette().color(QPalette::Link);
    if (qAbs(qGray(color.rgb()) - qGray(palette().color(QPalette::Base).rgb())) < 80)
      color = palette().color(QPalette::Text);
    QList<QPair<int, int>> ranges;
    for (auto block = document()->begin(); block.isValid(); block = block.next())
      for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto fragment = it.fragment();
        if (!fragment.charFormat().anchorHref().isEmpty()) ranges.append({fragment.position(), fragment.length()});
      }
    // Markdown fixes link foregrounds; recolor only anchors without reloading history or moving the selection.
    const QSignalBlocker blocked(document());
    for (const auto& range : ranges) {
      QTextCursor cursor(document());
      cursor.setPosition(range.first);
      cursor.setPosition(range.first + range.second, QTextCursor::KeepAnchor);
      QTextCharFormat format;
      format.setForeground(color);
      cursor.mergeCharFormat(format);
    }
  }
};
}

HelpWindow::HelpWindow(QWidget* owner) : QDialog(owner) {
  initialize_handbook();
  setObjectName(QStringLiteral("helpWindow"));
  setWindowTitle(tr("jwpqt Handbook"));
  resize(900, 620);
  auto* layout = new QVBoxLayout(this);
  auto* navigation = new QHBoxLayout;
  auto* back = new QPushButton(tr("Back"), this);
  auto* forward = new QPushButton(tr("Forward"), this);
  auto* home = new QPushButton(tr("Contents"), this);
  auto* smaller = new QPushButton(tr("Zoom Out"), this);
  auto* larger = new QPushButton(tr("Zoom In"), this);
  back->setObjectName(QStringLiteral("helpBack"));
  forward->setObjectName(QStringLiteral("helpForward"));
  home->setObjectName(QStringLiteral("helpContents"));
  for (auto* button : {back, forward, home, smaller, larger}) {
    button->setAutoDefault(false);
    navigation->addWidget(button);
  }
  navigation->addStretch();
  layout->addLayout(navigation);
  search_ = new QLineEdit(this);
  search_->setObjectName(QStringLiteral("helpSearch"));
  search_->setPlaceholderText(tr("Search all handbook topics"));
  search_->setAccessibleName(tr("Search all handbook topics"));
  layout->addWidget(search_);
  auto* splitter = new QSplitter(this);
  topics_ = new QListWidget(splitter);
  topics_->setObjectName(QStringLiteral("helpTopics"));
  topics_->setAccessibleName(tr("Handbook topics"));
  browser_ = new HandbookBrowser(splitter);
  browser_->setObjectName(QStringLiteral("helpText"));
  browser_->setOpenLinks(false);
  browser_->setOpenExternalLinks(false);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({240, 660});
  layout->addWidget(splitter, 1);
  status_ = new QLabel(this);
  status_->setObjectName(QStringLiteral("helpStatus"));
  status_->setWordWrap(true);
  layout->addWidget(status_);
  auto filter = [this](const QString& query) {
    const QPointer<HelpWindow> self(this);
    const QString needle = query.simplified();
    topics_->clear();
    if (!self) return;
    for (const auto& topic : topics) {
      QFile file(QStringLiteral(":/jwpqt/help/") + QLatin1String(topic.file));
      if (!file.open(QIODevice::ReadOnly)) continue;
      const QString title = tr(topic.title);
      if (!title.contains(needle, Qt::CaseInsensitive) &&
          !QString::fromUtf8(file.readAll()).simplified().contains(needle, Qt::CaseInsensitive)) continue;
      auto* item = new QListWidgetItem(title, topics_);
      if (!self) return;
      item->setData(Qt::UserRole, QString::fromLatin1(topic.file));
      if (!self) return;
    }
    status_->setText(tr("%1 topics. Help is offline; document and dictionary data are not searched.")
                         .arg(topics_->count()));
  };
  connect(search_, &QLineEdit::textChanged, this, filter);
  connect(search_, &QLineEdit::returnPressed, this, [this] {
    if (topics_->count()) open_topic(topics_->item(0)->data(Qt::UserRole).toString());
  });
  connect(topics_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
    open_topic(item->data(Qt::UserRole).toString());
  });
  connect(topics_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    open_topic(item->data(Qt::UserRole).toString());
  });
  connect(browser_, &QTextBrowser::anchorClicked, this, &HelpWindow::navigate);
  connect(back, &QPushButton::clicked, this, [this] { move_history(true); });
  connect(forward, &QPushButton::clicked, this, [this] { move_history(false); });
  connect(browser_, &QTextBrowser::backwardAvailable, back, &QPushButton::setEnabled);
  connect(browser_, &QTextBrowser::forwardAvailable, forward, &QPushButton::setEnabled);
  back->setEnabled(false); forward->setEnabled(false);
  connect(home, &QPushButton::clicked, this, [this] {
    const QPointer<HelpWindow> self(this);
    search_->clear();
    if (self) open_topic();
  });
  connect(smaller, &QPushButton::clicked, browser_, [this] { browser_->zoomOut(); });
  connect(larger, &QPushButton::clicked, browser_, [this] { browser_->zoomIn(); });
  filter({});
  qApp->installEventFilter(this);
}

HelpWindow::~HelpWindow() {
  // QTextBrowser continues updating history after emitting sourceChanged.
  if (navigating_) { browser_->setParent(nullptr); browser_->deleteLater(); }
}

void HelpWindow::move_history(bool backward) {
  if (navigating_) return;
  const QPointer<HelpWindow> self(this);
  navigating_ = true;
  if (backward) browser_->backward(); else browser_->forward();
  if (self) navigating_ = false;
}

void HelpWindow::navigate(const QUrl& link) {
  if (navigating_) return;
  const QPointer<HelpWindow> self(this);
  const QUrl url = browser_->source().resolved(link);
  if (!allowed(url)) {
    status_->setText(tr("Only bundled handbook links can be opened here."));
    return;
  }
  navigating_ = true;
  browser_->setSource(url, QTextDocument::MarkdownResource);
  if (self) {
    navigating_ = false;
    status_->setText(tr("jwpqt %1 - offline handbook").arg(QStringLiteral(JWPQT_VERSION)));
  }
}

void HelpWindow::open_topic(const QString& topic) {
  const QPointer<HelpWindow> self(this);
  navigate(QUrl(QStringLiteral("qrc:/jwpqt/help/") + topic));
  if (self) show();
  if (self) raise();
  if (self) activateWindow();
}

void HelpWindow::open_owner_topic(QWidget* origin, const QString& topic) {
  if (!origin) return;
  QWidget* owner = origin;
  while (owner->parentWidget()) owner = owner->parentWidget();
  HelpWindow* help = nullptr;
  for (QObject* child : owner->children())
    if ((help = dynamic_cast<HelpWindow*>(child))) break;
  if (!help) help = new HelpWindow(owner);
  help->open_topic(topic);
}

bool HelpWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress) return false;
  auto* key = static_cast<QKeyEvent*>(event);
  if (key->key() != Qt::Key_F1 || key->modifiers() != Qt::NoModifier) return false;
  auto* widget = qobject_cast<QWidget*>(watched);
  QString topic = QStringLiteral("start.md");
  bool found = false;
  for (auto* current = widget; current; current = current->parentWidget()) {
    if (!found) {
      const QString name = current->objectName().toLower();
      if (name.contains(QStringLiteral("find")) || name.contains(QStringLiteral("replace"))) topic = QStringLiteral("editing.md");
      else if (name.contains(QStringLiteral("dict"))) topic = QStringLiteral("dictionary.md");
      else if (name.contains(QStringLiteral("kanji")) || name.contains(QStringLiteral("jis"))) topic = QStringLiteral("kanji.md");
      else if (name.contains(QStringLiteral("print")) || name.contains(QStringLiteral("pagelayout"))) topic = QStringLiteral("printing.md");
      else if (name.contains(QStringLiteral("settings")) || name.contains(QStringLiteral("toolbar"))) topic = QStringLiteral("settings.md");
      else if (name.contains(QStringLiteral("project")) || name.contains(QStringLiteral("file"))) topic = QStringLiteral("files.md");
      found = topic != QStringLiteral("start.md");
    }
    if (current != parentWidget()) continue;
    key->accept();
    if (event->type() == QEvent::KeyPress) open_topic(topic);
    return true;
  }
  return false;
}
}
