//
// Created by Robert Peralta on 4/21/22.
//

#pragma once

#include "string"

class LylatUser
{
public:
  std::string uid;
  std::string displayName;
  std::string playKey;
  std::string connectCode;
  std::string latestVersion;
  std::string slp_uid;
  std::string slp_displayName;
  std::string slp_playKey;
  std::string slp_connectCode;
  std::string slp_latestVersion;

  int port;

  static LylatUser* GetUser(bool reloadFromDisk, bool refreshFromServer);
  static LylatUser* GetUser(bool reloadFromDisk);
  static LylatUser* GetUser();
  static LylatUser* GetUserFromDisk(const std::string& path);
  static bool DeleteUserFile();
  static std::string GetFilePath();

  void OverwriteLatestVersion(std::string basicString);

private:
  static LylatUser* singleton;
  static LylatUser* getUserFromDisk();
  static LylatUser* refreshUserFromSever(LylatUser* user);
};
