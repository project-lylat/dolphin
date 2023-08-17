//
// Created by Robert Peralta on 4/26/22.
//
#include "LylatMatchmakingClient.h"
#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "UICommon/GameFile.h"

LylatMatchmakingClient* LylatMatchmakingClient::singleton = nullptr;
std::string MmMessageType::CREATE_TICKET = "create-ticket";
std::string MmMessageType::CREATE_TICKET_RESP = "create-ticket-resp";
std::string MmMessageType::GET_TICKET_RESP = "get-ticket-resp";
static std::mutex search_mutex;

LylatMatchmakingClient::LylatMatchmakingClient()
{
  m_user = LylatUser::GetUser();
  m_state = ProcessState::IDLE;
  m_errorMsg = "";
  m_client = nullptr;
  m_server = nullptr;
  m_game = nullptr;
  generator = std::default_random_engine(Common::Timer::GetTimeMs());
  if (singleton != nullptr)
  {
    delete singleton;
  }
  singleton = this;
}

LylatMatchmakingClient::~LylatMatchmakingClient()
{
  m_state = ProcessState::ERROR_ENCOUNTERED;
  m_errorMsg = "Matchmaking shut down";
  // m_onFailureCallback(*m_game, m_errorMsg);

  if (m_search_thread.joinable())
    m_search_thread.join();

  terminateMmConnection();
}

LylatMatchmakingClient* LylatMatchmakingClient::GetClient()
{
  return singleton == NULL ? new LylatMatchmakingClient() : singleton;
}

void LylatMatchmakingClient::CancelSearch()
{
  std::lock_guard<std::mutex> lk(search_mutex);
  m_state = ProcessState::ERROR_ENCOUNTERED;
  m_errorMsg = "Search Canceled!";
  if (m_onFailureCallback)
  {
    m_onFailureCallback(*m_game, m_errorMsg);
  }
}

void LylatMatchmakingClient::Match(
    const UICommon::GameFile& game, std::string traversalRoomId,
    std::function<void(const UICommon::GameFile& game, bool isHost, std::string ip,
                       unsigned short port, unsigned short local_port, LylatNetplayClient* netplayClient)>
        onSuccessCallback,
    std::function<void(const UICommon::GameFile&, std::string)> onFailureCallback)
{
  std::lock_guard<std::mutex> lk(search_mutex);

  m_game = &game;
  m_traversal_room_id = traversalRoomId;
  m_searchSettings.mode = LylatMatchmakingClient::OnlinePlayMode::UNRANKED;
  m_onSuccessCallback = onSuccessCallback;
  m_onFailureCallback = onFailureCallback;
  m_state = ProcessState::INITIALIZING;
  m_search_thread = std::thread(&LylatMatchmakingClient::MatchmakeThread, this);
}

void LylatMatchmakingClient::MatchmakeThread()
{
  std::cout << "MatchmakeThread::starting"
            << "\n";

  while (IsSearching())
  {
    std::lock_guard<std::mutex> lk(search_mutex);

    std::cout << "MatchmakeThread::running"
              << "\n";

    switch (m_state)
    {
    case ProcessState::INITIALIZING:
      startMatchmaking();
      break;
    case ProcessState::MATCHMAKING:
      handleMatchmaking();
      break;
    case ProcessState::OPPONENT_CONNECTING:
      handleConnecting();
      break;
    }
  }
  std::cout << "MatchmakeThread::finishing..."
            << "\n";

  // Clean up ENET connections
  terminateMmConnection();
  std::cout << "MatchmakeThread::finished"
            << "\n";
}

bool LylatMatchmakingClient::IsSearching()
{
  // std::lock_guard<std::mutex> lk(search_mutex);

  std::cout << "isSearching()"
            << "\n";
  std::cout << "Current STATE: " << m_state << "\n";
  return searchingStates.count(m_state) != 0;
}

void LylatMatchmakingClient::startMatchmaking()
{
  m_client = nullptr;

  int retryCount = 0;
  while (m_client == nullptr && retryCount < 15)
  {
    // TODO: re enable at some point
    bool customPort = false;  // SConfig::GetInstance().m_slippiForceNetplayPort;

    if (customPort)
      m_hostPort = 41000 + (generator() % 10000);  // SConfig::GetInstance().m_slippiNetplayPort;
    else
      m_hostPort = 41000 + (generator() % 10000);
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Port to use: {}...", m_hostPort);

    // We are explicitly setting the client address because we are trying to utilize our connection
    // to the matchmaking service in order to hole punch. This port will end up being the port
    // we listen on when we start our server
    ENetAddress clientAddr;
    clientAddr.host = ENET_HOST_ANY;
    clientAddr.port = m_hostPort;

    m_client = enet_host_create(&clientAddr, 1, 3, 0, 0);
    retryCount++;
  }

  if (m_client == nullptr)
  {
    // Failed to create client
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Failed to create mm client";
    m_onFailureCallback(*m_game, m_errorMsg);
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Failed to create client...");
    return;
  }

  ENetAddress addr;
  auto effectiveHost = MM_HOST;
  WARN_LOG_FMT(LYLAT, "[Matchmaking] HOST: {}", effectiveHost.c_str());

  enet_address_set_host(&addr, effectiveHost.c_str());
  addr.port = MM_PORT;

  m_server = enet_host_connect(m_client, &addr, 3, 0);

  if (m_server == nullptr)
  {
    // Failed to connect to server
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Failed to start connection to mm server";
    m_onFailureCallback(*m_game, m_errorMsg);
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Failed to start connection to mm server...");
    return;
  }

  // Before we can request a ticket, we must wait for connection to be successful
  int connectAttemptCount = 0;
  while (!isMmConnected)
  {
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 500);
    if (net <= 0 || netEvent.type != ENET_EVENT_TYPE_CONNECT)
    {
      // Not yet connected, will retry
      connectAttemptCount++;
      if (connectAttemptCount >= 20)
      {
        WARN_LOG_FMT(LYLAT, "[Matchmaking] Failed to connect to mm server...");
        m_state = ProcessState::ERROR_ENCOUNTERED;
        m_errorMsg = "Failed to connect to mm server";
        m_onFailureCallback(*m_game, m_errorMsg);
        return;
      }

      continue;
    }

    netEvent.peer->data = &m_user->displayName;
    m_client->intercept = ENetUtil::InterceptCallback;
    isMmConnected = true;
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Connected to mm server...");
  }

  WARN_LOG_FMT(LYLAT, "[Matchmaking] Trying to find match...");

  if (!m_user)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Must be logged in to queue");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Must be logged in to queue. Go back to menu";
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  char lanAddr[30] = "";

  char host[256];
  char* IP = (char*)"";
  struct hostent* host_entry;
  int hostname;
  hostname = gethostname(host, sizeof(host));  // find the host name
  if (hostname == -1)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Error finding LAN address");
  }
  else
  {
    host_entry = gethostbyname(host);  // find host information
    if (host_entry == NULL || host_entry->h_addrtype != AF_INET)
    {
      WARN_LOG_FMT(LYLAT, "[Matchmaking] Error finding LAN host");
    }
    else
    {
      // Fetch the last IP (because that was correct for me, not sure if it will be for all)
      int i = 0;
      while (host_entry->h_addr_list[i] != 0)
      {
        IP = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[i]));
        // WARN_LOG(SLIPPI_ONLINE, "[Matchmaking] IP at idx %d: %s", i, IP);
        i++;
      }
      sprintf(lanAddr, "%s:%d", IP, m_hostPort);
    }
  }

  // TODO: re enable
  //  if (SConfig::GetInstance().m_slippiForceLanIp)
  //  {
  //
  //    //WARN_LOG(SLIPPI_ONLINE, "[Matchmaking] Overwriting LAN IP sent with configured address");
  //    sprintf(lanAddr, "%s:%d", SConfig::GetInstance().m_slippiLanIp.c_str(), m_hostPort);
  //  }

  // WARN_LOG(SLIPPI_ONLINE, "[Matchmaking] Sending LAN address: %s", lanAddr);

  std::vector<u8> connectCodeBuf;

  connectCodeBuf.insert(connectCodeBuf.end(), m_searchSettings.connectCode.begin(),
                        m_searchSettings.connectCode.end());

  // TODO: everything that's not unranked will be routed through slippi
  bool isSlippiMode = m_searchSettings.mode != LylatMatchmakingClient::OnlinePlayMode::UNRANKED &&
                      m_searchSettings.mode != LylatMatchmakingClient::OnlinePlayMode::RANKED;

  // Send message to server to create ticket
  picojson::object request;
  request["type"] = picojson::value(MmMessageType::CREATE_TICKET);

  picojson::object jUser;
  jUser["uid"] = picojson::value(isSlippiMode ? m_user->slp_uid : m_user->uid);
  jUser["playKey"] = picojson::value(isSlippiMode ? m_user->slp_playKey : m_user->playKey);
  jUser["connectCode"] =
      picojson::value(isSlippiMode ? m_user->slp_connectCode : m_user->connectCode);
  jUser["displayName"] = picojson::value(m_user->displayName);

  request["user"] = picojson::value(jUser);

  picojson::object jGame;
  jGame["id"] = picojson::value(m_game->GetGameID());
  jGame["ex_id"] = picojson::value(m_game->GetLylatID());
  jGame["revision"] = picojson::value((double)m_game->GetRevision());
  jGame["type"] = picojson::value("DolphinNetplay");
  jGame["name"] =
      picojson::value(m_game->GetInternalName() + ":" + Common::GetScmDescStr().c_str());

  picojson::object jSearch;
  jSearch["mode"] = picojson::value((double)m_searchSettings.mode);
  jSearch["traversalRoomId"] = picojson::value(m_traversal_room_id);
  // u8* connectCodeArr = &connectCodeBuf[0];
  jSearch["connectCode"] = picojson::value(m_searchSettings.connectCode);
  jSearch["game"] = picojson::value(jGame);

  request["search"] = picojson::value(jSearch);

  request["appVersion"] = picojson::value(Common::GetScmDescStr().c_str());
  request["ipAddressLan"] = picojson::value(lanAddr);
  sendMessage(request);

  // Get response from server
  picojson::value response;
  int rcvRes = receiveMessage(response, 5000);
  if (rcvRes != 0)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Did not receive response from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Failed to join mm queue";
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  std::string respType = response.get("type").to_str();
  if (respType != MmMessageType::CREATE_TICKET_RESP)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Received incorrect response for create ticket");
    // WARN_LOG_FMT(LYLAT, "%s", response.dump().c_str());
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Invalid response when joining mm queue";
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  std::string err = response.get("error").to_str();
  if (err.length() > 0 && err != "null")
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Received error from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = err;
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  m_state = ProcessState::MATCHMAKING;
  WARN_LOG_FMT(LYLAT, "[Matchmaking] Request ticket success");
}

void LylatMatchmakingClient::handleMatchmaking()
{
  // Deal with class shut down
  if (m_state != ProcessState::MATCHMAKING)
    return;

  // Get response from server
  picojson::value getResp;
  int rcvRes = receiveMessage(getResp, 2000);
  if (rcvRes == -1)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Have not yet received assignment");

    return;
  }
  else if (rcvRes != 0)
  {
    // Right now the only other code is -2 meaning the server died probably?
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Lost connection to the mm server response: {}", rcvRes);
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Lost connection to the mm server";
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  std::string respType = getResp.get("type").to_str();
  if (respType != MmMessageType::GET_TICKET_RESP)
  {
    WARN_LOG_FMT(LYLAT, "[Matchmaking] Received incorrect response for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Invalid response when getting mm status";
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  std::string err = getResp.get("error").to_str();
  std::string latestVersion = getResp.get("latestVersion").to_str();
  if (err.length() > 0 && err != "null")
  {
    if (latestVersion != "")
    {
      // Update version number when the mm server tells us our version is outdated
      // Force latest version for people whose file updates dont work
      m_user->OverwriteLatestVersion(latestVersion);
    }

    WARN_LOG_FMT(LYLAT, "[Matchmaking] Received error from server for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = err;
    m_onFailureCallback(*m_game, m_errorMsg);
    return;
  }

  m_isSwapAttempt = false;

  // Clear old users
  m_remoteIps.clear();
  m_playerInfo.clear();

  auto queue = getResp.get("players");
  if (queue.is<picojson::array>())
  {
    auto arr = queue.get<picojson::array>();
    std::string localExternalIp = "";

    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
      picojson::value el = *it;
      LylatUser playerInfo;

      bool isLocal = el.get("isLocalPlayer").get<bool>();
      playerInfo.uid = el.get("uid").to_str();
      playerInfo.displayName = el.get("displayName").to_str();
      playerInfo.connectCode = el.get("connectCode").to_str();
      playerInfo.port = (int)el.get("port").get<double>();
      playerInfo.isLocal = isLocal;
      m_playerInfo.push_back(playerInfo);

      if (isLocal)
      {
        std::vector<std::string> localIpParts;
        localIpParts = SplitString(el.get("ipAddress").to_str(), ':');
        localExternalIp = localIpParts[0];
        m_localPlayerIndex = playerInfo.port - 1;
      }
    };

    // Loop a second time to get the correct remote IPs
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
      picojson::value el = *it;

      if ((int)el.get("port").get<double>() - 1 == m_localPlayerIndex)
        continue;

      auto extIp = el.get("ipAddress").to_str();
      std::vector<std::string> exIpParts;
      exIpParts = SplitString(extIp, ':');

      auto lanIp = el.get("ipAddressLan").to_str();

      // WARN_LOG(SLIPPI_ONLINE, "LAN IP: %s", lanIp.c_str());

      if (exIpParts[0] != localExternalIp || lanIp.empty())
      {
        // If external IPs are different, just use that address
        m_remoteIps.push_back(extIp);
        continue;
      }

      // TODO: Instead of using one or the other, it might be better to try both

      // If external IPs are the same, try using LAN IPs
      m_remoteIps.push_back(lanIp);
    }
  }
  m_isHost = getResp.get("isHost").get<bool>();

  // Get allowed stages. For stage select modes like direct and teams, this will only impact the
  // first map selected
  m_allowedStages.clear();
  auto stages = getResp.get("stages");
  if (stages.is<picojson::array>())
  {
    auto stagesArr = stages.get<picojson::array>();
    for (auto it = stagesArr.begin(); it != stagesArr.end(); ++it)
    {
      picojson::value el = *it;
      auto stageId = (int)el.get<double>();
      m_allowedStages.push_back(stageId);
    }
  }

  if (m_allowedStages.empty())
  {
    // Default case, shouldn't ever really be hit but it's here just in case
    m_allowedStages.push_back(0x3);   // Pokemon
    m_allowedStages.push_back(0x8);   // Yoshi's Story
    m_allowedStages.push_back(0x1C);  // Dream Land
    m_allowedStages.push_back(0x1F);  // Battlefield
    m_allowedStages.push_back(0x20);  // Final Destination

    // Add FoD if singles
    if (m_playerInfo.size() == 2)
    {
      m_allowedStages.push_back(0x2);  // FoD
    }
  }

  // Disconnect and destroy enet client to mm server
  terminateMmConnection();

  m_state = ProcessState::OPPONENT_CONNECTING;
  WARN_LOG_FMT(LYLAT, "[Matchmaking] Opponent found. isDecider: {}", m_isHost ? "true" : "false");
}

void LylatMatchmakingClient::handleConnecting()
{
  m_isSwapAttempt = false;

  std::vector<std::string> remoteParts;
  std::vector<std::string> addrs;
  std::vector<u16> ports;
  for (int i = 0; i < m_remoteIps.size(); i++)
  {
    remoteParts.clear();
    remoteParts = SplitString(m_remoteIps[i], ':');
    addrs.push_back(remoteParts[0]);
    ports.push_back(std::stoi(remoteParts[1]));
  }

  std::stringstream ipLog;
  ipLog << "Remote player IPs: ";
  for (int i = 0; i < m_remoteIps.size(); i++)
  {
    ipLog << m_remoteIps[i] << ", ";
  }

  LylatUser remoteUser;
  for (int i = 0; i < m_playerInfo.size(); i++)
  {
    auto info = m_playerInfo.at(i);
    if (!info.isLocal)
    {
      remoteUser = info;
      break;
    }
  }
  INFO_LOG_FMT(LYLAT, "[Matchmaking] Connect with: {} at {}", remoteUser.displayName,
               remoteUser.connectCode);

  // Is host is now used to specify who the decider is
  auto client = new LylatNetplayClient(addrs, ports, 1, m_hostPort, m_isHost,
                                                      m_localPlayerIndex);

  while (!m_netplayClient)
  {
    auto status = client->GetConnectStatus();
    if (status == LylatNetplayClient::ConnectStatus::NET_CONNECT_STATUS_INITIATED)
    {
      INFO_LOG_FMT(LYLAT, "[Matchmaking] Connection not yet successful");
      Common::SleepCurrentThread(500);

      // Deal with class shut down
      if (m_state != ProcessState::OPPONENT_CONNECTING)
        return;

      continue;
    }
    else if (status != LylatNetplayClient::ConnectStatus::NET_CONNECT_STATUS_CONNECTED)
    {
      ERROR_LOG_FMT(LYLAT, "[Matchmaking] Connection attempt failed, looking for someone else.");

      // Return to the start to get a new ticket to find someone else we can hopefully connect with
      m_netplayClient = nullptr;
      m_state = ProcessState::INITIALIZING;
      return;
    }

    WARN_LOG_FMT(LYLAT, "[Matchmaking] Connection success!");

    // Successful connection
    m_netplayClient = client;
  }


  // Connection success, our work is done
  m_state = ProcessState::CONNECTION_SUCCESS;
  m_onSuccessCallback(*m_game, m_isHost, remoteUser.connectCode, ports[0], m_hostPort, m_netplayClient);
  //terminateMmConnection();
}

void LylatMatchmakingClient::disconnectFromServer()
{
  isMmConnected = false;

  if (m_server)
    enet_peer_disconnect(m_server, 0);
  else
    return;

  ENetEvent netEvent;
  while (enet_host_service(m_client, &netEvent, 3000) > 0)
  {
    switch (netEvent.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(netEvent.packet);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      m_server = nullptr;
      return;
    default:
      break;
    }
  }

  // didn't disconnect gracefully force disconnect
  enet_peer_reset(m_server);
  m_server = nullptr;
}

void LylatMatchmakingClient::terminateMmConnection()
{
  // Disconnect from server
  disconnectFromServer();

  // Destroy client
  if (m_client)
  {
    enet_host_destroy(m_client);
    m_client = nullptr;
  }
}

int LylatMatchmakingClient::receiveMessage(picojson::value& msg, int timeoutMs)
{
  int hostServiceTimeoutMs = 250;

  // Make sure loop runs at least once
  if (timeoutMs < hostServiceTimeoutMs)
    timeoutMs = hostServiceTimeoutMs;

  // This is not a perfect way to timeout but hopefully it's close enough?
  int maxAttempts = timeoutMs / hostServiceTimeoutMs;

  for (int i = 0; i < maxAttempts; i++)
  {
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, hostServiceTimeoutMs);
    if (net <= 0)
      continue;

    switch (netEvent.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
    {
      std::vector<u8> buf;
      buf.insert(buf.end(), netEvent.packet->data,
                 netEvent.packet->data + netEvent.packet->dataLength);

      std::string str(buf.begin(), buf.end());
      const auto error = picojson::parse(msg, str);
      WARN_LOG_FMT(LYLAT, "[Matchmaking] MESSAGE: {}", str);

      enet_packet_destroy(netEvent.packet);
      return 0;
    }
    case ENET_EVENT_TYPE_DISCONNECT:
      // Return -2 code to indicate we have lost connection to the server
      return -2;
    }
  }

  return -1;
}

void LylatMatchmakingClient::sendMessage(picojson::object msg)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channelId = 0;

  std::string msgContents = picojson::value(msg).serialize();

  ENetPacket* epac = enet_packet_create(msgContents.c_str(), msgContents.length(), flags);
  enet_peer_send(m_server, channelId, epac);
}
