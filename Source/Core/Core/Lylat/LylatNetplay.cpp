// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "LylatNetplay.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <thread>
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"

static std::mutex pad_mutex;
static std::mutex ack_mutex;

LylatNetplayClient* SLIPPI_NETPLAY = nullptr;

// called from ---GUI--- thread
LylatNetplayClient::~LylatNetplayClient()
{
  if (m_thread.joinable())
    m_thread.join();

  WARN_LOG_FMT(LYLAT, "Netplay client cleanup complete");
}

// called from ---SLIPPI EXI--- thread
LylatNetplayClient::LylatNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports,
                                       const u8 remotePlayerCount, const u16 localPort,
                                       bool isDecider, u8 playerIdx)
#ifdef _WIN32
    : m_qos_handle(nullptr), m_qos_flow_id(0)
#endif
{
  WARN_LOG_FMT(LYLAT, "Initializing Slippi Netplay for port: {}, with host: {}, player idx: {}",
               localPort, isDecider ? "true" : "false", playerIdx);
  this->isDecider = isDecider;
  this->m_remotePlayerCount = remotePlayerCount;
  this->playerIdx = playerIdx;

  // Set up remote player data structures
  int j = 0;
  for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++, j++)
  {
    if (j == playerIdx)
      j++;
    this->frameOffsetData[i] = FrameOffsetData();
    this->lastFrameTiming[i] = FrameTiming();
    this->pingUs[i] = 0;
    this->lastFrameAcked[i] = 0;
  }

  SLIPPI_NETPLAY = std::move(this);

  // Local address
  ENetAddress* localAddr = nullptr;
  ENetAddress localAddrDef;

  // It is important to be able to set the local port to listen on even in a client connection
  // because not doing so will break hole punching, the host is expecting traffic to come from a
  // specific ip/port and if the port does not match what it is expecting, it will not get through
  // the NAT on some routers
  if (localPort > 0)
  {
    INFO_LOG_FMT(LYLAT, "Setting up local address");

    localAddrDef.host = ENET_HOST_ANY;
    localAddrDef.port = localPort;

    localAddr = &localAddrDef;
  }

  // TODO: Figure out how to use a local port when not hosting without accepting incoming
  // connections
  m_client = enet_host_create(localAddr, 10, 3, 0, 0);

  if (m_client == nullptr)
  {

    ERROR_LOG_FMT(LYLAT, "Couldn't Create Client");
    PanicAlertFmt("Couldn't Create Client");
    slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_FAILED;
    return;
  }

  for (int i = 0; i < remotePlayerCount; i++)
  {
    ENetAddress addr;
    enet_address_set_host(&addr, addrs[i].c_str());
    addr.port = ports[i];
    // INFO_LOG_FMT(LYLAT, "Set ENet host, addr = %x, port = %d", addr.host, addr.port);

    ENetPeer* peer = enet_host_connect(m_client, &addr, 3, 0);
    m_server.push_back(peer);

    // Store this connection
    std::stringstream keyStrm;
    keyStrm << addr.host << "-" << addr.port;
    activeConnections[keyStrm.str()][peer] = true;
    INFO_LOG_FMT(LYLAT, "New connection (constr): {}", keyStrm.str().c_str());

    if (peer == nullptr)
    {
      PanicAlertFmt("Couldn't create peer.");
      ERROR_LOG_FMT(LYLAT, "Couldn't Create peer");
      slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_FAILED;
      return;
    }
    else
    {
      // INFO_LOG_FMT(LYLAT, "Connecting to ENet host, addr = %x, port = %d",
      // peer->address.host,
      //         peer->address.port);
    }
  }

  slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_INITIATED;

  m_thread = std::thread(&LylatNetplayClient::ThreadFunc, this);
}

// Make a dummy client
LylatNetplayClient::LylatNetplayClient(bool isDecider)
{
  this->isDecider = isDecider;
  SLIPPI_NETPLAY = std::move(this);
  slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_FAILED;
}

u8 LylatNetplayClient::PlayerIdxFromPort(u8 port)
{
  u8 p = port;
  if (port > playerIdx)
  {
    p--;
  }
  return p;
}

u8 LylatNetplayClient::LocalPlayerPort()
{
  return this->playerIdx;
}

// called from ---NETPLAY--- thread
void LylatNetplayClient::ThreadFunc()
{
  // Let client die 1 second before host such that after a swap, the client won't be connected to
  u64 startTime = Common::Timer::GetTimeMs();
  u64 timeout = 8000;

  std::vector<bool> connections;
  std::vector<ENetAddress> remoteAddrs;
  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    remoteAddrs.push_back(m_server[i]->address);
    connections.push_back(false);
  }

  while (slippiConnectStatus == ConnectStatus::NET_CONNECT_STATUS_INITIATED)
  {
    // This will confirm that connection went through successfully
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 500);
    if (net > 0)
    {
      sf::Packet rpac;
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(LYLAT, "[Netplay] got receive event with nil peer");
          continue;
        }
        INFO_LOG_FMT(LYLAT, "[Netplay] got receive event with peer addr {}:{}",
                     netEvent.peer->address.host, netEvent.peer->address.port);
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);

        //OnData(rpac, netEvent.peer);

        enet_packet_destroy(netEvent.packet);
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(LYLAT, "[Netplay] got disconnect event with nil peer");
          continue;
        }
        INFO_LOG_FMT(LYLAT, "[Netplay] got disconnect event with peer addr {}:{}.",
                     netEvent.peer->address.host, netEvent.peer->address.port);
        break;

      case ENET_EVENT_TYPE_CONNECT:
      {
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(LYLAT, "[Netplay] got connect event with nil peer");
          continue;
        }

        std::stringstream keyStrm;
        keyStrm << netEvent.peer->address.host << "-" << netEvent.peer->address.port;
        activeConnections[keyStrm.str()][netEvent.peer] = true;
        INFO_LOG_FMT(LYLAT, "New connection (early): {}", keyStrm.str().c_str());

        for (auto pair : activeConnections)
        {
          INFO_LOG_FMT(LYLAT, "{}: {}", pair.first.c_str(), pair.second.size());
        }

        INFO_LOG_FMT(LYLAT, "[Netplay] got connect event with peer addr {}:{}.",
                     netEvent.peer->address.host, netEvent.peer->address.port);

        auto isAlreadyConnected = false;
        for (int i = 0; i < m_server.size(); i++)
        {
          if (connections[i] && netEvent.peer->address.host == m_server[i]->address.host &&
              netEvent.peer->address.port == m_server[i]->address.port)
          {
            m_server[i] = netEvent.peer;
            isAlreadyConnected = true;
            break;
          }
        }

        if (isAlreadyConnected)
        {
          // Don't add this person again if they are already connected. Not doing this can cause one
          // person to take up 2 or more spots, denying one or more players from connecting and thus
          // getting stuck on the "Waiting" step
          INFO_LOG_FMT(LYLAT, "Already connected!");
          break;  // Breaks out of case
        }

        for (int i = 0; i < m_server.size(); i++)
        {
          // This check used to check for port as well as host. The problem was that for some
          // people, their internet will switch the port they're sending from. This means these
          // people struggle to connect to others but they sometimes do succeed. When we were
          // checking for port here though we would get into a state where the person they succeeded
          // to connect to would not accept the connection with them, this would lead the player
          // with this internet issue to get stuck waiting for the other player. The only downside
          // to this that I can guess is that if you fail to connect to one person out of two that
          // are on your LAN, it might report that you failed to connect to the wrong person. There
          // might be more problems tho, not sure
          INFO_LOG_FMT(LYLAT, "[Netplay] Comparing connection address: {} - {}",
                       remoteAddrs[i].host, netEvent.peer->address.host);
          if (remoteAddrs[i].host == netEvent.peer->address.host && !connections[i])
          {
            INFO_LOG_FMT(LYLAT, "[Netplay] Overwriting ENetPeer for address: {}:{}",
                         netEvent.peer->address.host, netEvent.peer->address.port);
            INFO_LOG_FMT(LYLAT,
                         "[Netplay] Overwriting ENetPeer with id ({}) with new peer of id {}",
                         m_server[i]->connectID, netEvent.peer->connectID);
            m_server[i] = netEvent.peer;
            connections[i] = true;
            break;
          }
        }
        break;
      }
      }
    }

    bool allConnected = true;
    for (int i = 0; i < m_remotePlayerCount; i++)
    {
      if (!connections[i])
        allConnected = false;
    }

    if (allConnected)
    {
      m_client->intercept = ENetUtil::InterceptCallback;
      INFO_LOG_FMT(LYLAT, "Slippi online connection successful!");
      slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_CONNECTED;
      break;
    }

    for (int i = 0; i < m_remotePlayerCount; i++)
    {
      INFO_LOG_FMT(LYLAT, "m_client peer {} state: {}", i, m_client->peers[i].state);
    }
    INFO_LOG_FMT(LYLAT, "[Netplay] Not yet connected. Res: {}, Type: {}", net, netEvent.type);

    // Time out after enough time has passed
    u64 curTime = Common::Timer::GetTimeMs();
    if ((curTime - startTime) >= timeout || !m_do_loop.IsSet())
    {
      for (int i = 0; i < m_remotePlayerCount; i++)
      {
        if (!connections[i])
        {
          failedConnections.push_back(i);
        }
      }

      slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_FAILED;
      INFO_LOG_FMT(LYLAT, "Slippi online connection failed");
      return;
    }
  }
}

bool LylatNetplayClient::IsDecider()
{
  return isDecider;
}

LylatNetplayClient::ConnectStatus LylatNetplayClient::GetConnectStatus()
{
  return slippiConnectStatus;
}
