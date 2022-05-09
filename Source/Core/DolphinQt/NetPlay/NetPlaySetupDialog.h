// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QLayout>
#include <QtWebView/QtWebView>

#include "Core/Lylat/LylatUser.h"
#include "DolphinQt/GameList/GameListModel.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QGridLayout;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace UICommon
{
class GameFile;
}

class LylatWidget;

class NetPlaySetupDialog : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent);

  void accept() override;
  void show();

  enum ConnectionType : int
  {
    CONN_TYPE_LYLAT = 0,
    CONN_TYPE_DIRECT = 1,
    CONN_TYPE_TRAVERSAL = 2,
  };

  std::map<ConnectionType, std::string> TraversalChoiceMap{
      {CONN_TYPE_LYLAT, "lylat"},
      {CONN_TYPE_DIRECT, "direct"},
      {CONN_TYPE_TRAVERSAL, "traversal"},
  };

  std::map<std::string, ConnectionType> TraversalChoiceReversedMap{
      {"lylat", CONN_TYPE_LYLAT},
      {"direct", CONN_TYPE_DIRECT},
      {"traversal", CONN_TYPE_TRAVERSAL},
  };

  void Refresh();

signals:
  bool Join();
  bool Search(const UICommon::GameFile& game);
  bool Host(const UICommon::GameFile& game);
  bool OpenLylatJSON(std::optional<std::string> path);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void PopulateGameList(QListWidget* list, QString selected_game);
  void ResetTraversalHost();

  void SaveSettings();

  void OnConnectionTypeChanged(int index);

  // Main Widget
  QDialogButtonBox* m_button_box;
  QComboBox* m_connection_type;
  QLineEdit* m_nickname_edit;
  QGridLayout* m_main_layout;
  QTabWidget* m_tab_widget;
  QPushButton* m_reset_traversal_button;

  // Connection Widget
  QLabel* m_ip_label;
  QLineEdit* m_ip_edit;
  QLabel* m_connect_port_label;
  QSpinBox* m_connect_port_box;
  QPushButton* m_connect_button;

  // Host Widget
  QLabel* m_host_port_label;
  QSpinBox* m_host_port_box;
  QListWidget* m_host_games;
  QPushButton* m_host_button;
  QCheckBox* m_host_force_port_check;
  QSpinBox* m_host_force_port_box;
  QCheckBox* m_host_chunked_upload_limit_check;
  QSpinBox* m_host_chunked_upload_limit_box;
  QCheckBox* m_host_server_browser;
  QLineEdit* m_host_server_name;
  QLineEdit* m_host_server_password;
  QComboBox* m_host_server_region;

  // Lylat Widget
  LylatWidget* m_lylat_widget;
  QWidget* m_lylat_sign_in_widget;
  QWidget* m_lylat_connect_widget;
  QPushButton* m_lylat_toggle_login_button;
  QPushButton* m_lylat_reload_button;
  QPushButton* m_lylat_connect_button;
  QPushButton* m_lylat_attach_json_button;
  LylatUser* m_lylat_user;
  QListWidget* m_lylat_games;
public:
  QLabel* m_sign_in_label;

#ifdef USE_UPNP
  QCheckBox* m_host_upnp;
#endif

  const GameListModel& m_game_list_model;
};

class LylatWidget : public QWidget
{
  Q_OBJECT

  ~LylatWidget() {
    m_netplay_setup_dialog = nullptr;
  }

public:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent* event) override;
  NetPlaySetupDialog* m_netplay_setup_dialog;

};