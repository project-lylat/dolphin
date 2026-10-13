// Copyright 2026 Project Lylat
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "Common/CommonTypes.h"

namespace Lylat
{
// QR / device-code sign-in (the Lylat server's docs/device-login.md):
//   POST <base>/api/device/code, then POST <base>/api/device/token every `interval` seconds.
// A 200 from the token endpoint is the lylat.json account file. The device code is a
// secret: it only lives in the worker thread, never in the view or a log.
enum class DeviceLoginState
{
  Idle,
  Loading,
  Showing,
  Approved,
  LoggedIn,
  Expired,
  Denied,
  Error,
  Unsupported,  // the server answered 404 to /api/device/code: use the browser flow
};

struct DeviceLoginView
{
  DeviceLoginState state = DeviceLoginState::Idle;
  std::string user_code;
  std::string verification_uri;
  std::string verification_uri_complete;
  int match_number = -1;
  std::chrono::steady_clock::time_point deadline{};
  std::string error;
  u32 serial = 0;  // bumps on every new code
};

// The Lylat web origin, from [NetPlay] LylatURL (default https://lylat.gg).
std::string GetLylatBaseUrl();

class DeviceLogin
{
public:
  DeviceLogin();
  ~DeviceLogin();
  DeviceLogin(const DeviceLogin&) = delete;
  DeviceLogin& operator=(const DeviceLogin&) = delete;

  // Asks for a new code (cancels any attempt in progress).
  void Start();
  // Stops polling and joins the worker.
  void Cancel();
  DeviceLoginView GetView() const;

private:
  void Run(std::string base);
  void Set(DeviceLoginState state, const std::string& error = {});
  bool Wait(std::chrono::milliseconds duration);

  mutable std::mutex m_mutex;
  std::condition_variable m_cv;
  std::atomic<bool> m_cancel{false};
  std::thread m_thread;
  DeviceLoginView m_view;
};

// Writes a server-provided account file to where manual login puts lylat.json.
bool SaveAccountFile(const std::string& body);
}  // namespace Lylat
