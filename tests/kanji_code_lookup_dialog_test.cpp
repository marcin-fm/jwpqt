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
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>

#include "jwpqt/core/kanji_info.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "text_bridge.h"
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
  for (int i = 0; i < compact_grid->count(); ++i) {
    const auto* item = compact_grid->item(i);
    if (item->data(Qt::UserRole).isValid()) continue;
    require(QFontMetrics(item->font()).horizontalAdvance(item->text()) + 6 <=
                compact_grid->gridSize().width(),
            "A two-digit Bushu stroke heading is clipped");
  }
  compact.close();
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
  for (const char* name : {"skipLegend", "fourCornerLegend"}) {
    const auto* legend = dialog.findChild<QLabel*>(QLatin1String(name));
    require(legend && !legend->pixmap().isNull(), "Lookup reference diagram is not embedded");
  }
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
    for (const auto* item : {bushu_item, stroke_item}) {
      const QImage icon = item->icon().pixmap(QSize(16, 16), 1.0).toImage();
      require(icon.pixelColor(0, 0) == (dark ? palette.color(QPalette::Window) : QColor(Qt::white)) &&
                  icon.pixelColor(4, 8) == (dark ? palette.color(QPalette::Text) : QColor(Qt::black)),
              "Bushu bitmap does not follow the live light/dark palette");
    }
    const QImage spahn_image = spahn_item->icon().pixmap(QSize(16, 16), 1.0).toImage();
    const QImage skip = dialog.findChild<QLabel*>(QStringLiteral("skipLegend"))->pixmap().toImage();
    const QImage corner = dialog.findChild<QLabel*>(QStringLiteral("fourCornerLegend"))->pixmap().toImage();
    require(skip.size() == QSize(236, 96) && corner.size() == QSize(320, 120),
            "Vector lookup artwork has the wrong logical size");
    require(transparent_pixels(skip) > 100 && transparent_pixels(corner) > 100,
            "Vector lookup artwork painted an opaque paper background");
    const QColor artwork_ink = dark ? palette.color(QPalette::Text) : QColor(Qt::black);
    require(contains_opaque_color(skip, artwork_ink) &&
                contains_opaque_color(corner, artwork_ink),
            "Vector lookup artwork did not use the live light/dark text color");
    for (int row = 0; row < 2; ++row)
      for (int column = 0; column < 2; ++column)
        require(opaque_pixels(skip, QRect(column * 118, row * 48, 118, 48)) > 40,
                "A SKIP vector diagram quadrant is empty");
    for (int cell = 0; cell < 10; ++cell)
      require(opaque_pixels(corner, QRect(cell * 32, 0, 32, 120)) > 30,
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
  return EXIT_SUCCESS;
}
