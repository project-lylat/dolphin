// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/WiiKeysDialog.h"

#include <variant>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "Core/Boot/Boot.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"

using IOS::HLE::IOSC;

bool WiiKeysDialog::NeedsKeys(const BootParameters& parameters)
{
  bool wii = std::holds_alternative<DiscIO::VolumeWAD>(parameters.parameters);
  if (const auto* disc = std::get_if<BootParameters::Disc>(&parameters.parameters))
    wii = disc->volume && disc->volume->GetVolumeType() == DiscIO::Platform::WiiDisc;
  return wii && !IOSC::HasUserCommonKey();
}

void WiiKeysDialog::WarnMissingKeys(QWidget* parent)
{
  const int answer = ModalMessageBox::warning(
      parent, tr("Wii Keys Needed"),
      tr("This is a Wii game. This build of Dolphin does not include the Wii keys needed to read "
         "Wii discs, WADs and Wii save files, so it cannot start.\n\n"
         "Set up your own keys now? You can dump them from your Wii with BootMii."),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
  if (answer != QMessageBox::Yes)
    return;
  WiiKeysDialog dialog(parent);
  if (dialog.exec() == QDialog::Accepted)
  {
    ModalMessageBox::information(parent, tr("Wii Keys Saved"),
                                 tr("Your Wii keys are saved. Start the game again."));
  }
}

WiiKeysDialog::WiiKeysDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Set Up Wii Keys"));
  setWindowIcon(Resources::GetAppIcon());

  const IOSC::UserKeys keys = IOSC::ReadUserKeys();
  const auto make_field = [](const std::string& value) {
    auto* field = new QLineEdit(QString::fromStdString(value));
    field->setPlaceholderText(QStringLiteral("00112233445566778899aabbccddeeff"));
    field->setMaxLength(32);
    field->setMinimumWidth(field->fontMetrics().horizontalAdvance(QLatin1Char('0')) * 36);
    return field;
  };
  m_common = make_field(keys.common);
  m_korean_common = make_field(keys.korean_common);
  m_sd = make_field(keys.sd);

  auto* intro = new QLabel(
      tr("Enter your Wii's keys as 32 hex digits each. The common key is needed to play Wii "
         "games; the Korean common key is only for Korean games, and the SD key is only for "
         "importing and exporting Wii saves."));
  intro->setWordWrap(true);

  const QString path = QString::fromStdString(IOSC::GetUserKeysPath());
  auto* where = new QLabel(tr("They are saved to <a href=\"%1\">%2</a>.")
                               .arg(QUrl::fromLocalFile(QFileInfo(path).absolutePath()).toString(),
                                    QDir::toNativeSeparators(path).toHtmlEscaped()));
  where->setTextFormat(Qt::RichText);
  where->setOpenExternalLinks(true);
  where->setWordWrap(true);

  m_error = new QLabel;
  m_error->setStyleSheet(QStringLiteral("color: #e05050;"));
  m_error->hide();

  auto* form = new QFormLayout;
  form->addRow(tr("Common key:"), m_common);
  form->addRow(tr("Korean common key (optional):"), m_korean_common);
  form->addRow(tr("SD key (optional):"), m_sd);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &WiiKeysDialog::Save);
  connect(buttons, &QDialogButtonBox::rejected, this, &WiiKeysDialog::reject);

  auto* layout = new QVBoxLayout;
  layout->addWidget(intro);
  layout->addLayout(form);
  layout->addWidget(where);
  layout->addWidget(m_error);
  layout->addWidget(buttons);
  setLayout(layout);
}

void WiiKeysDialog::Save()
{
  IOSC::UserKeys keys = IOSC::ReadUserKeys();
  keys.common = m_common->text().trimmed().toStdString();
  keys.korean_common = m_korean_common->text().trimmed().toStdString();
  keys.sd = m_sd->text().trimmed().toStdString();
  if (!IOSC::IsValidKey(keys.common))
  {
    m_error->setText(tr("The common key must be 32 hex digits."));
    m_error->show();
    return;
  }
  if ((!keys.korean_common.empty() && !IOSC::IsValidKey(keys.korean_common)) ||
      (!keys.sd.empty() && !IOSC::IsValidKey(keys.sd)))
  {
    m_error->setText(tr("Each key must be 32 hex digits, or left empty."));
    m_error->show();
    return;
  }
  if (!IOSC::SaveUserKeys(keys))
  {
    m_error->setText(tr("The keys could not be saved."));
    m_error->show();
    return;
  }
  accept();
}
