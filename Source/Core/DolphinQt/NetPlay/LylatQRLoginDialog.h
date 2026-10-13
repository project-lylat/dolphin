// Copyright 2026 Project Lylat
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QDialog>

#include "Common/CommonTypes.h"

class QLabel;
class QPushButton;
class QTimer;

namespace Lylat
{
class DeviceLogin;
}

// Sign in to Lylat by scanning a QR code with a phone (device-code flow).
// Result: Accepted when lylat.json was written; ManualLoginRequested when the
// player picked "Manual login" or the server has no device-code endpoint.
class LylatQRLoginDialog final : public QDialog
{
  Q_OBJECT
public:
  enum Result
  {
    ManualLoginRequested = QDialog::Accepted + 1,
  };

  explicit LylatQRLoginDialog(QWidget* parent = nullptr);
  ~LylatQRLoginDialog() override;

private:
  void Poll();
  void SetQRCode(const QString& text);
  void ShowMessage(const QString& title, const QString& detail, bool retry);

  std::unique_ptr<Lylat::DeviceLogin> m_login;
  QTimer* m_timer;
  QLabel* m_qr_label;
  QLabel* m_code_label;
  QLabel* m_number_label;
  QLabel* m_title_label;
  QLabel* m_detail_label;
  QLabel* m_url_label;
  QLabel* m_expiry_label;
  QPushButton* m_retry_button;
  QPushButton* m_manual_button;
  QPushButton* m_cancel_button;
  u32 m_shown_serial = 0;
  int m_last_state = -1;
  int m_logged_in_ticks = 0;
};
