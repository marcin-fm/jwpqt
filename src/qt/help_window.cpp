// SPDX-License-Identifier: GPL-2.0-or-later
#include "help_window.h"
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QResource>
#include <QSet>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextFragment>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVector>

static void initialize_handbook() { Q_INIT_RESOURCE(handbook_assets); Q_INIT_RESOURCE(notice_assets); }

namespace jwpqt::qt {
namespace {
struct Topic {
  QString id;
  QString file;
  QString title;
  QString chapter;
};

struct Handbook {
  QVector<Topic> topics;
  QHash<QString, qsizetype> by_id;
  QHash<QString, qsizetype> by_file;
};

const Handbook& handbook() {
  static const Handbook value = [] {
    Handbook result;
    QFile file(QStringLiteral(":/jwpqt/help/manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) return result;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return result;
    const auto root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) return result;
    const auto entries = root.value(QStringLiteral("topics")).toArray();
    if (entries.size() != root.value(QStringLiteral("topic_count")).toInt()) return result;
    for (const auto& entry : entries) {
      const auto object = entry.toObject();
      Topic topic{object.value(QStringLiteral("id")).toString(),
                  object.value(QStringLiteral("file")).toString(),
                  object.value(QStringLiteral("title")).toString(),
                  object.value(QStringLiteral("chapter")).toString()};
      if (!topic.id.startsWith(QStringLiteral("IDH_")) || topic.file.isEmpty() ||
          !topic.file.startsWith(QStringLiteral("topics/")) || topic.file.contains(QStringLiteral("..")) ||
          topic.title.isEmpty() || topic.chapter.isEmpty() || result.by_id.contains(topic.id) ||
          result.by_file.contains(topic.file)) return Handbook{};
      const qsizetype index = result.topics.size();
      result.by_id.insert(topic.id, index);
      result.by_file.insert(topic.file, index);
      result.topics.push_back(std::move(topic));
    }
    return result;
  }();
  return value;
}

QString topic_file(QString topic) {
  const auto& data = handbook();
  if (const auto found = data.by_id.constFind(topic); found != data.by_id.cend())
    return data.topics.at(*found).file;
  return topic;
}

QString context_topic(QWidget* widget) {
  for (auto* current = widget; current; current = current->parentWidget()) {
    const QString explicit_topic = current->property("jwpqtHelpTopic").toString();
    if (handbook().by_id.contains(explicit_topic)) return explicit_topic;
    const QString name = current->objectName().toLower();
    if (name.contains(QStringLiteral("pagelayout"))) return QStringLiteral("IDH_PRINT_LAYOUT");
    if (name.contains(QStringLiteral("printpreview"))) return QStringLiteral("IDH_PRINT_PRINTING");
    if (name.contains(QStringLiteral("find")) || name.contains(QStringLiteral("replace")))
      return QStringLiteral("IDH_EDIT_SEARCH");
    if (name.contains(QStringLiteral("wnnuserdictionary"))) return QStringLiteral("IDH_TEXT_USERKANJI");
    if (name.contains(QStringLiteral("edictuserdictionary"))) return QStringLiteral("IDH_DICT_USEREDIT");
    if (name.contains(QStringLiteral("edictregistry"))) return QStringLiteral("IDH_DICT_DICTIONARIES");
    if (name.contains(QStringLiteral("kanjireadinglookup"))) return QStringLiteral("IDH_KANJI_READLOOKUP");
    if (name.contains(QStringLiteral("kanjicount"))) return QStringLiteral("IDH_KANJI_COUNTKANJI");
    if (name.contains(QStringLiteral("jistable"))) return QStringLiteral("IDH_KANJI_JISTABLE");
    if (name.contains(QStringLiteral("userdictionary"))) return QStringLiteral("IDH_DICT_USERDICT");
    if (name.contains(QStringLiteral("dictionarymanager"))) return QStringLiteral("IDH_DICT_DICTIONARIES");
    if (name.contains(QStringLiteral("edict")) || name.contains(QStringLiteral("dict")))
      return QStringLiteral("IDH_DICT_GENERAL");
    if (name.contains(QStringLiteral("characterinformation")) || name.contains(QStringLiteral("kanjiinfo")))
      return QStringLiteral("IDH_KANJI_CHARINFO");
    if (name.contains(QStringLiteral("kanji")) || name.contains(QStringLiteral("jis")))
      return QStringLiteral("IDH_KANJI_LOOKUP");
    if (name.contains(QStringLiteral("toolbar"))) return QStringLiteral("IDH_OPTIONS_TOOLBAR");
    if (name.contains(QStringLiteral("settings"))) return QStringLiteral("IDH_OPTIONS_INTRO");
    if (name.contains(QStringLiteral("project"))) return QStringLiteral("IDH_FILE_PROJECT");
    if (name.contains(QStringLiteral("file"))) return QStringLiteral("IDH_FILE_TYPES");
  }
  return QStringLiteral("IDH_INTRO_WHATISJWPCE");
}

bool allowed(const QUrl& url) {
  if (url.scheme() != QStringLiteral("qrc") || !url.host().isEmpty() || url.hasQuery()) return false;
  const QString prefix = QStringLiteral("/jwpqt/help/");
  if (!url.path().startsWith(prefix)) return false;
  const QString file = url.path().mid(prefix.size());
  static const QSet<QString> supporting = {
      QStringLiteral("start.md"), QStringLiteral("files.md"), QStringLiteral("editing.md"),
      QStringLiteral("dictionary.md"), QStringLiteral("kanji.md"), QStringLiteral("printing.md"),
      QStringLiteral("settings.md"), QStringLiteral("installation.md"), QStringLiteral("about.md"),
      QStringLiteral("gnugpl.txt"), QStringLiteral("_cpright.txt")};
  return supporting.contains(file) || handbook().by_file.contains(file);
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
    QString chapter;
    int matches = 0;
    for (const auto& topic : handbook().topics) {
      QFile file(QStringLiteral(":/jwpqt/help/") + topic.file);
      if (!file.open(QIODevice::ReadOnly)) continue;
      if (!topic.id.contains(needle, Qt::CaseInsensitive) &&
          !topic.chapter.contains(needle, Qt::CaseInsensitive) &&
          !topic.title.contains(needle, Qt::CaseInsensitive) &&
          !QString::fromUtf8(file.readAll()).simplified().contains(needle, Qt::CaseInsensitive)) continue;
      if (chapter != topic.chapter) {
        chapter = topic.chapter;
        auto* heading = new QListWidgetItem(chapter, topics_);
        heading->setFlags(Qt::NoItemFlags);
        QFont font = heading->font();
        font.setBold(true);
        heading->setFont(font);
      }
      auto* item = new QListWidgetItem(topic.title, topics_);
      if (!self) return;
      item->setData(Qt::UserRole, topic.file);
      item->setData(Qt::UserRole + 1, topic.id);
      if (!self) return;
      ++matches;
    }
    status_->setText(tr("%1 of %2 topics. Help is offline; document and dictionary data are not searched.")
                         .arg(matches).arg(handbook().topics.size()));
  };
  connect(search_, &QLineEdit::textChanged, this, filter);
  connect(search_, &QLineEdit::returnPressed, this, [this] {
    if (auto* item = topics_->currentItem()) {
      const QString file = item->data(Qt::UserRole).toString();
      if (!file.isEmpty()) {
        open_topic(file);
        return;
      }
    }
    for (int index = 0; index < topics_->count(); ++index) {
      const QString file = topics_->item(index)->data(Qt::UserRole).toString();
      if (!file.isEmpty()) { open_topic(file); break; }
    }
  });
  connect(topics_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
    const QString file = item->data(Qt::UserRole).toString();
    if (!file.isEmpty()) open_topic(file);
  });
  connect(topics_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    const QString file = item->data(Qt::UserRole).toString();
    if (!file.isEmpty()) open_topic(file);
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
  QUrl url;
  if (link.scheme() == QStringLiteral("help")) {
    const QString file = topic_file(link.path());
    if (file == link.path()) {
      status_->setText(tr("That bundled help topic is unavailable."));
      return;
    }
    url = QUrl(QStringLiteral("qrc:/jwpqt/help/") + file);
  } else {
    url = browser_->source().resolved(link);
  }
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
  navigate(QUrl(QStringLiteral("qrc:/jwpqt/help/") + topic_file(topic)));
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
  QString topic;
  for (auto* current = widget; current; current = current->parentWidget()) {
    if (topic.isEmpty()) topic = context_topic(current);
    if (current != parentWidget()) continue;
    key->accept();
    if (event->type() == QEvent::KeyPress) open_topic(topic);
    return true;
  }
  return false;
}
}
