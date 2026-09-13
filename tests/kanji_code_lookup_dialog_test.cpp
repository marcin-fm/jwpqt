// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QPointer>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFontMetrics>
#include <QImage>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegion>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>

#include "jwpqt/core/kanji_info.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_lookup_page.h"
#include "kanji_reading_lookup_dialog.h"
#include "text_bridge.h"
#include "vector_artwork.h"
#include "jwpqt/core/jwp_text_codec.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

int opaque_pixels(const QImage& image, const QRect& region) {
  int count = 0;
  for (int y = region.top(); y <= region.bottom(); ++y)
    for (int x = region.left(); x <= region.right(); ++x)
      if (image.pixelColor(x, y).alpha() >= 192) ++count;
  return count;
}

int transparent_pixels(const QImage& image) {
  int count = 0;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if (image.pixelColor(x, y).alpha() == 0) ++count;
  return count;
}

bool contains_opaque_color(const QImage& image, const QColor& color) {
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if (image.pixelColor(x, y).alpha() >= 192 &&
          image.pixelColor(x, y).rgb() == color.rgb())
        return true;
  return false;
}

QImage render_artwork(jwpqt::qt::SvgArtworkWidget& widget,
                      QSize logical_size) {
  const QSize previous = widget.size();
  widget.resize(logical_size);
  QImage image(logical_size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  widget.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
  painter.end();
  widget.resize(previous);
  return image;
}

QRect artwork_region(QRectF source_region, QSizeF source_size,
                     const QImage& image) {
  QSizeF target = source_size;
  target.scale(image.size(), Qt::KeepAspectRatio);
  const QPointF origin((image.width() - target.width()) / 2.0,
                       (image.height() - target.height()) / 2.0);
  const qreal scale = target.width() / source_size.width();
  return QRectF(origin.x() + source_region.x() * scale,
                origin.y() + source_region.y() * scale,
                source_region.width() * scale,
                source_region.height() * scale)
      .toAlignedRect();
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void put_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] = static_cast<char>((value >> shift) & 0xffU);
}

jwpqt::core::KanjiInfoDatabase database(std::uint32_t flags = 0x28U) {
  constexpr std::size_t variable = 28;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, flags);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 12, 23U | (3U << 8U));
  put_u16(bytes, 14, (1U << 8U) | (2U << 11U));
  put_u16(bytes, 16, 3U);
  put_u16(bytes, 18, (2492U << 1U) | 1U);
  put_u16(bytes, 20, 2829U << 1U);
  put_u16(bytes, 22, 1927U);
  put_u32(bytes, 24, static_cast<std::uint32_t>(variable) << 8U);
  append_u16(bytes, 10947U);
  append_u32(bytes,
              4U | (1123U << 4U) | (2U << 17U) | (4U << 22U) | (5U << 27U));
  append_u32(bytes, 7U | 1234U << 6U | 5U << 20U);
  bytes.push_back('F');
  append_u16(bytes, 640U);
  bytes.push_back('\0');
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

jwpqt::core::KanjiLookupLists lookup_lists(
    const std::vector<std::vector<jwpqt::core::JisCode>>& groups) {
  std::string bytes(groups.size() * 4U, '\0');
  std::size_t offset = bytes.size();
  for (std::size_t index = 0; index < groups.size(); ++index) {
    put_u16(bytes, index * 4U, static_cast<std::uint16_t>(offset));
    put_u16(bytes, index * 4U + 2U,
            static_cast<std::uint16_t>(groups[index].size()));
    for (const auto code : groups[index]) append_u16(bytes, code);
    offset = bytes.size();
  }
  return jwpqt::core::KanjiLookupLists::parse(bytes, groups.size());
}

void test_radical_and_stroke_tabs() {
  using namespace jwpqt;
  std::vector<std::vector<core::JisCode>> radical_groups(
      core::kRadicalListGroups);
  radical_groups[0] = {0x3021U};
  std::vector<std::vector<core::JisCode>> stroke_groups(
      core::kStrokeListGroups);
  stroke_groups[2] = {0x3021U};
  const auto radicals = lookup_lists(radical_groups);
  const auto strokes = lookup_lists(stroke_groups);
  const auto source = database();
  std::vector<core::JisCode> inserted;
  core::JisCode shown = 0;
  qt::KanjiCodeLookupDialog dialog(
      source, [&](const auto& codes) { inserted = codes; },
      [&](core::JisCode code) { shown = code; }, nullptr, QPixmap{},
      &radicals, &strokes);
  const auto* tabs = dialog.findChild<QTabWidget*>();
  require(tabs && tabs->count() == 8 && tabs->tabText(6) == "Radical" &&
               tabs->tabText(7) == "Stroke Count",
           "Radical and Stroke Count are not first-class lookup tabs");
  dialog.show();
  QApplication::processEvents();
  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiCodeResults"));
  auto* insert =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInsert"));
  auto* information =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInfo"));
  auto* automatic =
      dialog.findChild<QCheckBox*>(QStringLiteral("kanjiCodeAutoSearch"));
  auto* search =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeSearch"));
  auto* clear =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeClear"));

  dialog.select_radical_mode();
  auto* radical = dynamic_cast<qt::KanjiLookupPage*>(
      dialog.findChild<QWidget*>(QStringLiteral("kanjiRadicalLookupPage")));
  require(radical && tabs->currentWidget() == radical,
          "Radical mode did not select its shared lookup tab");
  radical->set_selected_radicals({0});
  require(dialog.search_radical() &&
               dialog.results().size() == 1 &&
               dialog.results()[0].code == 0x3021U && results &&
                results->isVisible() && insert && insert->isVisible() &&
                information && information->isVisible(),
            "Radical tab did not run the bounded radical lookup");
  auto* stroke_count = radical->findChild<QSpinBox*>(
      QStringLiteral("kanjiRadicalLookupStrokeCount"));
  const int tabs_top = tabs->mapTo(&dialog, QPoint{}).y();
  require(search && clear &&
              dialog.findChildren<QListWidget*>(
                  QStringLiteral("kanjiCodeResults")).size() == 1 &&
              radical->findChild<QListWidget*>(
                  QStringLiteral("kanjiRadicalLookupResults")) == nullptr &&
              radical->findChild<QPushButton*>(
                  QStringLiteral("kanjiRadicalLookupSearch")) == nullptr &&
              results->mapTo(&dialog, QPoint{}).y() + results->height() <
                  tabs_top &&
              search->mapTo(&dialog, QPoint{}).y() + search->height() <
                  tabs_top &&
              stroke_count &&
              stroke_count->mapTo(&dialog, QPoint{}).y() > tabs_top,
          "Radical tab duplicated result controls or did not begin at Stroke count");
  require(dialog.grab().save(QStringLiteral("lookup-radical-unified-light.png")),
          "Could not save the unified Radical layout acceptance image");
  insert->click();
  information->click();
  require(inserted == std::vector<core::JisCode>{0x3021U} &&
              shown == 0x3021U,
          "Radical tab lost shared result callbacks");

  dialog.select_stroke_mode();
  auto* stroke = dynamic_cast<qt::KanjiLookupPage*>(
      dialog.findChild<QWidget*>(QStringLiteral("kanjiStrokeLookupPage")));
  require(stroke && tabs->currentWidget() == stroke,
          "Stroke Count mode did not select its shared lookup tab");
  stroke->set_stroke_range(3, 3);
  require(dialog.search_stroke() &&
               dialog.results().size() == 1 &&
               dialog.results()[0].code == 0x3021U &&
               results->item(0)->toolTip() == QStringLiteral("3 strokes"),
           "Stroke Count tab did not run the bounded stroke lookup");

  int automatic_changes = 0;
  dialog.set_auto_search_handler([&](bool automatic) {
    ++automatic_changes;
    require(!automatic, "Shared Auto Search published the wrong value");
  });
  require(automatic && automatic->isChecked(),
          "Shared Auto Search is not visible on Radical and Stroke tabs");
  automatic->click();
  require(automatic_changes == 1 && !automatic->isChecked(),
          "Shared lookup Auto Search did not publish once");

  QPalette dark = dialog.palette();
  dark.setColor(QPalette::Window, QColor(35, 38, 41));
  dark.setColor(QPalette::Base, QColor(24, 27, 29));
  dark.setColor(QPalette::Text, QColor(236, 236, 236));
  dark.setColor(QPalette::WindowText, QColor(236, 236, 236));
  dark.setColor(QPalette::Button, QColor(66, 69, 73));
  dark.setColor(QPalette::ButtonText, QColor(236, 236, 236));
  dark.setColor(QPalette::Highlight, QColor(64, 112, 170));
  dark.setColor(QPalette::HighlightedText, QColor(Qt::white));
  dialog.setPalette(dark);
  dialog.select_radical_mode();
  QApplication::processEvents();
  require(dialog.grab().save(QStringLiteral("lookup-radical-unified-dark.png")),
          "Could not save the dark unified Radical layout acceptance image");
}

void test_dialog() {
  const auto source = database();
  std::vector<jwpqt::core::JisCode> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiCodeLookupDialog dialog(
      source, [&](const auto& codes) { inserted = codes; },
      [&](jwpqt::core::JisCode code) { shown = code; });
  jwpqt::core::KanjiSkipQuery skip;
  skip.type = {1, 1};
  skip.first = {2, 2};
  skip.second = {3, 3};
  dialog.set_skip_query(skip);
  require(dialog.search_skip() && dialog.results().size() == 1 &&
              dialog.results()[0].code == 0x3021U,
          "Native SKIP dialog returned wrong results");
  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiCodeResults"));
  require(results->flow() == QListView::LeftToRight && !results->isWrapping() &&
              results->font().pixelSize() == 16 && results->item(0)->isSelected(),
          "Code lookup results are not a readable selected character strip");
  results->item(0)->setSelected(true);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInsert"))->click();
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInfo"))->click();
  require(inserted == std::vector<jwpqt::core::JisCode>{0x3021U} &&
              shown == 0x3021U,
          "Native code lookup callbacks received wrong result");

  jwpqt::core::KanjiFourCornerQuery corner;
  corner.digits = {1, 2, 3, 4, 5};
  dialog.set_four_corner_query(corner);
  require(dialog.search_four_corner() && dialog.results().size() == 1,
          "Native four-corner dialog returned wrong results");

  jwpqt::core::KanjiBushuQuery bushu;
  bushu.radical = {22, 22};
  bushu.strokes = {3, 3};
  bushu.classical = false;
  dialog.set_bushu_query(bushu);
  require(dialog.search_bushu() && dialog.results().size() == 1,
          "Native Bushu dialog returned wrong results");
  auto* nelson =
      dialog.findChild<QCheckBox*>(QStringLiteral("bushuNelson"));
  auto* classical =
      dialog.findChild<QCheckBox*>(QStringLiteral("bushuClassical"));
  require(nelson != nullptr && classical != nullptr,
          "Native Bushu controls were not created");
  nelson->setChecked(false);
  require(classical->isChecked(),
          "Native Bushu dialog allowed both radical systems to be disabled");

  jwpqt::core::KanjiSpahnQuery spahn;
  spahn.radical_strokes = {2, 2};
  spahn.radical = {4, 4};
  spahn.other_strokes = {5, 5};
  spahn.index = {7, 7};
  dialog.set_spahn_query(spahn);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeSearch"))->click();
  require(dialog.results().size() == 1,
          "Native Spahn-Hadamitzky dialog returned wrong results");

  auto* radical_strokes = dialog.findChild<QSpinBox*>(
      QStringLiteral("strokeBushuRadicalStrokes"));
  auto* variants =
      dialog.findChild<QCheckBox*>(QStringLiteral("strokeBushuVariants"));
  auto* radicals =
      dialog.findChild<QListWidget*>(QStringLiteral("strokeBushuRadicals"));
  auto* minimum = dialog.findChild<QSpinBox*>(
      QStringLiteral("strokeBushuMinimumStrokes"));
  auto* maximum = dialog.findChild<QSpinBox*>(
      QStringLiteral("strokeBushuMaximumStrokes"));
  require(radical_strokes != nullptr && variants != nullptr &&
              radicals != nullptr && minimum != nullptr && maximum != nullptr,
          "Native Stroke/Bushu controls were not created");
  radical_strokes->setValue(2);
  variants->setChecked(false);
  bool selected_bushu = false;
  for (int row = 0; row < radicals->count(); ++row) {
    if (radicals->item(row)->data(Qt::UserRole).toUInt() == 22U) {
      radicals->setCurrentRow(row);
      selected_bushu = true;
      break;
    }
  }
  minimum->setValue(3);
  maximum->setValue(3);
  require(selected_bushu && dialog.search_stroke_bushu() &&
              dialog.results().size() == 1,
          "Native Stroke/Bushu lookup returned wrong results");
  radical_strokes->setValue(4);
  variants->setChecked(true);
  require(radicals->count() == 44,
          "Stroke/Bushu variants did not preserve every source sprite");
  variants->setChecked(false);
  require(radicals->count() == 35,
          "Stroke/Bushu reduced choices are not source-compatible");
}

void test_index_dialog() {
  for (const std::uint32_t flags : {0x20U, 0x28U, 0x38U}) {
    const auto source = database(flags);
    jwpqt::qt::KanjiCodeLookupDialog dialog(source, {}, {});
    auto* type = dialog.findChild<QComboBox*>(QStringLiteral("kanjiIndexType"));
    auto* index = dialog.findChild<QSpinBox*>(QStringLiteral("kanjiIndexValue"));
    auto* volume = dialog.findChild<QSpinBox*>(QStringLiteral("kanjiIndexVolume"));
    auto* tabs = dialog.findChild<QTabWidget*>();
    require(type && index && volume && tabs && tabs->count() == 6 &&
                type->count() == (flags == 0x20U ? 4 : flags == 0x28U ? 6 : 21) &&
                !volume->isEnabled(), "Index lookup types do not follow metadata capabilities");
    tabs->setCurrentIndex(5);
    index->setValue(2829);
    dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeSearch"))->click();
    require(dialog.results().size() == 1 && dialog.results()[0].code == 0x3021U,
            "Index lookup did not search the selected Nelson number");
    if (flags != 0x20U) {
      type->setCurrentIndex(5);
      require(volume->isEnabled() && dialog.results().size() == 1,
              "Changing index type cleared results or left volume disabled");
      index->setValue(1123);
      volume->setValue(4);
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeSearch"))->click();
      require(dialog.results().size() == 1, "Morohashi volume lookup failed");
      index->setValue(65535);
      require(!dialog.search_index() && dialog.results().size() == 1,
              "Invalid volume index replaced working results");
    }
    if (flags == 0x38U) {
      type->setCurrentIndex(14);
      require(!volume->isEnabled(), "Frequency lookup retained an active volume field");
      index->setValue(640);
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeSearch"))->click();
      require(dialog.results().size() == 1, "Frequency index lookup failed");
      type->setCurrentIndex(19);
      require(volume->isEnabled(), "Busy People did not enable volume");
    }
    dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeClear"))->click();
    require(index->value() == 0 && volume->value() == 0 && dialog.results().empty(),
            "Index Clear did not reset fields and results");
  }
}

void test_graphical_controls_and_automatic_search() {
  const auto source = database(0x38U);
  QPixmap sheet(16, 241 * 16);
  sheet.fill(Qt::white);
  const QFont original_font = QApplication::font();
  QFont wide_font = original_font;
  wide_font.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
  wide_font.setStretch(125);
  QApplication::setFont(wide_font);
  {
    jwpqt::qt::KanjiCodeLookupDialog compact(source, {}, {}, nullptr, sheet);
    compact.select_bushu_mode();
    compact.show();
    QApplication::processEvents();
    auto* compact_grid = compact.findChild<QListWidget*>(QStringLiteral("bushuRadicals"));
    compact_grid->doItemsLayout();
    QApplication::processEvents();
    require(compact_grid->verticalScrollBar()->maximum() == 0 &&
                compact_grid->horizontalScrollBar()->maximum() == 0,
            "The default Bushu glyph grid unnecessarily hides later stroke groups");
    bool reduced = false;
    for (int i = 0; i < compact_grid->count(); ++i) {
      const auto* item = compact_grid->item(i);
      if (item->data(Qt::UserRole).isValid()) continue;
      require(QFontMetrics(item->font()).horizontalAdvance(item->text()) + 6 <=
                  compact_grid->gridSize().width(),
              "A two-digit Bushu stroke heading is clipped");
      reduced = reduced || item->font().pixelSize() < 16;
    }
    require(reduced, "Wide-font Bushu headings did not adapt");
  }
  QApplication::setFont(original_font);
  jwpqt::qt::KanjiCodeLookupDialog dialog(source, {}, {});
  auto* bushu = dialog.findChild<QListWidget*>(QStringLiteral("bushuRadicals"));
  auto* spahn = dialog.findChild<QListWidget*>(QStringLiteral("spahnRadicals"));
  auto* variants = dialog.findChild<QCheckBox*>(QStringLiteral("spahnVariants"));
  auto* automatic = dialog.findChild<QCheckBox*>(QStringLiteral("kanjiCodeAutoSearch"));
  auto* timer = dialog.findChild<QTimer*>(QStringLiteral("kanjiCodeSearchTimer"));
  auto* clear = dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeClear"));
  require(bushu && bushu->count() == 258 && spahn && spahn->count() == 116 &&
              !spahn->item(0)->icon().isNull() && variants && automatic &&
              automatic->isChecked() && timer && clear,
          "Graphical radical choices or automatic search controls are missing");
  for (int row = 0; row < bushu->count(); ++row) {
    auto* item = bushu->item(row);
    if (item->data(Qt::UserRole).isValid()) continue;
    require(!(item->flags() & Qt::ItemIsSelectable), "Stroke header is selectable as a radical");
    QMetaObject::invokeMethod(bushu, "itemClicked", Qt::DirectConnection,
                              Q_ARG(QListWidgetItem*, item));
  }
  require(dialog.findChild<QSpinBox*>(QStringLiteral("bushuRadical"))->value() == -1,
          "Clicking a stroke header changed the radical query");
  auto* skip_legend = dynamic_cast<jwpqt::qt::SvgArtworkWidget*>(
      dialog.findChild<QWidget*>(QStringLiteral("skipLegend")));
  auto* corner_legend = dynamic_cast<jwpqt::qt::SvgArtworkWidget*>(
      dialog.findChild<QWidget*>(QStringLiteral("fourCornerLegend")));
  require(skip_legend && corner_legend,
          "Responsive lookup reference diagrams are not embedded");
  require(skip_legend->minimumSizeHint() == QSize(118, 48) &&
              skip_legend->sizeHint() == QSize(236, 96) &&
              skip_legend->hasHeightForWidth() &&
              skip_legend->heightForWidth(472) == 192 &&
              corner_legend->minimumSizeHint() == QSize(160, 60) &&
              corner_legend->sizeHint() == QSize(320, 120) &&
              corner_legend->hasHeightForWidth() &&
              corner_legend->heightForWidth(640) == 240,
          "Lookup reference diagrams do not expose responsive vector geometry");
  variants->setChecked(false);
  require(spahn->count() == 79, "Spahn variants did not reduce to canonical choices");
  dialog.select_spahn_mode();
  dialog.show();
  QApplication::processEvents();
  const QPoint point = spahn->visualItemRect(spahn->item(0)).center();
  QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                    QPointF(spahn->viewport()->mapToGlobal(point)), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point),
                      QPointF(spahn->viewport()->mapToGlobal(point)), Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(spahn->viewport(), &press);
  QApplication::sendEvent(spahn->viewport(), &release);
  require(dialog.findChild<QSpinBox*>(QStringLiteral("spahnRadicalStrokes"))->value() == 2 &&
              dialog.findChild<QSpinBox*>(QStringLiteral("spahnRadical"))->value() == 0 &&
              spahn->count() == 19,
          "Clicking a Spahn glyph did not set the source stroke/letter code");

  timer->setInterval(1);
  jwpqt::core::KanjiSpahnQuery query;
  query.radical_strokes = {2, 2};
  query.radical = {4, 4};
  query.other_strokes = {5, 5};
  query.index = {7, 7};
  dialog.set_spahn_query(query);
  QEventLoop loop;
  QTimer::singleShot(20, &loop, &QEventLoop::quit);
  loop.exec();
  require(dialog.results().size() == 1, "Visible field changes did not run automatic search");
  dialog.set_automatic_search(false);
  require(dialog.results().size() == 1 && !timer->isActive(),
          "Applying automatic preference changed existing results");
  automatic->setChecked(false);
  dialog.findChild<QSpinBox*>(QStringLiteral("spahnRadical"))->setValue(0);
  require(!timer->isActive() && dialog.results().empty(),
          "Changed criteria with Auto off retained stale results");
  automatic->setChecked(true);
  require(timer->isActive(), "Automatic search did not schedule the pending query");
  clear->click();
  QTimer::singleShot(20, &loop, &QEventLoop::quit);
  loop.exec();
  require(dialog.results().empty() && !timer->isActive(), "Clear allowed stale results to reappear");
  dialog.findChild<QSpinBox*>(QStringLiteral("spahnRadical"))->setValue(4);
  require(timer->isActive(), "Could not prepare pending lookup on tab change");
  dialog.select_index_mode();
  require(!timer->isActive() && automatic->isHidden(), "Index tab retained another mode's automatic search");
  dialog.findChild<QSpinBox*>(QStringLiteral("kanjiIndexValue"))->setValue(2829);
  QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
  require(dialog.results().empty(), "A stale timeout ran the Index query");
  dialog.select_spahn_mode();
  dialog.findChild<QSpinBox*>(QStringLiteral("spahnOtherStrokes"))->setValue(5);
  require(timer->isActive(), "Could not prepare a pending lookup before close");
  dialog.reject();
  QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
  require(!timer->isActive() && dialog.results().empty(), "Closing the lookup retained automatic work");
}

void test_result_keys_and_bushu_steps() {
  const auto source = database();
  std::vector<jwpqt::core::JisCode> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiCodeLookupDialog dialog(source,
      [&](const auto& codes) { inserted = codes; }, [&](auto code) { shown = code; });
  dialog.set_automatic_search(false);
  dialog.select_bushu_mode();
  auto* radical = dialog.findChild<QSpinBox*>("bushuRadical");
  auto* strokes = dialog.findChild<QSpinBox*>("bushuStrokes");
  auto* choices = dialog.findChild<QListWidget*>("bushuRadicals");
  radical->setValue(75);
  require(choices->currentItem() && choices->currentItem()->data(Qt::UserRole + 2).toInt() == 4,
          "Bushu stepping did not use the selected canonical radical's stroke count");
  auto key = [](QWidget* target, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers);
    QApplication::sendEvent(target, &event);
  };
  key(strokes, Qt::Key_Up);
  require(strokes->value() == 4, "Bushu Any did not step to radical minimum");
  key(strokes, Qt::Key_Down);
  require(strokes->value() == -1, "Bushu minimum did not step to Any");
  key(strokes, Qt::Key_Down);
  require(strokes->value() == 30, "Bushu Any did not wrap backwards");
  key(strokes, Qt::Key_Up);
  require(strokes->value() == -1, "Bushu maximum did not wrap to Any");
  auto* list = dialog.findChild<QListWidget*>("kanjiCodeResults");
  jwpqt::core::JwpText codes;
  for (int i = 0; i < 8; ++i) {
    codes.push_back(static_cast<jwpqt::core::JisCode>(0x3021 + i));
    auto* item = new QListWidgetItem(jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text({codes.back()})), list);
    item->setData(Qt::UserRole, codes.back());
  }
  list->setCurrentRow(0);
  key(list, Qt::Key_F2);
  require(list->currentRow() == 1, "F2 did not move forward");
  key(list, Qt::Key_Right, Qt::ControlModifier);
  require(list->currentRow() == 6, "Ctrl-Right did not move five characters");
  key(list, Qt::Key_F3, Qt::ControlModifier);
  require(list->currentRow() == 1, "Ctrl-F3 did not move five characters backwards");
  key(list, Qt::Key_Less, Qt::ShiftModifier);
  require(list->currentRow() == 0, "Less-than did not move backwards");
  key(list, Qt::Key_I);
  require(shown == codes.front(), "I did not open current character information");
  key(list, Qt::Key_Return);
  require(inserted == jwpqt::core::JwpText{codes.front()}, "Return searched instead of inserting the result");
  key(list, Qt::Key_C, Qt::ShiftModifier);
  require(QApplication::clipboard()->text() == jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text(codes)) &&
              list->currentRow() == 0 && list->selectedItems().size() == 1,
          "Shift-C did not copy all results without changing selection");
  key(list, Qt::Key_C, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == list->item(0)->text(), "Ctrl-C did not copy selected result");
  auto* copy = dialog.findChild<QPushButton*>("kanjiCodeCopy");
  key(copy, Qt::Key_Space, Qt::ShiftModifier);
  require(QApplication::clipboard()->text() == jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text(codes)),
          "Shift-Space on Copy did not copy all");
  QApplication::clipboard()->clear();
  QMouseEvent copy_click(QEvent::MouseButtonRelease, QPointF(copy->rect().center()),
      QPointF(copy->mapToGlobal(copy->rect().center())), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
  QApplication::sendEvent(copy, &copy_click);
  require(QApplication::clipboard()->text() == jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text(codes)) &&
              list->selectedItems().size() == 1, "Shift-click Copy changed selection or omitted results");
  dialog.show();
  key(list, Qt::Key_F4);
  require(!dialog.isVisible(), "Result F4 did not close the lookup");
  dialog.set_automatic_search(true);
  dialog.select_index_mode();
  auto* index = dialog.findChild<QSpinBox*>("kanjiIndexValue");
  index->setValue(2829);
  require(dialog.search_index() && !dialog.results().empty(), "Could not prepare explicit Index result");
  index->setValue(2828);
  require(dialog.results().size() == 1 && !dialog.findChild<QTimer*>("kanjiCodeSearchTimer")->isActive(),
          "Global Auto changed explicit Index result retention");

  for (const int command : {Qt::Key_I, Qt::Key_Return}) {
    QPointer<jwpqt::qt::KanjiCodeLookupDialog> owner;
    const auto destroy = [&] { delete owner.data(); throw std::runtime_error("deleted lookup"); };
    owner = new jwpqt::qt::KanjiCodeLookupDialog(source,
        [&](const auto&) { destroy(); }, [&](auto) { destroy(); });
    require(owner->search_skip(), "Could not prepare deletion fixture");
    key(owner->findChild<QListWidget*>("kanjiCodeResults"), command);
    require(!owner, "Result callback did not destroy the owner safely");
  }
  QPointer<jwpqt::qt::KanjiCodeLookupDialog> automatic_owner =
      new jwpqt::qt::KanjiCodeLookupDialog(source, {}, {});
  automatic_owner->set_auto_search_handler([&](bool) { delete automatic_owner.data(); });
  automatic_owner->findChild<QCheckBox*>("kanjiCodeAutoSearch")->setChecked(false);
  require(!automatic_owner, "Auto callback did not safely release its dialog");
}

void test_artwork_palette_changes() {
  const auto source = database(0x38U);
  QImage image(16, 241 * 16, QImage::Format_ARGB32);
  image.fill(Qt::white);
  for (int sprite = 0; sprite < 241; ++sprite) image.setPixelColor(4, sprite * 16 + 8, Qt::black);
  jwpqt::qt::KanjiCodeLookupDialog dialog(source, {}, {}, nullptr, QPixmap::fromImage(image));
  dialog.findChild<QCheckBox*>(QStringLiteral("kanjiCodeAutoSearch"))->setChecked(false);
  auto* bushu = dialog.findChild<QListWidget*>(QStringLiteral("bushuRadicals"));
  auto* stroke = dialog.findChild<QListWidget*>(QStringLiteral("strokeBushuRadicals"));
  auto* spahn = dialog.findChild<QListWidget*>(QStringLiteral("spahnRadicals"));
  auto* timer = dialog.findChild<QTimer*>(QStringLiteral("kanjiCodeSearchTimer"));
  auto* skip_legend = dynamic_cast<jwpqt::qt::SvgArtworkWidget*>(
      dialog.findChild<QWidget*>(QStringLiteral("skipLegend")));
  auto* corner_legend = dynamic_cast<jwpqt::qt::SvgArtworkWidget*>(
      dialog.findChild<QWidget*>(QStringLiteral("fourCornerLegend")));
  require(skip_legend && corner_legend,
          "Responsive lookup artwork is missing from the palette fixture");
  dialog.select_skip_mode();
  dialog.show();
  QApplication::processEvents();
  require(skip_legend->size().width() >= skip_legend->sizeHint().width() &&
              skip_legend->size().height() >= skip_legend->sizeHint().height(),
          "The SKIP diagram did not expand into its interface page");
  dialog.select_four_corner_mode();
  QApplication::processEvents();
  require(corner_legend->size().width() >= corner_legend->sizeHint().width() &&
              corner_legend->size().height() >= corner_legend->sizeHint().height(),
          "The Four Corner diagram did not expand into its interface page");
  auto* bushu_item = bushu->item(1);
  auto* stroke_item = stroke->item(1);
  auto* spahn_item = spahn->item(0);
  bushu->setCurrentItem(bushu_item);
  stroke->setCurrentItem(stroke_item);
  spahn->setCurrentItem(spahn_item);
  dialog.set_index_query({jwpqt::core::KanjiIndexType::kNelson, 2829, 0});
  require(dialog.search_index(), "Could not prepare code lookup palette regression");
  dialog.select_bushu_mode();
  dialog.show();
  QImage light_spahn, light_skip, light_corner;
  bool saved_dark = false;
  for (bool dark : {false, true, false, true}) {
    QPalette palette = dialog.palette();
    const QColor window = dark ? QColor(32, 35, 37) : QColor(240, 240, 240);
    const QColor base = dark ? QColor(21, 22, 23) : QColor(Qt::white);
    const QColor text = dark ? QColor(240, 240, 240) : QColor(16, 16, 16);
    for (const auto role : {QPalette::WindowText, QPalette::Text,
                            QPalette::ButtonText})
      palette.setColor(QPalette::All, role, text);
    palette.setColor(QPalette::All, QPalette::Window, window);
    palette.setColor(QPalette::All, QPalette::Button, window);
    palette.setColor(QPalette::All, QPalette::Base, base);
    palette.setColor(QPalette::All, QPalette::AlternateBase,
                     dark ? QColor(42, 45, 47) : QColor(248, 248, 248));
    palette.setColor(QPalette::All, QPalette::Highlight,
                     dark ? QColor(52, 103, 145) : QColor(48, 140, 198));
    palette.setColor(QPalette::All, QPalette::HighlightedText, Qt::white);
    dialog.setPalette(palette);
    QApplication::processEvents();
    require(bushu->palette().color(QPalette::Base) == palette.color(QPalette::Base),
            "Bushu grid background retained the previous palette");
    require(skip_legend->palette().color(QPalette::Text) == text &&
                corner_legend->palette().color(QPalette::Text) == text,
            "Embedded vector diagrams retained the previous palette");
    for (const auto* item : {bushu_item, stroke_item}) {
      const QImage icon = item->icon().pixmap(QSize(16, 16), 1.0).toImage();
      require(icon.pixelColor(0, 0) == (dark ? palette.color(QPalette::Window) : QColor(Qt::white)) &&
                  icon.pixelColor(4, 8) == (dark ? palette.color(QPalette::Text) : QColor(Qt::black)),
              "Bushu bitmap does not follow the live light/dark palette");
    }
    const QImage spahn_image = spahn_item->icon().pixmap(QSize(16, 16), 1.0).toImage();
    jwpqt::qt::SvgArtworkWidget skip_probe(
        QStringLiteral(":/jwpqt/assets/icons/skip-diagram.svg"),
        QSize(236, 96));
    jwpqt::qt::SvgArtworkWidget corner_probe(
        QStringLiteral(":/jwpqt/assets/icons/four-corner-diagram.svg"),
        QSize(320, 120));
    skip_probe.setPalette(palette);
    corner_probe.setPalette(palette);
    const QImage skip_small = render_artwork(skip_probe, QSize(118, 48));
    const QImage corner_small = render_artwork(corner_probe, QSize(160, 60));
    const QImage skip = render_artwork(skip_probe, QSize(708, 288));
    const QImage corner = render_artwork(corner_probe, QSize(800, 300));
    require(skip_small.size() == QSize(118, 48) &&
                corner_small.size() == QSize(160, 60) &&
                skip.size() == QSize(708, 288) &&
                corner.size() == QSize(800, 300),
            "Responsive vector lookup artwork ignored logical or device scale");
    require(transparent_pixels(skip) > 100 && transparent_pixels(corner) > 100,
            "Vector lookup artwork painted an opaque paper background");
    const QColor artwork_ink = palette.color(QPalette::Text);
    require(contains_opaque_color(skip_small, artwork_ink) &&
                contains_opaque_color(corner_small, artwork_ink) &&
                contains_opaque_color(skip, artwork_ink) &&
                contains_opaque_color(corner, artwork_ink),
            "Vector lookup artwork did not use the live light/dark text color");
    for (int row = 0; row < 2; ++row)
      for (int column = 0; column < 2; ++column)
        require(opaque_pixels(skip, artwork_region(
                    QRectF(column * 59, row * 24, 59, 24), QSizeF(118, 48),
                    skip)) > 80,
                "A SKIP vector diagram quadrant is empty");
    for (const QRectF region : {
             QRectF(0, 0, 40, 20), QRectF(40, 0, 40, 20),
             QRectF(80, 0, 40, 20), QRectF(120, 0, 40, 20),
             QRectF(0, 20, 40, 20), QRectF(40, 20, 40, 20),
             QRectF(80, 20, 40, 20), QRectF(0, 40, 40, 20),
             QRectF(40, 40, 40, 20), QRectF(80, 40, 40, 20)})
      require(opaque_pixels(corner, artwork_region(
                  region, QSizeF(160, 60), corner)) > 50,
              "A Four Corner vector diagram cell is empty");
    if (light_spahn.isNull()) {
      dialog.select_skip_mode();
      require(dialog.grab().save(QStringLiteral("lookup-skip-vector-light.png")),
              "Could not save the light SKIP vector diagram");
      dialog.select_four_corner_mode();
      require(dialog.grab().save(QStringLiteral("lookup-four-corner-vector-light.png")),
              "Could not save the light Four Corner vector diagram");
      dialog.select_bushu_mode();
      require(dialog.grab().save(QStringLiteral("lookup-vector-artwork-light.png")),
              "Could not save the light lookup artwork acceptance image");
      light_spahn = spahn_image; light_skip = skip; light_corner = corner;
    } else if (dark && !saved_dark) {
      dialog.select_skip_mode();
      require(dialog.grab().save(QStringLiteral("lookup-skip-vector-dark.png")),
              "Could not save the dark SKIP vector diagram");
      dialog.select_four_corner_mode();
      require(dialog.grab().save(QStringLiteral("lookup-four-corner-vector-dark.png")),
              "Could not save the dark Four Corner vector diagram");
      dialog.select_bushu_mode();
      require(dialog.grab().save(QStringLiteral("lookup-vector-artwork-dark.png")),
              "Could not save the dark lookup artwork acceptance image");
      saved_dark = true;
    }
    require((spahn_image == light_spahn) != dark && (skip == light_skip) != dark &&
                (corner == light_corner) != dark,
            "Embedded lookup artwork did not switch and restore with the palette");
    require(bushu->item(0)->background().color() ==
                (dark ? palette.color(QPalette::Window) : QColor(Qt::white)) &&
                bushu->item(0)->foreground().color() == (dark ? QColor(255, 128, 128) : QColor(176, 0, 32)),
            "Bushu stroke headings do not follow the dark palette");
    require(bushu->currentItem() == bushu_item && stroke->currentItem() == stroke_item &&
                spahn->currentItem() == spahn_item && dialog.results().size() == 1 && !timer->isActive(),
            "Changing artwork rebuilt selections, results or pending lookup work");
  }
  QListWidgetItem* rare = nullptr;
  QListWidgetItem* common = nullptr;
  for (int row = 0; row < bushu->count(); ++row) {
    auto* item = bushu->item(row);
    if (!item->data(Qt::UserRole).isValid()) continue;
    if (item->data(Qt::UserRole + 1).toInt() == 66) rare = item;
    if (item->data(Qt::UserRole + 1).toInt() == 75) common = item;
  }
  require(rare && common, "Source rare radical fixture is missing");
  for (bool dark : {false, true, false}) {
    auto colors = dialog.palette();
    colors.setColor(QPalette::Window, dark ? QColor(32, 35, 37) : QColor(240, 240, 240));
    colors.setColor(QPalette::Text, dark ? QColor(240, 240, 240) : QColor(16, 16, 16));
    dialog.setPalette(colors);
    dialog.set_radical_preferences(false, true);
    const auto rare_image = rare->icon().pixmap(QSize(16, 16), 1.0).toImage();
    const auto common_image = common->icon().pixmap(QSize(16, 16), 1.0).toImage();
    require(rare_image.pixelColor(4, 8) == QColor(128, 128, 128) &&
            common_image.pixelColor(4, 8) == (dark ? colors.color(QPalette::Text) : QColor(Qt::black)) &&
            bushu->currentItem() == bushu_item && dialog.results().size() == 1 && !timer->isActive(),
            "Rare artwork changed the wrong glyph, selection or search state");
    dialog.set_radical_preferences(false, false);
    require(rare->icon().pixmap(QSize(16, 16), 1.0).toImage().pixelColor(4, 8) == common_image.pixelColor(4, 8),
            "Disabling rare artwork did not restore ink");
  }
  const int full_stroke = stroke->count(), full_spahn = spahn->count();
  const auto selected_bushu = stroke->currentItem()->data(Qt::UserRole);
  dialog.set_radical_preferences(true, false);
  require(stroke->count() < full_stroke && spahn->count() < full_spahn &&
          stroke->currentItem()->data(Qt::UserRole) == selected_bushu && dialog.results().size() == 1 && !timer->isActive(),
          "Variant reduction changed canonical selection/results or failed to hide choices");
  dialog.set_radical_preferences(false, false);
  require(stroke->count() == full_stroke && spahn->count() == full_spahn, "Variant expansion lost choices");
  QPalette dark_again = dialog.palette();
  dark_again.setColor(QPalette::Window, QColor(32, 35, 37));
  dialog.setPalette(dark_again);
  dialog.findChild<QCheckBox*>(QStringLiteral("spahnVariants"))->setChecked(false);
  dialog.findChild<QSpinBox*>(QStringLiteral("strokeBushuRadicalStrokes"))->setValue(4);
  for (const auto* item : {spahn->item(0), stroke->item(1)})
    require(item->icon().pixmap(QSize(16, 16), 1.0).toImage().pixelColor(0, 0) ==
                dialog.palette().color(QPalette::Window),
            "Rebuilt radical choices lost the active dark palette");
  timer->setInterval(60000);
  dialog.findChild<QCheckBox*>(QStringLiteral("kanjiCodeAutoSearch"))->setChecked(true);
  const int timer_id = timer->timerId();
  QPalette light = dialog.palette();
  light.setColor(QPalette::Window, QColor(240, 240, 240));
  dialog.setPalette(light);
  QApplication::processEvents();
  require(timer->isActive() && timer->timerId() == timer_id,
          "Changing artwork restarted or cancelled a pending code lookup");
}

void test_preference_callbacks() {
  using namespace jwpqt::qt;
  const auto source = database();
  KanjiCodeLookupDialog dialog(source, {}, {});
  require(dialog.findChild<QCheckBox*>("skipMisclassifications")->isEnabled(),
          "Available SKIP references were disabled");
  int calls = 0;
  dialog.set_preferences_handler([&](bool, bool, bool, int) {
    ++calls;
    dialog.set_search_preferences(false, true, true, 20);
  });
  dialog.findChild<QCheckBox*>("bushuNelson")->click();
  require(calls == 1 && !dialog.findChild<QCheckBox*>("bushuNelson")->isChecked() &&
          !dialog.findChild<QCheckBox*>("strokeBushuNelson")->isChecked(),
          "An older callback overwrote reentrant lookup preferences");
  QPointer<KanjiCodeLookupDialog> dying = new KanjiCodeLookupDialog(source, {}, {});
  dying->set_preferences_handler([&](bool, bool, bool, int) { delete dying.data(); });
  dying->findChild<QCheckBox*>("bushuNelson")->click();
  require(!dying, "Code preference callback did not dispose its owner safely");
  dying = new KanjiCodeLookupDialog(source, {}, {});
  dying->set_variants_handler([&](bool) { delete dying.data(); });
  dying->findChild<QCheckBox*>("spahnVariants")->click();
  require(!dying, "Variant preference callback did not dispose its owner safely");
  QPointer<KanjiReadingLookupDialog> reading = new KanjiReadingLookupDialog(source, {}, {});
  reading->set_preferences_handler([&](bool, bool, int) { delete reading.data(); });
  reading->findChild<QCheckBox*>("kanjiReadingFlexibleKun")->click();
  require(!reading, "Reading preference callback did not dispose its owner safely");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  test_graphical_controls_and_automatic_search();
  test_index_dialog();
  test_result_keys_and_bushu_steps();
  test_artwork_palette_changes();
  test_preference_callbacks();
  test_radical_and_stroke_tabs();
  return EXIT_SUCCESS;
}
