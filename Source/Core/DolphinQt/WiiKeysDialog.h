// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QWidget;
struct BootParameters;

// This build ships no Wii common keys or SD key (IOS::HLE::IOSC::GetUserKeysPath). Before a Wii
// disc or WAD boots without them, the player is told why it cannot run and asked to enter their
// own keys, which are saved to User/Wii/wii_keys.txt.
class WiiKeysDialog final : public QDialog
{
  Q_OBJECT

public:
  // True when `parameters` boots a Wii disc or WAD and the common key is missing.
  static bool NeedsKeys(const BootParameters& parameters);
  // Warns and offers the dialog. The game does not boot this time: after the keys are saved the
  // player starts it again, so the disc is read with them.
  static void WarnMissingKeys(QWidget* parent);

  explicit WiiKeysDialog(QWidget* parent = nullptr);

private:
  void Save();

  QLineEdit* m_common;
  QLineEdit* m_korean_common;
  QLineEdit* m_sd;
  QLabel* m_error;
};
