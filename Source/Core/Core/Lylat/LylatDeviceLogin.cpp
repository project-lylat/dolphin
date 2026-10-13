// Copyright 2026 Project Lylat
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Lylat/LylatDeviceLogin.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Lylat/LylatUser.h"

namespace Lylat
{
namespace
{
const char* Platform()
{
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}

std::string JsonString(const picojson::value& root, const char* key)
{
  if (!root.is<picojson::object>() || !root.contains(key))
    return {};
  const picojson::value& v = root.get(key);
  return v.is<std::string>() ? v.get<std::string>() : std::string{};
}

int JsonInt(const picojson::value& root, const char* key, int fallback)
{
  if (!root.is<picojson::object>() || !root.contains(key))
    return fallback;
  const picojson::value& v = root.get(key);
  if (v.is<double>())
    return static_cast<int>(v.get<double>());
  // The Lylat server sends match_number as a zero-padded string ("07").
  if (v.is<std::string>())
  {
    const std::string& s = v.get<std::string>();
    if (!s.empty() && s.size() <= 4 && s.find_first_not_of("0123456789") == std::string::npos)
      return std::atoi(s.c_str());
  }
  return fallback;
}

struct Reply
{
  bool reached = false;
  int http = 0;
  std::string body;
};
}  // namespace

std::string GetLylatBaseUrl()
{
  std::string url = Config::Get(Config::NETPLAY_LYLAT_URL);
  while (!url.empty() && url.back() == '/')
    url.pop_back();
  return url;
}

bool SaveAccountFile(const std::string& body)
{
  const std::string path = LylatUser::GetFilePath();
  const std::string tmp = path + ".qr-login.tmp";
  if (!File::WriteStringToFile(tmp, body))
    return false;
  // Check it with the same parser as a dropped lylat.json before replacing the old one.
  LylatUser* check = LylatUser::GetUserFromDisk(tmp);
  if (!check)
  {
    File::Delete(tmp);
    return false;
  }
  delete check;
  if (!File::Rename(tmp, path))
  {
    File::Delete(tmp);
    return false;
  }
  return true;
}

DeviceLogin::DeviceLogin() = default;

DeviceLogin::~DeviceLogin()
{
  Cancel();
}

void DeviceLogin::Cancel()
{
  m_cancel = true;
  m_cv.notify_all();
  if (m_thread.joinable())
    m_thread.join();
}

void DeviceLogin::Start()
{
  Cancel();
  m_cancel = false;
  {
    std::lock_guard lock(m_mutex);
    m_view.state = DeviceLoginState::Loading;
    m_view.error.clear();
  }
  m_thread = std::thread(&DeviceLogin::Run, this, GetLylatBaseUrl());
}

DeviceLoginView DeviceLogin::GetView() const
{
  std::lock_guard lock(m_mutex);
  return m_view;
}

void DeviceLogin::Set(DeviceLoginState state, const std::string& error)
{
  if (state == DeviceLoginState::Error)
    WARN_LOG_FMT(NETPLAY, "Lylat QR sign-in: {}", error);
  std::lock_guard lock(m_mutex);
  m_view.state = state;
  m_view.error = error;
}

bool DeviceLogin::Wait(std::chrono::milliseconds duration)
{
  std::unique_lock lock(m_mutex);
  return !m_cv.wait_for(lock, duration, [this] { return m_cancel.load(); });
}

void DeviceLogin::Run(std::string base)
{
  if (base.empty())
  {
    Set(DeviceLoginState::Unsupported);
    return;
  }

  const auto post = [this](const std::string& url, const std::string& json) {
    Reply r;
    Common::HttpRequest req(std::chrono::milliseconds{8000},
                            [this](s64, s64, s64, s64) { return !m_cancel.load(); });
    const Common::HttpRequest::Headers headers = {{"Content-Type", "application/json"},
                                                  {"Accept", "application/json"}};
    const auto resp =
        req.Post(url, json, headers, Common::HttpRequest::AllowedReturnCodes::All);
    r.http = req.GetLastResponseCode();
    r.reached = resp.has_value() && r.http != 0;
    if (resp)
      r.body.assign(resp->begin(), resp->end());
    return r;
  };

  std::string device_code;  // secret: never logged or shown
  int interval = 5;
  {
    const std::string req = std::string("{\"client\":\"dolphin-lylat\",\"platform\":\"") +
                            Platform() + "\",\"name\":\"Lylat Dolphin " +
                            Common::GetScmDescStr() + "\"}";
    const Reply r = post(base + "/api/device/code", req);
    if (m_cancel)
      return;
    if (r.reached && r.http == 404)
    {
      Set(DeviceLoginState::Unsupported);
      return;
    }
    if (!r.reached)
    {
      Set(DeviceLoginState::Error, "Could not reach the server.");
      return;
    }
    if (r.http == 429)
    {
      Set(DeviceLoginState::Error, "Too many codes. Try again in a few minutes.");
      return;
    }
    picojson::value root;
    if (r.http != 200 || !picojson::parse(root, r.body).empty())
    {
      Set(DeviceLoginState::Error, fmt::format("Server error ({}).", r.http));
      return;
    }
    device_code = JsonString(root, "device_code");
    const std::string user_code = JsonString(root, "user_code");
    const std::string uri = JsonString(root, "verification_uri");
    const std::string complete = JsonString(root, "verification_uri_complete");
    const int number = JsonInt(root, "match_number", -1);
    const int expires = std::clamp(JsonInt(root, "expires_in", 600), 10, 3600);
    interval = std::clamp(JsonInt(root, "interval", 5), 1, 30);
    if (device_code.empty() || user_code.empty() || complete.empty() || number < 0 ||
        complete.size() > 512)
    {
      Set(DeviceLoginState::Error, "The server sent an unusable code.");
      return;
    }
    std::lock_guard lock(m_mutex);
    m_view.user_code = user_code;
    m_view.verification_uri = uri;
    m_view.verification_uri_complete = complete;
    m_view.match_number = number;
    m_view.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(expires);
    m_view.error.clear();
    ++m_view.serial;
    m_view.state = DeviceLoginState::Showing;
  }

  const std::string token_json = "{\"device_code\":\"" + device_code + "\"}";
  int failures = 0;
  while (true)
  {
    if (!Wait(std::chrono::seconds(interval)))
      return;
    if (std::chrono::steady_clock::now() >= GetView().deadline)
    {
      Set(DeviceLoginState::Expired);
      return;
    }
    const Reply r = post(base + "/api/device/token", token_json);
    if (m_cancel)
      return;
    if (!r.reached || r.http >= 500)
    {
      if (++failures >= 6)
      {
        Set(DeviceLoginState::Error, "Lost contact with the server.");
        return;
      }
      continue;
    }
    failures = 0;
    if (r.http == 200)
    {
      if (!SaveAccountFile(r.body))
      {
        Set(DeviceLoginState::Error, "The server sent an invalid account file.");
        return;
      }
      // The GUI thread reloads lylat.json (LylatUser::GetUser(true)) when it sees LoggedIn.
      Set(DeviceLoginState::Approved);
      // Let the "Approved" state show for a moment before "Logged in".
      if (!Wait(std::chrono::milliseconds(1200)))
        return;
      Set(DeviceLoginState::LoggedIn);
      return;
    }
    picojson::value root;
    const std::string error =
        picojson::parse(root, r.body).empty() ? JsonString(root, "error") : std::string{};
    if (error == "authorization_pending")
      continue;
    if (error == "slow_down")
    {
      interval = std::min(interval + 5, 60);
      continue;
    }
    if (error == "expired_token" || error == "invalid_grant")
    {
      Set(DeviceLoginState::Expired);
      return;
    }
    if (error == "access_denied")
    {
      Set(DeviceLoginState::Denied);
      return;
    }
    if (r.http == 404)
    {
      Set(DeviceLoginState::Unsupported);
      return;
    }
    if (++failures >= 6)
    {
      Set(DeviceLoginState::Error, "The server refused the request.");
      return;
    }
  }
}
}  // namespace Lylat
