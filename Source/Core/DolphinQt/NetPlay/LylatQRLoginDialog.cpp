// Copyright 2026 Project Lylat
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/LylatQRLoginDialog.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include <QDialogButtonBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <qrcodegen.h>

#include "Core/Lylat/LylatDeviceLogin.h"

namespace
{
constexpr int QR_PIXELS = 240;
}

LylatQRLoginDialog::LylatQRLoginDialog(QWidget* parent)
    : QDialog(parent), m_login(std::make_unique<Lylat::DeviceLogin>())
{
  setWindowTitle(tr("Sign in to Lylat"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_qr_label = new QLabel;
  m_qr_label->setFixedSize(QR_PIXELS, QR_PIXELS);
  m_qr_label->setAlignment(Qt::AlignCenter);

  m_code_label = new QLabel;
  m_code_label->setAlignment(Qt::AlignCenter);
  m_code_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  QFont code_font = m_code_label->font();
  code_font.setPointSizeF(code_font.pointSizeF() * 1.6);
  code_font.setBold(true);
  code_font.setStyleHint(QFont::Monospace);
  m_code_label->setFont(code_font);

  m_number_label = new QLabel;
  m_number_label->setAlignment(Qt::AlignCenter);

  m_title_label = new QLabel;
  QFont title_font = m_title_label->font();
  title_font.setPointSizeF(title_font.pointSizeF() * 1.3);
  title_font.setBold(true);
  m_title_label->setFont(title_font);
  m_title_label->setWordWrap(true);

  m_detail_label = new QLabel;
  m_detail_label->setWordWrap(true);

  m_url_label = new QLabel;
  m_url_label->setOpenExternalLinks(true);
  m_url_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_url_label->setWordWrap(true);

  m_expiry_label = new QLabel;

  auto* left = new QVBoxLayout;
  left->addWidget(m_qr_label, 0, Qt::AlignHCenter);
  left->addWidget(m_code_label);
  left->addWidget(m_number_label);
  left->addStretch();

  auto* right = new QVBoxLayout;
  right->addWidget(m_title_label);
  right->addWidget(m_detail_label);
  right->addWidget(m_url_label);
  right->addWidget(m_expiry_label);
  right->addStretch();

  auto* body = new QHBoxLayout;
  body->addLayout(left);
  body->addSpacing(12);
  body->addLayout(right, 1);

  auto* buttons = new QDialogButtonBox;
  m_retry_button = buttons->addButton(tr("Retry"), QDialogButtonBox::ActionRole);
  m_manual_button = buttons->addButton(tr("Manual login"), QDialogButtonBox::ActionRole);
  m_cancel_button = buttons->addButton(QDialogButtonBox::Cancel);

  auto* layout = new QVBoxLayout;
  layout->addLayout(body);
  layout->addWidget(buttons);
  setLayout(layout);
  setMinimumWidth(620);

  connect(m_retry_button, &QPushButton::clicked, this, [this] {
    m_shown_serial = 0;
    m_last_state = -1;
    m_login->Start();
    Poll();
  });
  connect(m_manual_button, &QPushButton::clicked, this, [this] {
    m_login->Cancel();
    done(ManualLoginRequested);
  });
  connect(m_cancel_button, &QPushButton::clicked, this, [this] {
    m_login->Cancel();
    reject();
  });

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &LylatQRLoginDialog::Poll);
  m_timer->start(200);

  m_login->Start();
  Poll();
}

LylatQRLoginDialog::~LylatQRLoginDialog()
{
  m_login->Cancel();
}

void LylatQRLoginDialog::SetQRCode(const QString& text)
{
  const QByteArray utf8 = text.toUtf8();
  std::vector<uint8_t> qr(qrcodegen_BUFFER_LEN_MAX);
  std::vector<uint8_t> temp(qrcodegen_BUFFER_LEN_MAX);
  if (!qrcodegen_encodeText(utf8.constData(), temp.data(), qr.data(), qrcodegen_Ecc_MEDIUM,
                            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO,
                            true))
  {
    m_qr_label->clear();
    return;
  }
  const int size = qrcodegen_getSize(qr.data());
  constexpr int quiet = 4;
  const int total = size + quiet * 2;
  QImage image(total, total, QImage::Format_RGB32);
  image.fill(Qt::white);
  for (int y = 0; y < size; ++y)
  {
    for (int x = 0; x < size; ++x)
    {
      if (qrcodegen_getModule(qr.data(), x, y))
        image.setPixel(x + quiet, y + quiet, qRgb(0, 0, 0));
    }
  }
  // Whole pixels per module keep the code sharp.
  const int scale = std::max(1, QR_PIXELS / total);
  m_qr_label->setPixmap(QPixmap::fromImage(
      image.scaled(total * scale, total * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
}

void LylatQRLoginDialog::ShowMessage(const QString& title, const QString& detail, bool retry)
{
  m_title_label->setText(title);
  m_detail_label->setText(detail);
  m_retry_button->setVisible(retry);
  if (retry)
    m_retry_button->setDefault(true);
}

void LylatQRLoginDialog::Poll()
{
  using State = Lylat::DeviceLoginState;
  const Lylat::DeviceLoginView view = m_login->GetView();

  if (view.state == State::Showing || view.state == State::Approved)
  {
    if (view.serial != m_shown_serial)
    {
      m_shown_serial = view.serial;
      const QString complete = QString::fromStdString(view.verification_uri_complete);
      SetQRCode(complete);
      m_code_label->setText(QString::fromStdString(view.user_code));
      m_number_label->setText(
          tr("Pick <b>%1</b> on your phone").arg(view.match_number, 2, 10, QLatin1Char('0')));
      const QString uri = QString::fromStdString(
          view.verification_uri.empty() ? view.verification_uri_complete : view.verification_uri);
      m_url_label->setText(tr("Or open <a href=\"%1\">%2</a> and enter the code.")
                               .arg(complete.toHtmlEscaped(), uri.toHtmlEscaped()));
    }
  }
  else
  {
    m_shown_serial = 0;
    m_qr_label->clear();
    m_code_label->clear();
    m_number_label->clear();
    m_url_label->clear();
  }

  if (view.state == State::Showing)
  {
    const auto left = std::chrono::duration_cast<std::chrono::seconds>(
                          view.deadline - std::chrono::steady_clock::now())
                          .count();
    const long long secs = std::max<long long>(0, left);
    m_expiry_label->setText(
        tr("Expires in %1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0')));
  }
  else
  {
    m_expiry_label->clear();
  }

  if (static_cast<int>(view.state) == m_last_state && view.state != State::LoggedIn)
    return;
  m_last_state = static_cast<int>(view.state);

  switch (view.state)
  {
  case State::Idle:
  case State::Loading:
    ShowMessage(tr("Getting a code..."), tr("Hang on."), false);
    break;
  case State::Showing:
    ShowMessage(tr("Scan with your phone and approve"),
                tr("Scan the QR code with your phone, sign in to Lylat if asked, then pick the "
                   "number shown here."),
                false);
    break;
  case State::Approved:
    ShowMessage(tr("Approved!"), tr("Signing you in..."), false);
    break;
  case State::LoggedIn:
    if (m_logged_in_ticks++ == 0)
    {
      ShowMessage(tr("Logged in"), tr("You are signed in."), false);
      m_manual_button->setVisible(false);
      m_cancel_button->setText(tr("Close"));
    }
    // About 1.5 s on "Logged in", then close.
    if (m_logged_in_ticks > 8)
    {
      m_timer->stop();
      accept();
    }
    break;
  case State::Expired:
    ShowMessage(tr("Code expired"), tr("That code is no longer valid."), true);
    break;
  case State::Denied:
    ShowMessage(tr("Sign-in denied"), tr("The request was not approved."), true);
    break;
  case State::Error:
    ShowMessage(tr("Something went wrong"), QString::fromStdString(view.error), true);
    break;
  case State::Unsupported:
    // No device-code endpoint on this server: fall back to the browser flow.
    m_timer->stop();
    done(ManualLoginRequested);
    break;
  }
}
