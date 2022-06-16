// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"
#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <Qos2.h>
#endif

#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL 30 // Number of frames to wait before attempting to time-sync
#define SLIPPI_PING_DISPLAY_INTERVAL 60
#define SLIPPI_REMOTE_PLAYER_MAX 3
#define SLIPPI_REMOTE_PLAYER_COUNT 3

class OnlinePlayMode;
class LylatNetplayClient
{
  public:
	void ThreadFunc();
	void SendAsync(std::unique_ptr<sf::Packet> packet);

	LylatNetplayClient(bool isDecider); // Make a dummy client
	LylatNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports, const u8 remotePlayerCount,
	                    const u16 localPort, bool isDecider, u8 playerIdx);
	~LylatNetplayClient();

	// Slippi Online
	enum class ConnectStatus
	{
		NET_CONNECT_STATUS_UNSET,
		NET_CONNECT_STATUS_INITIATED,
		NET_CONNECT_STATUS_CONNECTED,
		NET_CONNECT_STATUS_FAILED,
		NET_CONNECT_STATUS_DISCONNECTED,
	};

	bool IsDecider();
	bool IsConnectionSelected();
	u8 LocalPlayerPort();
	ConnectStatus GetConnectStatus();

	struct
	{
		std::recursive_mutex game;
		// lock order
		std::recursive_mutex players;
		std::recursive_mutex async_queue_write;
	} m_crit;

	ENetHost *m_client = nullptr;
	std::vector<ENetPeer *> m_server;
	std::thread m_thread;
	u8 m_remotePlayerCount = 0;

	std::string m_selected_game;
	Common::Flag m_is_running{false};
	Common::Flag m_do_loop{true};

	unsigned int m_minimum_buffer_size = 6;

	u32 m_current_game = 0;

	// Slippi Stuff
	struct FrameTiming
	{
		int32_t frame;
		u64 timeUs;
	};

	struct FrameOffsetData
	{
		// TODO: Should the buffer size be dynamic based on time sync interval or not?
		int idx;
		std::vector<s32> buf;
	};

	bool isConnectionSelected = false;
	bool isDecider = false;
	bool hasGameStarted = false;
	u8 playerIdx = 0;

	std::unordered_map<std::string, std::map<ENetPeer *, bool>> activeConnections;

	u64 pingUs[SLIPPI_REMOTE_PLAYER_MAX];
	int32_t lastFrameAcked[SLIPPI_REMOTE_PLAYER_MAX];
	FrameOffsetData frameOffsetData[SLIPPI_REMOTE_PLAYER_MAX];
	FrameTiming lastFrameTiming[SLIPPI_REMOTE_PLAYER_MAX];

	ConnectStatus slippiConnectStatus = ConnectStatus::NET_CONNECT_STATUS_UNSET;
	std::vector<int> failedConnections;

	bool m_is_recording = false;

  private:
	u8 PlayerIdxFromPort(u8 port);

	bool m_is_connected = false;

#ifdef _WIN32
	HANDLE m_qos_handle;
	QOS_FLOWID m_qos_flow_id;
#endif

	u32 m_timebase_frame = 0;
};
extern LylatNetplayClient *LYLAT_NETPLAY; // singleton static pointer

static bool IsOnline()
{
	return LYLAT_NETPLAY != nullptr;
}
