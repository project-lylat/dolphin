//
// Created by Robert Peralta on 4/26/22.
//

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "Common/ENetUtil.h"
#include "Common/StringUtil.h"
#include "Common/TraversalClient.h"
#include "Common/Version.h"

#include "LylatUser.h"
#include "picojson.h"

#include <random>

namespace UICommon
{
class GameFile;
}

class MmMessageType
{
public:
  static std::string CREATE_TICKET;
  static std::string CREATE_TICKET_RESP;
  static std::string GET_TICKET_RESP;
};

class LylatMatchmakingClient
{

  enum OnlinePlayMode
  {
    RANKED = 0,
    UNRANKED = 1,
    DIRECT = 2,
    TEAMS = 3,
  };

  enum ProcessState
  {
    IDLE,
    INITIALIZING,
    MATCHMAKING,
    OPPONENT_CONNECTING,
    CONNECTION_SUCCESS,
    ERROR_ENCOUNTERED,
  };

  struct MatchSearchSettings
  {
    OnlinePlayMode mode = OnlinePlayMode::RANKED;
    std::string connectCode = "";
  };

public:
  LylatMatchmakingClient();
  ~LylatMatchmakingClient();

  static LylatMatchmakingClient* GetClient();

  void Match(const UICommon::GameFile& game, std::string traversalRoomId,
             std::function<void(const UICommon::GameFile& game, bool isHost, std::string ip,
                                unsigned short port, unsigned short localPort)>
                 onSuccessCallback,
             std::function<void(const UICommon::GameFile&, std::string)> onFailureCallback);
  void MatchmakeThread();
  void CancelSearch();
  bool IsSearching();

  std::string m_errorMsg = "";

protected:
  static LylatMatchmakingClient* singleton;
  const std::string MM_HOST = "lylat.gg";
//  const std::string MM_HOST = "localhost";
  const u16 MM_PORT = 43113;

  ENetHost* m_client;
  ENetPeer* m_server;
  const UICommon::GameFile* m_game;
  std::string m_traversal_room_id;

  std::default_random_engine generator;

  bool isMmConnected = false;

  std::thread m_search_thread;

  MatchSearchSettings m_searchSettings;

  ProcessState m_state;
  std::vector<std::string> m_remoteIps;
  std::vector<LylatUser> m_playerInfo;
  std::vector<u16> m_allowedStages;
  LylatUser* m_user;

  int m_isSwapAttempt = false;
  int m_hostPort;
  int m_localPlayerIndex;
  bool m_joinedLobby;
  bool m_isHost;

  const std::unordered_map<ProcessState, bool> searchingStates = {
      {ProcessState::INITIALIZING, true},
      {ProcessState::MATCHMAKING, true},
      {ProcessState::OPPONENT_CONNECTING, true},
  };

  std::function<void(const UICommon::GameFile&, bool, std::string, unsigned short, unsigned short)>
      m_onSuccessCallback;
  std::function<void(const UICommon::GameFile&, std::string)> m_onFailureCallback;

  void disconnectFromServer();
  void terminateMmConnection();
  void sendMessage(picojson::object msg);
  int receiveMessage(picojson::value& msg, int timeoutMs);

  void sendHolePunchMsg(std::string remoteIp, u16 remotePort, u16 localPort);

  void startMatchmaking();
  void handleMatchmaking();
  void handleConnecting();
};
