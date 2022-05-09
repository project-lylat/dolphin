//
// Created by Robert Peralta on 4/21/22.
//

#include "LylatUser.h"
#include "Common/FileUtil.h"
#include "picojson.h"

LylatUser* LylatUser::singleton = nullptr;

LylatUser* LylatUser::GetUser(bool reloadFromDisk, bool refreshFromServer)
{
  if (!singleton || reloadFromDisk)
    singleton = LylatUser::getUserFromDisk();

  if (refreshFromServer)
    singleton = LylatUser::refreshUserFromSever(singleton);

  return singleton;
}

LylatUser* LylatUser::GetUser(bool reloadFromDisk)
{
  return GetUser(reloadFromDisk, false);
}

LylatUser* LylatUser::GetUser()
{
  return GetUser(false, false);
}

bool LylatUser::DeleteUserFile()
{
  std::string path = LylatUser::GetFilePath();
  if (File::Exists(path))
  {
    if (File::Delete(path))
    {
      singleton = nullptr;
      return true;
    }
    return false;
  }

  return false;
}

std::string LylatUser::GetFilePath()
{
  return File::GetUserPath(D_USER_IDX) + "lylat.json";
}

LylatUser* LylatUser::GetUserFromDisk(const std::string& path)
{
  std::string data;
  if (File::Exists(path) && File::ReadFileToString(path, data))
  {
    picojson::value json;
    const auto error = picojson::parse(json, data);

    // TODO: show error because of invalid json
    if (!error.empty())
      return nullptr;

    auto user = new LylatUser();

    if (!json.contains("uid"))
      return nullptr;
    if (!json.contains("displayName"))
      return nullptr;
    if (!json.contains("playKey"))
      return nullptr;
    if (!json.contains("connectCode"))
      return nullptr;
    if (!json.contains("latestVersion"))
      return nullptr;

    user->uid = json.get("uid").to_str();
    user->displayName = json.get("displayName").to_str();
    user->playKey = json.get("playKey").to_str();
    user->connectCode = json.get("connectCode").to_str();
    user->latestVersion = json.get("latestVersion").to_str();

    if (!json.contains("slippi"))
      return user;
    auto slp = json.get("slippi");

    if (!slp.contains("uid"))
      return user;
    if (!slp.contains("displayName"))
      return user;
    if (!slp.contains("playKey"))
      return user;
    if (!slp.contains("connectCode"))
      return user;
    if (!slp.contains("latestVersion"))
      return user;

    user->slp_uid = slp.get("uid").to_str();
    user->slp_displayName = slp.get("displayName").to_str();
    user->slp_playKey = slp.get("playKey").to_str();
    user->slp_connectCode = slp.get("connectCode").to_str();
    user->slp_latestVersion = slp.get("latestVersion").to_str();

    return user;
  }

  return nullptr;
}

LylatUser* LylatUser::getUserFromDisk()
{
  std::string path = LylatUser::GetFilePath();
  return GetUserFromDisk(path);
}

LylatUser* LylatUser::refreshUserFromSever(LylatUser* user)
{
  // TODO: implement
  return nullptr;
}
void LylatUser::OverwriteLatestVersion(std::string version)
{
  this->latestVersion = std::move(version);
}
