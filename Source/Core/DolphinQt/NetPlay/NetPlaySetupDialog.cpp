// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>

#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"
#include "DolphinQt/Settings.h"

#include <QDropEvent>
#include <QMimeData>
#include "Common/FileUtil.h"
#include "UICommon/GameFile.h"
#include "UICommon/NetPlayIndex.h"

static std::string SIGN_IN_STR =
    "SIGN IN:<br /><br />"
    "Click on the Sign In Button Above to login.<br /><br />"
    "If that doesn't work, <a href=\"https://lylat.gg/users/enable\">Click "
    "Here</a> or open your browser at <a "
    "href=\"https://lylat.gg/users/enable\">https://lylat.gg/"
    "users/enable</a> and follow the steps to sign in. <br /><br /><br /> "
    "AFTER YOU HAVE DOWNLOADED YOUR \"lylat.json\" FILE, DRAG AND DROP IT HERE OR <br />"
    "CLICK THE \"Attach lylat.json\" BUTTON TO FINISH SET UP!";

void LylatWidget::dragEnterEvent(QDragEnterEvent* event)
{
  setBackgroundRole(QPalette::Highlight);
  m_netplay_setup_dialog->m_sign_in_label->setText(tr("Drop your lylat.json here!"));

  if (event->mimeData()->hasText())
    event->acceptProposedAction();
}

void LylatWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
  setBackgroundRole(QPalette::Base);
  m_netplay_setup_dialog->m_sign_in_label->setText(tr(SIGN_IN_STR.c_str()));
  event->accept();
}

void LylatWidget::dropEvent(QDropEvent* event)
{
  m_netplay_setup_dialog->m_sign_in_label->setText(tr(SIGN_IN_STR.c_str()));

  const QMimeData* mimeData = event->mimeData();
  if (!mimeData->hasText())
    return;

  if (mimeData->text().isEmpty())
    return;

  auto jsonPath = mimeData->text().toStdString();
#ifdef _WIN32
  jsonPath = ReplaceAll(std::move(jsonPath), "file:///", "");
#else
  jsonPath = ReplaceAll(std::move(jsonPath), "file://", "");
#endif
  auto user = LylatUser::GetUserFromDisk(jsonPath);
  if (!user)
    return;

  File::Copy(jsonPath, LylatUser::GetFilePath());
  m_netplay_setup_dialog->Refresh();
}

NetPlaySetupDialog::NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model)
{
  setWindowTitle(tr("NetPlay Setup"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();

  bool use_index = Config::Get(Config::NETPLAY_USE_INDEX);
  std::string index_region = Config::Get(Config::NETPLAY_INDEX_REGION);
  std::string index_name = Config::Get(Config::NETPLAY_INDEX_NAME);
  std::string index_password = Config::Get(Config::NETPLAY_INDEX_PASSWORD);
  std::string nickname = Config::Get(Config::NETPLAY_NICKNAME);
  std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  int connect_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  int host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  int host_listen_port = Config::Get(Config::NETPLAY_LISTEN_PORT);
  bool enable_chunked_upload_limit = Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT);
  u32 chunked_upload_limit = Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT);
#ifdef USE_UPNP
  bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  m_host_upnp->setChecked(use_upnp);
#endif

  m_nickname_edit->setText(QString::fromStdString(nickname));
  m_connection_type->setCurrentIndex(TraversalChoiceReversedMap[traversal_choice]);
  m_connect_port_box->setValue(connect_port);
  m_host_port_box->setValue(host_port);

  m_host_force_port_box->setValue(host_listen_port);
  m_host_force_port_box->setEnabled(false);

  m_host_server_browser->setChecked(use_index);

  m_host_server_region->setEnabled(use_index);
  m_host_server_region->setCurrentIndex(
      m_host_server_region->findData(QString::fromStdString(index_region)));

  m_host_server_name->setEnabled(use_index);
  m_host_server_name->setText(QString::fromStdString(index_name));

  m_host_server_password->setEnabled(use_index);
  m_host_server_password->setText(QString::fromStdString(index_password));

  m_host_chunked_upload_limit_check->setChecked(enable_chunked_upload_limit);
  m_host_chunked_upload_limit_box->setValue(chunked_upload_limit);
  m_host_chunked_upload_limit_box->setEnabled(enable_chunked_upload_limit);

  OnConnectionTypeChanged(m_connection_type->currentIndex());

  ConnectWidgets();
}

void NetPlaySetupDialog::CreateMainLayout()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  m_nickname_edit = new QLineEdit;
  m_connection_type = new QComboBox;
  m_reset_traversal_button = new NonDefaultQPushButton(tr("Reset Traversal Settings"));
  m_lylat_toggle_login_button = new NonDefaultQPushButton(tr("Sign In"));
  m_lylat_reload_button = new NonDefaultQPushButton(tr("Refresh"));
  m_tab_widget = new QTabWidget;

  m_nickname_edit->setValidator(
      new UTF8CodePointCountValidator(NetPlay::MAX_NAME_LENGTH, m_nickname_edit));

  auto alert_label = new QLabel(
      tr("ALERT:\n\n"
         "All players must use the same Dolphin version.\n"
         "If enabled, SD cards must be identical between players.\n"
         "If DSP LLE is used, DSP ROMs must be identical between players.\n"
         "If a game is hanging on boot, it may not support Dual Core Netplay."
         " Disable Dual Core.\n"
         "If connecting directly, the host must have the chosen UDP port open/forwarded!\n"
         "\n"
         "Wii Remote support in netplay is experimental and may not work correctly.\n"
         "Use at your own risk.\n"));

  // Connection widget
  auto* connection_widget = new QWidget;
  auto* connection_layout = new QGridLayout;

  m_ip_label = new QLabel;
  m_ip_edit = new QLineEdit;
  m_connect_port_label = new QLabel(tr("Port:"));
  m_connect_port_box = new QSpinBox;
  m_connect_button = new NonDefaultQPushButton(tr("Connect"));

  m_connect_port_box->setMaximum(65535);

  connection_layout->addWidget(m_ip_label, 0, 0);
  connection_layout->addWidget(m_ip_edit, 0, 1);
  connection_layout->addWidget(m_connect_port_label, 0, 2);
  connection_layout->addWidget(m_connect_port_box, 0, 3);
  connection_layout->addWidget(alert_label, 1, 0, -1, -1);
  connection_layout->addWidget(m_connect_button, 3, 3, Qt::AlignRight);

  connection_widget->setLayout(connection_layout);

  // Host widget
  auto* host_widget = new QWidget;
  auto* host_layout = new QGridLayout;
  m_host_port_label = new QLabel(tr("Port:"));
  m_host_port_box = new QSpinBox;
  m_host_force_port_check = new QCheckBox(tr("Force Listen Port:"));
  m_host_force_port_box = new QSpinBox;
  m_host_chunked_upload_limit_check = new QCheckBox(tr("Limit Chunked Upload Speed:"));
  m_host_chunked_upload_limit_box = new QSpinBox;
  m_host_server_browser = new QCheckBox(tr("Show in server browser"));
  m_host_server_name = new QLineEdit;
  m_host_server_password = new QLineEdit;
  m_host_server_region = new QComboBox;

#ifdef USE_UPNP
  m_host_upnp = new QCheckBox(tr("Forward port (UPnP)"));
#endif
  m_host_games = new QListWidget;
  m_lylat_games = new QListWidget;
  m_host_button = new NonDefaultQPushButton(tr("Host"));

  m_host_port_box->setMaximum(65535);
  m_host_force_port_box->setMaximum(65535);
  m_host_chunked_upload_limit_box->setRange(1, 1000000);
  m_host_chunked_upload_limit_box->setSingleStep(100);
  m_host_chunked_upload_limit_box->setSuffix(QStringLiteral(" kbps"));

  m_host_chunked_upload_limit_check->setToolTip(tr(
      "This will limit the speed of chunked uploading per client, which is used for save sync."));

  m_host_server_name->setToolTip(tr("Name of your session shown in the server browser"));
  m_host_server_name->setPlaceholderText(tr("Name"));
  m_host_server_password->setToolTip(tr("Password for joining your game (leave empty for none)"));
  m_host_server_password->setPlaceholderText(tr("Password"));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_host_server_region->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

  host_layout->addWidget(m_host_port_label, 0, 0);
  host_layout->addWidget(m_host_port_box, 0, 1);
#ifdef USE_UPNP
  host_layout->addWidget(m_host_upnp, 0, 2);
#endif
  host_layout->addWidget(m_host_server_browser, 1, 0);
  host_layout->addWidget(m_host_server_region, 1, 1);
  host_layout->addWidget(m_host_server_name, 1, 2);
  host_layout->addWidget(m_host_server_password, 1, 3);
  host_layout->addWidget(m_host_games, 2, 0, 1, -1);
  host_layout->addWidget(m_host_force_port_check, 3, 0);
  host_layout->addWidget(m_host_force_port_box, 3, 1, Qt::AlignLeft);
  host_layout->addWidget(m_host_chunked_upload_limit_check, 4, 0);
  host_layout->addWidget(m_host_chunked_upload_limit_box, 4, 1, Qt::AlignLeft);
  host_layout->addWidget(m_host_button, 4, 3, 2, 1, Qt::AlignRight);

  host_widget->setLayout(host_layout);

  // Lylat Sign In Widget
  m_lylat_widget = new LylatWidget;
  m_lylat_sign_in_widget = new QWidget;
  m_lylat_connect_widget = new QWidget;
  auto lylat_layout = new QGridLayout;
  auto lylat_sign_in_layout = new QGridLayout;
  auto lylat_connect_layout = new QGridLayout;
  m_lylat_attach_json_button = new NonDefaultQPushButton(tr("Attach lylat.json"));

  m_lylat_widget->m_netplay_setup_dialog = this;
  m_lylat_widget->setAcceptDrops(true);
  m_lylat_widget->autoFillBackground();

  m_sign_in_label = new QLabel(tr(SIGN_IN_STR.c_str()));

  m_sign_in_label->setTextFormat(Qt::RichText);
  m_sign_in_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_sign_in_label->setOpenExternalLinks(true);

  // Lylat Sign In Layout
  lylat_sign_in_layout->addWidget(m_sign_in_label, 0, 0, 4, -1);
  lylat_sign_in_layout->addWidget(m_lylat_attach_json_button, 5, 0, 1, -1);
  // Lylat Connect Layout
  m_lylat_connect_button = new NonDefaultQPushButton(tr("Connect"));

  lylat_connect_layout->addWidget(
      new QLabel(tr("Choose the game you want to start playing and then click on Connect button")),
      0, 0, 2, -1);
  lylat_connect_layout->addWidget(m_lylat_games, 2, 0, 1, -1);
  lylat_connect_layout->addWidget(m_lylat_connect_button, 4, 3, 2, 1, Qt::AlignRight);

  lylat_layout->addWidget(m_lylat_sign_in_widget, 0, 0, -1, -1);
  lylat_layout->addWidget(m_lylat_connect_widget, 0, 0, -1, -1);

  connection_widget->setVisible(false);

  m_lylat_sign_in_widget->setLayout(lylat_sign_in_layout);
  m_lylat_connect_widget->setLayout(lylat_connect_layout);
  m_lylat_widget->setLayout(lylat_layout);

  // Setup Tabs
  m_connection_type->addItem(tr("Lylat"));
  m_connection_type->addItem(tr("Direct Connection"));
  m_connection_type->addItem(tr("Traversal Server"));

  m_main_layout->addWidget(new QLabel(tr("Connection Type:")), 0, 0);
  m_main_layout->addWidget(m_connection_type, 0, 1);
  m_main_layout->addWidget(m_reset_traversal_button, 0, 2);
  m_main_layout->addWidget(m_lylat_toggle_login_button, 0, 2);
  m_main_layout->addWidget(m_lylat_reload_button, 1, 2);
  m_main_layout->addWidget(new QLabel(tr("Nickname:")), 1, 0);
  m_main_layout->addWidget(m_nickname_edit, 1, 1);
  m_main_layout->addWidget(m_tab_widget, 2, 0, 1, -1);
  m_main_layout->addWidget(m_button_box, 3, 0, 1, -1);

  // Tabs
  m_tab_widget->addTab(connection_widget, tr("Connect"));
  m_tab_widget->addTab(host_widget, tr("Host"));
  m_tab_widget->addTab(m_lylat_widget, tr("Lylat"));

  setLayout(m_main_layout);
}

void NetPlaySetupDialog::ConnectWidgets()
{
  connect(m_connection_type, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::OnConnectionTypeChanged);
  connect(m_nickname_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);

  // Connect widget
  connect(m_ip_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_connect_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  // Host widget
  connect(m_host_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  connect(m_host_games, qOverload<int>(&QListWidget::currentRowChanged), [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                      m_host_games->item(index)->text());
  });
  connect(m_host_games, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);

  connect(m_host_force_port_check, &QCheckBox::toggled,
          [this](bool value) { m_host_force_port_box->setEnabled(value); });
  connect(m_host_chunked_upload_limit_check, &QCheckBox::toggled, this, [this](bool value) {
    m_host_chunked_upload_limit_box->setEnabled(value);
    SaveSettings();
  });
  connect(m_host_chunked_upload_limit_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);

  connect(m_host_server_browser, &QCheckBox::toggled, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_password, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_region,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::SaveSettings);

#ifdef USE_UPNP
  connect(m_host_upnp, &QCheckBox::stateChanged, this, &NetPlaySetupDialog::SaveSettings);
#endif
  // Lylat Widget
  connect(m_lylat_games, qOverload<int>(&QListWidget::currentRowChanged), [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/lylatgame"),
                                      m_host_games->item(index)->text());
  });
  connect(m_lylat_games, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);
  connect(m_lylat_toggle_login_button, &QPushButton::clicked, this, [this]() {
    if (LylatUser::GetUser(true))
    {
      LylatUser::DeleteUserFile();
      this->OnConnectionTypeChanged(this->m_connection_type->currentIndex());
    }
    else
    {
      QDesktopServices::openUrl(QUrl(tr("http://lylat.gg/users/enable"), QUrl::TolerantMode));
    }
  });

  connect(m_lylat_attach_json_button, &QPushButton::clicked, this,
          [this]() { emit OpenLylatJSON(std::nullopt); });

  connect(m_lylat_reload_button, &QPushButton::clicked, this,
          [this]() { this->OnConnectionTypeChanged(this->m_connection_type->currentIndex()); });
  connect(m_lylat_connect_button, &QPushButton::clicked, this, &QDialog::accept);

  connect(m_connect_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_host_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_reset_traversal_button, &QPushButton::clicked, this,
          &NetPlaySetupDialog::ResetTraversalHost);
  connect(m_host_server_browser, &QCheckBox::toggled, this, [this](bool value) {
    m_host_server_region->setEnabled(value);
    m_host_server_name->setEnabled(value);
    m_host_server_password->setEnabled(value);
  });
}

void NetPlaySetupDialog::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, m_nickname_edit->text().toStdString());
  Config::SetBaseOrCurrent(m_connection_type->currentIndex() == CONN_TYPE_DIRECT ?
                               Config::NETPLAY_ADDRESS :
                               Config::NETPLAY_HOST_CODE,
                           m_ip_edit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT,
                           static_cast<u16>(m_connect_port_box->value()));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_PORT, static_cast<u16>(m_host_port_box->value()));
#ifdef USE_UPNP
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_UPNP, m_host_upnp->isChecked());
#endif

  if (m_host_force_port_check->isChecked())
    Config::SetBaseOrCurrent(Config::NETPLAY_LISTEN_PORT,
                             static_cast<u16>(m_host_force_port_box->value()));

  Config::SetBaseOrCurrent(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_check->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_box->value());

  Config::SetBaseOrCurrent(Config::NETPLAY_USE_INDEX, m_host_server_browser->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION,
                           m_host_server_region->currentData().toString().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, m_host_server_name->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD,
                           m_host_server_password->text().toStdString());
}

enum TabIndexes : int
{
  TAB_CONNECT,
  TAB_HOST,
  TAB_LYLAT,
};

void NetPlaySetupDialog::SetConnectionType(ConnectionType type)
{
  m_connection_type->setCurrentIndex(type);
}

void NetPlaySetupDialog::Refresh()
{
  OnConnectionTypeChanged(m_connection_type->currentIndex());
}

void NetPlaySetupDialog::OnConnectionTypeChanged(int index)
{
  ConnectionType type = (ConnectionType)index;
  std::string address;

  m_ip_label->setText(index == CONN_TYPE_DIRECT ? tr("IP Address:") : tr("Host Code:"));
  m_nickname_edit->setEnabled(true);  // Enable Nickname editing

  m_connect_port_box->setHidden(index != CONN_TYPE_DIRECT);
  m_connect_port_label->setHidden(index != CONN_TYPE_DIRECT);

  m_host_port_label->setHidden(index != CONN_TYPE_DIRECT);
  m_host_port_box->setHidden(index != CONN_TYPE_DIRECT);
#ifdef USE_UPNP
  m_host_upnp->setHidden(index != CONN_TYPE_DIRECT);
#endif
  m_host_force_port_check->setHidden(index == CONN_TYPE_DIRECT);
  m_host_force_port_box->setHidden(index == CONN_TYPE_DIRECT);

  m_reset_traversal_button->setHidden(index != CONN_TYPE_TRAVERSAL);
  m_lylat_toggle_login_button->setHidden(index != CONN_TYPE_LYLAT);
  m_lylat_reload_button->setHidden(index != CONN_TYPE_LYLAT);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)

  m_tab_widget->setTabEnabled(TAB_CONNECT, type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_CONNECT)->setVisible(type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_CONNECT)->setEnabled(type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_CONNECT)->setHidden(type == CONN_TYPE_LYLAT);

  m_tab_widget->setTabEnabled(TAB_HOST, type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_HOST)->setVisible(type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_HOST)->setEnabled(type != CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_HOST)->setHidden(type == CONN_TYPE_LYLAT);

  m_tab_widget->setTabEnabled(TAB_LYLAT, type == CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_LYLAT)->setVisible(type == CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_LYLAT)->setEnabled(type == CONN_TYPE_LYLAT);
  m_tab_widget->widget(TAB_LYLAT)->setHidden(type != CONN_TYPE_LYLAT);
#else
  m_tab_widget->setTabVisible(TAB_CONNECT, type != CONN_TYPE_LYLAT);
  m_tab_widget->setTabVisible(TAB_HOST, type != CONN_TYPE_LYLAT);
  m_tab_widget->setTabVisible(TAB_LYLAT, type == CONN_TYPE_LYLAT);
#endif
  if(type == CONN_TYPE_LYLAT)
  {
    m_tab_widget->setCurrentIndex(TAB_LYLAT);
    auto current_widget = m_tab_widget->widget(TAB_LYLAT);
    if(current_widget) m_tab_widget->setCurrentWidget(current_widget);
  }

  switch (type)
  {
  case CONN_TYPE_LYLAT:
    m_lylat_user = LylatUser::GetUser(true);  // Get Lylat User
    m_nickname_edit->setEnabled(false);       // Disable Nickname editing
    m_tab_widget->tabBarClicked(TAB_LYLAT);

    // Before switching layouts the current LayoutManager needs to be deleted.

    if (m_lylat_user)
    {
      m_lylat_toggle_login_button->setText(tr("Sign Out"));
      m_tab_widget->setTabText(TAB_LYLAT, tr("Connect"));
      m_nickname_edit->setText(tr(m_lylat_user->displayName.c_str()));
      m_lylat_sign_in_widget->setVisible(false);
      m_lylat_connect_widget->setVisible(true);
    }
    else
    {
      m_lylat_toggle_login_button->setText(tr("Sign In"));
      m_tab_widget->setTabText(TAB_LYLAT, tr("Sign in"));
      m_lylat_sign_in_widget->setVisible(true);
      m_lylat_connect_widget->setVisible(false);
    }

    break;
  case CONN_TYPE_DIRECT:
    address = Config::Get(Config::NETPLAY_ADDRESS);
    m_ip_label->setText(tr("IP Address:"));

    break;
  case CONN_TYPE_TRAVERSAL:
    address = Config::Get(Config::NETPLAY_HOST_CODE);
    m_ip_label->setText(tr("Host Code:"));
    break;
  default:
    break;
  }
  m_ip_edit->setText(QString::fromStdString(address));
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE,
                           std::string(TraversalChoiceMap[(ConnectionType)index]));
}

void NetPlaySetupDialog::show()
{
  PopulateGameList(
      m_host_games,
      Settings::GetQSettings().value(QStringLiteral("netplay/hostgame"), QString{}).toString());
  PopulateGameList(
      m_lylat_games,
      Settings::GetQSettings().value(QStringLiteral("netplay/lylatgame"), QString{}).toString());
  QDialog::show();
}

void NetPlaySetupDialog::accept()
{
  SaveSettings();
  switch (m_tab_widget->currentIndex())
  {
  case TAB_CONNECT:
  {
    emit Join();
    break;
  }
  case TAB_HOST:
  {
    auto items = m_host_games->selectedItems();
    if (items.empty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must select a game to host!"));
      return;
    }

    if (m_host_server_browser->isChecked() && m_host_server_name->text().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must provide a name for your session!"));
      return;
    }

    if (m_host_server_browser->isChecked() &&
        m_host_server_region->currentData().toString().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("You must provide a region for your session!"));
      return;
    }

    emit Host(*items[0]->data(Qt::UserRole).value<std::shared_ptr<const UICommon::GameFile>>());
    break;
  }
  case TAB_LYLAT:
  {
    auto games = m_lylat_games->selectedItems();
    if (games.empty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must select a game to find a match!"));
      return;
    }

    emit Search(*games[0]->data(Qt::UserRole).value<std::shared_ptr<const UICommon::GameFile>>());
    break;
  }
  default:
  {
    break;
  }
  }
}

void NetPlaySetupDialog::PopulateGameList(QListWidget* list, QString selected_game)
{
  QSignalBlocker blocker(list);

  list->clear();
  for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list_model.GetGameFile(i);

    auto* item =
        new QListWidgetItem(QString::fromStdString(m_game_list_model.GetNetPlayName(*game)));
    item->setData(Qt::UserRole, QVariant::fromValue(std::move(game)));
    list->addItem(item);
  }

  list->sortItems();

  auto find_list = list->findItems(selected_game, Qt::MatchFlag::MatchExactly);

  if (find_list.count() > 0)
    list->setCurrentItem(find_list[0]);
}

void NetPlaySetupDialog::ResetTraversalHost()
{
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_SERVER,
                           Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue());
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_PORT,
                           Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue());

  ModalMessageBox::information(
      this, tr("Reset Traversal Server"),
      tr("Reset Traversal Server to %1:%2")
          .arg(QString::fromStdString(Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue()),
               QString::number(Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue())));
}
