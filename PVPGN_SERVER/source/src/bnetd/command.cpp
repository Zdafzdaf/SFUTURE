/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999  Gediminas (gediminas_lt@mailexcite.com)
 * Copyright (C) 1999  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2000  Dizzy
 * Copyright (C) 2000  Onlyer (onlyer@263.net)
 * Copyright (C) 2003,2004  Aaron
 * Copyright (C) 2004  Donny Redmond (dredmond@linuxmail.org)
 * Copyright (C) 2008  Pelish (pelish@gmail.com)
 * Copyright (C) 2014  HarpyWar (harpywar@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


 /*

	 /host MAP PLAYERS NAME
	 /host dota 5x5 dota go go

	 ���� ��� ��� ��������� �� ������� ���� �� :
	 - ����� ���������� ����
	 - ���������� ���� ��� ������ *
	 - ��������� ���� ��� �������
	 - ����� 10 ������, ���� ���� �� ����� ������� �� ������ ������ � ��������� ��������� ����� ����

	 * ������������� ��� ����� �������������� ��� �������� ������ ���� /swap /unhost � �.�
 */



#include "common/setup_before.h"
#include "command.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <regex>
#include <fstream>

#include "compat/gmtime_s.h"
#include "compat/localtime_s.h"
#include "compat/strcasecmp.h"
#include "common/tag.h"
#include "common/util.h"
#include "common/version.h"
#include "common/eventlog.h"
#include "common/bnettime.h"
#include "common/addr.h"
#include "common/packet.h"
#include "common/bnethash.h"
#include "common/list.h"
#include "common/proginfo.h"
#include "common/queue.h"
#include "common/bn_type.h"
#include "common/xalloc.h"
#include "common/xstr.h"
#include "common/trans.h"
#include "common/lstr.h"
#include "common/hashtable.h"
#include "common/xstring.h"

#include "connection.h"
#include "message.h"
#include "channel.h"
#include "game.h"
#include "team.h"
#include "account.h"
#include "account_wrap.h"
#include "server.h"
#include "prefs.h"
#include "ladder.h"
#include "timer.h"
#include "helpfile.h"
#include "mail.h"
#include "runprog.h"
#include "alias_command.h"
#include "realm.h"
#include "ipban.h"
#include "command_groups.h"
#include "news.h"
#include "topic.h"
#include "friends.h"
#include "clan.h"
#include "common/flags.h"
#include "icons.h"
#include "userlog.h"
#include "i18n.h"
#include "account_email_verification.h"
#include "handle_bnet.h"
#include "attrlayer.h"
#include "watch.h"
#include <filesystem>
namespace fs = std::filesystem;
#include "FTPClient.h"

#include "bcrypt.h"
#include <thread>


//here
#include "ini.h"

#ifdef WITH_LUA
#include "luainterface.h"
#endif
#include "common/setup_after.h"

static std::regex GetGameNameAndMessage(".*?<(.*?)>X<(.*?)>");

namespace pvpgn
{

	namespace bnetd
	{
		static char const* bnclass_get_str(unsigned int cclass);
		static void do_whisper(t_connection* user_c, char const* dest, char const* text);
		static void do_whois(t_connection* c, char const* dest);
		static void user_timer_cb(t_connection* c, std::time_t now, t_timer_data str);

		std::string msgtemp, msgtemp2;
		char msgtemp0[MAX_MESSAGE_LEN];


		unsigned int filesize(std::string filename)
		{
			std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
			return in.tellg();
		}

		std::vector<std::string> get_file_list(const fs::path& path)
		{
			std::vector<std::string> m_file_list;
			if (!path.empty())
			{
				fs::path apk_path(path);
				fs::recursive_directory_iterator end;

				for (fs::recursive_directory_iterator i(apk_path); i != end; ++i)
				{
					const fs::path cp = (*i);
					m_file_list.push_back(cp.string());
				}
			}
			return m_file_list;
		}

		std::vector<unsigned char> readFile(std::string  filename)
		{
			std::vector<unsigned char> vec;
			// open the file:
			std::ifstream file(filename, std::ios::binary);
			if (file.is_open() && file.good())
			{
				// Stop eating new lines in binary mode!!!
				file.unsetf(std::ios::skipws);

				// get its size:
				std::streampos fileSize;

				file.seekg(0, std::ios::end);
				fileSize = file.tellg();
				file.seekg(0, std::ios::beg);

				// reserve capacity

				vec.reserve(fileSize);

				// read the data:
				vec.insert(vec.begin(),
					std::istream_iterator<unsigned char>(file),
					std::istream_iterator<unsigned char>());
			}
			return vec;
		}


		bool AllreadyUpdate = false;
		void UpadeMapList(std::string FtpIp,
			int FtpPort,
			std::string FtpUser,
			std::string FtpPassword,
			std::string FtpStartDir, std::string mapcfghostpath, std::string mapcfgpath)
		{
			if (AllreadyUpdate)
			{
				std::cout << "AllreadyUpdate" << std::endl;
				return;
			}

			AllreadyUpdate = true;

			try
			{
				std::string MapCfgHostPath = mapcfghostpath;
				std::string MapCfgPath = mapcfgpath;


				nsFTP::CFTPClient ftpClient;

				nsFTP::CLogonInfo logonInfo = nsFTP::CLogonInfo(FtpIp, FtpPort, FtpUser, FtpPassword);
				std::cout << FtpIp << " " << FtpPort << " " << FtpUser << " " << FtpPassword << " " << mapcfgpath << std::endl;
				// connect to server
				if (ftpClient.Login(logonInfo))
				{
					std::cout << "login" << std::endl;
					// get directory listing
					{
						nsFTP::TFTPFileStatusShPtrVec list;
						ftpClient.List(FtpStartDir, list);
						// get fa
						auto LocalMapList = get_file_list(MapCfgPath);
						std::vector<std::string> RemoteFileList;

						std::cout << "files:" << std::to_string(LocalMapList.size()) << std::endl;
						// iterate listing
						for (nsFTP::TFTPFileStatusShPtrVec::iterator it = list.begin();
							it != list.end(); ++it)
						{
							RemoteFileList.push_back(fs::u8path((*it)->Name()).string());
							fs::path RemoteFilePath = fs::u8path((*it)->Path() + (*it)->Name());
							std::string RemoveFileExt = RemoteFilePath.extension().string();
							std::transform(RemoveFileExt.begin(), RemoveFileExt.end(), RemoveFileExt.begin(), (int(*)(int))tolower);
							std::cout << RemoveFileExt << std::endl;
							if (RemoveFileExt == ".ini")
							{
								bool needReplaceFile = true;
								fs::path FilePath = MapCfgPath + fs::u8path((*it)->Name()).string();
								auto filedata = readFile(FilePath.string() + ".time");
								time_t localfiletime = 0;
								time_t remotefiletime = (*it)->MTime();

								if (filedata.size() == sizeof(time_t))
								{
									memcpy(&localfiletime, &filedata[0], sizeof(time_t));
									if (localfiletime == (*it)->MTime())
									{
										needReplaceFile = false;
									}
								}

								if (needReplaceFile)
								{
									std::string remoteaddr = RemoteFilePath.string();
									replaceAll(remoteaddr, "\\", "/");
									ftpClient.DownloadFile(remoteaddr, FilePath.string());
									std::ofstream sw(FilePath.string() + ".time", std::ofstream::binary);
									sw.write((const char*)&remotefiletime, sizeof(time_t));
									sw.close();
								}
							}
						}


						for (auto& locfile : LocalMapList)
						{
							transform(locfile.begin(), locfile.end(), locfile.begin(), (int(*)(int))tolower);
						}

						for (auto& remfile : RemoteFileList)
						{
							transform(remfile.begin(), remfile.end(), remfile.begin(), (int(*)(int))tolower);
						}

						if (LocalMapList.size() && RemoteFileList.size())
						{
							for (auto locfile : LocalMapList)
							{
								bool FileFound = false;
								for (auto remfile : RemoteFileList)
								{
									if (locfile.find(remfile) != std::string::npos)
									{
										FileFound = true;
									}

								}

								if (!FileFound)
								{
									fs::remove(locfile);
								}
							}

						}

					}

					// get directory listing
					{
						nsFTP::TFTPFileStatusShPtrVec list;
						ftpClient.List(FtpStartDir + "pvpgn_stats_cfgs/", list);
						// get fa
						auto LocalMapList = get_file_list(MapCfgHostPath);
						std::vector<std::string> RemoteFileList;
						// iterate listing
						for (nsFTP::TFTPFileStatusShPtrVec::iterator it = list.begin();
							it != list.end(); ++it)
						{
							RemoteFileList.push_back(fs::u8path((*it)->Name()).string());
							fs::path RemoteFilePath = fs::u8path((*it)->Path() + (*it)->Name());
							std::string RemoveFileExt = RemoteFilePath.extension().string();
							std::transform(RemoveFileExt.begin(), RemoveFileExt.end(), RemoveFileExt.begin(), (int(*)(int))tolower);

							if (RemoveFileExt == ".ini")
							{
								bool needReplaceFile = true;
								fs::path FilePath = MapCfgHostPath + fs::u8path((*it)->Name()).string();
								auto filedata = readFile(FilePath.string() + ".time");
								time_t localfiletime = 0;
								time_t remotefiletime = (*it)->MTime();

								if (filedata.size() == sizeof(time_t))
								{
									memcpy(&localfiletime, &filedata[0], sizeof(time_t));
									if (localfiletime == (*it)->MTime())
									{
										needReplaceFile = false;
									}
								}

								if (needReplaceFile)
								{
									std::string remoteaddr = RemoteFilePath.string();
									replaceAll(remoteaddr, "\\", "/");
									ftpClient.DownloadFile(remoteaddr, FilePath.string());
									std::ofstream sw(FilePath.string() + ".time", std::ofstream::binary);
									sw.write((const char*)&remotefiletime, sizeof(time_t));
									sw.close();
								}
							}
						}


						for (auto& locfile : LocalMapList)
						{
							transform(locfile.begin(), locfile.end(), locfile.begin(), (int(*)(int))tolower);
						}

						for (auto& remfile : RemoteFileList)
						{
							transform(remfile.begin(), remfile.end(), remfile.begin(), (int(*)(int))tolower);
						}

						if (LocalMapList.size() && RemoteFileList.size())
						{
							for (auto locfile : LocalMapList)
							{
								bool FileFound = false;
								for (auto remfile : RemoteFileList)
								{
									if (locfile.find(remfile) != std::string::npos)
									{
										FileFound = true;
									}

								}

								if (!FileFound)
								{
									fs::remove(locfile);
								}
							}

						}

					}

					ftpClient.Logout();
				}
				else
				{
					std::cout << "bad login" << std::endl;

				}
			}
			catch (...)
			{
				std::cout << "errrrr" << std::endl;
			}

			AllreadyUpdate = false;
			NeedReloadMapList = true;
		}


		std::vector<MapHostStruct> MapHostStructList;

		extern void PrintMapList(t_connection* c)
		{
			if (!c)
				return;
			message_send_text(c, message_type_info, c, "[MAP CODE : MAP NAME : MAP ARGS]");
			for (auto& var : MapHostStructList)
			{
				message_send_text(c, message_type_info, c, localize(c, "code:{} name:{} modes:{}", var.MapCode, var.MapName, var._MapModes));
			}

		}

		static int GetMapHostStructByCode(std::string MapCode, MapHostStruct* outmaphoststr)
		{
			for (auto& maphoststr : MapHostStructList)
			{
				if (ToLower(maphoststr.MapCode) == ToLower(MapCode))
				{
					*outmaphoststr = maphoststr;
					return 1;
				}

			}
			return 0;
		}

		static void SendNewCrc32ToAll()
		{
			t_elem const* curr;
			t_connection* conn;
			LIST_TRAVERSE_CONST(connlist(), curr)
			{
				conn = (t_connection*)elem_get_data(curr);
				if (conn && conn_get_state(conn) != conn_state_destroy)
					for (auto& s : MapHostStructList)
					{
						conn_send_ah_packet(conn, 0x16, s.crc32);
					}

			}


		}

		static void SendNewMapListToAll()
		{
			std::cout << "Update maplist to clients!" << std::endl;
			t_elem const* curr;
			t_connection* conn;
			LIST_TRAVERSE_CONST(connlist(), curr)
			{
				conn = (t_connection*)elem_get_data(curr);
				if (conn && conn_get_state(conn) != conn_state_destroy)
				{
					conn_send_ah_packet(conn, 0x3F27);
					std::cout << "1!" << std::endl;
					for (auto& s : MapHostStructList)
					{
						conn_send_ah_newmap(conn, &s);
					}
				}
			}

		}

		static bool IsCodeInMapList(std::string code)
		{
			code = ToLower(code);
			for (auto& s : MapHostStructList)
				if (ToLower(s.MapCode) == code)
					return true;
			return false;
		}

		static void ParseMap(std::string file, bool stats, t_connection* c)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "Load server config:{}", file.c_str());
			std::string printstr = "Load map ";
			eventlog(eventlog_level_error, __FUNCTION__, printstr.c_str());
			nc::Ini ini(file);

			if (ini.Load())
			{
				std::string filename = fs::path(file).filename().string();
				filename.pop_back();
				filename.pop_back();
				filename.pop_back();
				filename.pop_back();

				eventlog(eventlog_level_error, __FUNCTION__, "Ini ok. Filename:{}", filename);
				printstr += " start-> ";
				auto MapInfo = ini["MapInfo"];
				std::cout << "start1" << std::endl;
				if (MapInfo.values.size())
				{
					eventlog(eventlog_level_error, __FUNCTION__, "Mapinfo ok");
					MapHostStruct newMHStruct = MapHostStruct();
					newMHStruct.stats = stats;

					newMHStruct.MapName = MapInfo["Name"];

					printstr += newMHStruct.MapName;

					newMHStruct.MapCode = MapInfo["HostCmd"];

					printstr += " with code ";
					printstr += newMHStruct.MapCode;


					newMHStruct.MapLocalPath = filename;// MapInfo["BotPath"];
					newMHStruct.crc32 = string_to_uint32(MapInfo["ScriptCrc32"]);

					int dllcount = str_to_int(MapInfo["DllFilesCount"]);
					for (int i = 0; i < dllcount; i++)
					{
						auto DllFile = ini["DllFile_" + std::to_string(i + 1)];
						if (DllFile.values.size())
						{
							std::cout << "Need add crc32 whitelist : " << DllFile["Crc32"];
							std::cout << "," << string_to_uint32(DllFile["Crc32"]);
							std::cout << std::endl;
							uint32_t dllcrc = string_to_uint32(DllFile["Crc32"]);
							if (dllcrc != 0)
								CrcWhiteList.push_back(DllFile["Crc32"]);
						}
					}
					eventlog(eventlog_level_error, __FUNCTION__, "Dlls ok");
					printstr += " with ";
					printstr += std::to_string(dllcount) + " dlls ";
					std::cout << "start2" << std::endl;

					int playersettingscount = str_to_int(MapInfo["PlayersSettings"]);
					for (int i = 0; i < playersettingscount; i++)
					{
						auto playersetting = ini["PlayerCount_" + std::to_string(i + 1)];
						if (playersetting.values.size())
						{
							HostPlayersSettings tmpPlayerSettings = HostPlayersSettings();
							tmpPlayerSettings.ForStats = playersetting["Stats"].empty() || playersetting["Stats"] == "true";
							tmpPlayerSettings.Players = playersetting["Players"];
							newMHStruct.PlayersSettings.push_back(tmpPlayerSettings);
						}
					}
					eventlog(eventlog_level_error, __FUNCTION__, "Players ok");
					newMHStruct.DefaultPlayers = MapInfo["DefaultPlayers"];

					int modessettingscount = str_to_int(MapInfo["ModesCount"]);
					std::string _mapmodes = "";
					for (int i = 0; i < modessettingscount; i++)
					{

						auto modesettings = ini["Mode_" + std::to_string(i + 1)];
						if (modesettings.values.size())
						{
							HostModesSettings tmpModeSettings = HostModesSettings();


							tmpModeSettings.ForStats = modesettings["Stats"].empty() || modesettings["Stats"] == "true";
							int playermodesettingscount = str_to_int(modesettings["PlayersSettings"]);



							tmpModeSettings.Mode = modesettings["Mode"];

							eventlog(eventlog_level_error, __FUNCTION__, "Mode {}->{} players settings {}", tmpModeSettings.Mode, "Mode_" + std::to_string(i + 1), std::to_string(playermodesettingscount));


							for (int i = 0; i < playermodesettingscount; i++)
							{
								eventlog(eventlog_level_error, __FUNCTION__, "Mode {} players {}", tmpModeSettings.Mode, modesettings["Players_" + std::to_string(i + 1)]);
								if (modesettings["Players_" + std::to_string(i + 1)].length())
									tmpModeSettings.Players.push_back(modesettings["Players_" + std::to_string(i + 1)]);
								else
									break;
							}

							_mapmodes += " -" + tmpModeSettings.Mode;

							newMHStruct.HostModes.push_back(tmpModeSettings);

						}
					}

					std::cout << "start3" << std::endl;
					newMHStruct._MapModes = _mapmodes;

					newMHStruct.DefaultMode = MapInfo["DefaultMode"];

					newMHStruct.StatsType = MapInfo["StatsType"];
					std::cout << "start4" << std::endl;
					newMHStruct.Category = MapInfo["MapCategory"];
					std::cout << "start5" << std::endl;
					printstr += " with stats type ";
					printstr += newMHStruct.StatsType;
					std::cout << "start6" << std::endl;
					printstr += " and category ";
					printstr += newMHStruct.Category;
					std::cout << "start7" << std::endl;


					std::cout << "Add new map to maplist!!" << std::endl;
					MapHostStructList.push_back(newMHStruct);
					std::cout << "start8" << std::endl;
				}
				else
				{
					std::cout << "MAP INFO ANAL ERROR" << std::endl;
				}

			}
			else
			{
				std::cout << "INI ANAL ERROR" << std::endl;
			}
			std::cout << "End..." << std::endl;
			if (c != NULL)
				message_send_text(c, message_type_error, c, (printstr.length() >= MAX_MESSAGE_LEN ? std::string(printstr.begin(), printstr.begin() + MAX_MESSAGE_LEN - 1) : printstr));
			//ini.~Ini( );
			std::cout << "End ok" << std::endl;
			/*		SendNewCrc32ToAll( );
					std::cout << "SendNewCrc32ToAll ok" << std::endl;*/
			SendNewMapListToAll();
		}


		extern void ReloadMapHostList(t_connection* c)
		{
			InitCrcWhiteList();
			std::cout << "Clear maplist..." << std::endl;
			MapHostStructList.clear();

			fs::path curpath = fs::current_path();

			auto HostMapDir = curpath / "maphostcodes";
			auto CustomMapDir = curpath / "mapcodescustom";
			eventlog(eventlog_level_error, __FUNCTION__, "Get host maps.");
			std::vector<std::string> HostMapFiles = get_file_list(HostMapDir);
			eventlog(eventlog_level_error, __FUNCTION__, "Get chost maps.");
			std::vector<std::string> CustomMapFiles = get_file_list(CustomMapDir);

			eventlog(eventlog_level_error, __FUNCTION__, "Host file list:");
			for (auto s : HostMapFiles)
			{
				if (fs::path(s).extension().string() != ".ini")
					continue;
				eventlog(eventlog_level_error, __FUNCTION__, "{}", s.c_str());
				ParseMap(s, true, c);
			}

			eventlog(eventlog_level_error, __FUNCTION__, "Custom file list:");
			for (auto s : CustomMapFiles)
			{
				if (fs::path(s).extension().string() != ".ini")
					continue;
				eventlog(eventlog_level_error, __FUNCTION__, "{}", s.c_str());
				ParseMap(s, false, c);
			}

			SendNewCrc32ToAll();
		}

		/*static void OldParseMap( )
		{

			//auto curpath2 = curpath.( "maphostcodes" );
			std::string filenames[ ] = { "mapcodes.txt", "mapcodescustom.txt" };

			std::regex GetMapHostData( "CODE:\\{(.*?)\\}NAME:\\{(.*?)\\}PATH:\\{(.*?)\\}MODES:\\{(.*?)\\}PLAYERS:\\{(.*?)\\}CATEGORY:\\{(.*?)\\}DEFAULTPLAYERS:\\{(.*?)\\}CRC32:\\{(.*?)\\}" );
			std::regex GetMapModesOrPlayers( "(.+?)\\|" );
			std::regex IsMapStats( "(stats|nostats),(.*)" );

			std::smatch matchMapHostData;

			MapHostStructList.clear( );

			for ( std::string filename : filenames )
			{
				std::ifstream infile( filename );
				if ( infile.is_open( ) )
				{
					std::string str;
					while ( std::getline( infile, str ) )
					{
						if ( std::regex_match( str, matchMapHostData, GetMapHostData ) )
						{
							MapHostStruct newMHStruct = MapHostStruct( );
							newMHStruct.MapCode = matchMapHostData[ 1 ].str( );
							newMHStruct.MapName = matchMapHostData[ 2 ].str( );
							newMHStruct.MapLocalPath = matchMapHostData[ 3 ].str( );
							std::string tmpModesList = matchMapHostData[ 4 ].str( );
							std::string tmpPlayerList = matchMapHostData[ 5 ].str( );
							newMHStruct._MapModes = tmpModesList;
							newMHStruct._Players = tmpPlayerList;
							newMHStruct.Category = matchMapHostData[ 6 ].str( );
							newMHStruct.DefaultPlayers = matchMapHostData[ 7 ].str( );
							newMHStruct.crc32 = string_to_uint32( matchMapHostData[ 8 ].str( ) );

							if ( newMHStruct.crc32 != 0 )
							{
								eventlog( eventlog_level_error, __FUNCTION__, "Load map \"{}\" crc32 = {} ", newMHStruct.MapName, newMHStruct.crc32 );
							}

							if ( std::regex_match( newMHStruct.MapCode, matchMapHostData, IsMapStats ) )
							{
								if ( matchMapHostData[ 1 ].str( ) == "stats" )
								{
									newMHStruct.stats = true;
								}
								else
								{
									newMHStruct.stats = false;
								}
								newMHStruct.MapCode = matchMapHostData[ 2 ].str( );
							}
							else
							{
								newMHStruct.stats = false;
							}


							message_send_text( c, message_type_info, c, localize( c, "Found map:{} with code:{} and args:{}", newMHStruct.MapName, newMHStruct.MapCode, newMHStruct._MapModes ) );


							while ( std::regex_search( tmpModesList, matchMapHostData, GetMapModesOrPlayers ) )
							{

								//	message_send_text( c, message_type_error, c, localize( c, "arg:{}", matchMapHostData[ 1 ].str( ) ) );
								newMHStruct.MapModes.push_back( matchMapHostData[ 1 ].str( ) );

								tmpModesList = matchMapHostData.suffix( ).str( );
								//tmpModesList = matchMapHostData.suffix( ).str( );
							}


							while ( std::regex_search( tmpPlayerList, matchMapHostData, GetMapModesOrPlayers ) )
							{
								message_send_text( c, message_type_error, c, localize( c, "players:{}", matchMapHostData[ 1 ].str( ) ) );
								newMHStruct.Players.push_back( matchMapHostData[ 1 ].str( ) );


								tmpPlayerList = matchMapHostData.suffix( ).str( );
							}

							MapHostStructList.push_back( newMHStruct );
						}
						else
						{
							message_send_text( c, message_type_error, c, localize( c, "Found bad line:{}", str ) );
						}
					}
				}

				infile.close( );
			}
		}*/

		static char const* bnclass_get_str(unsigned int cclass)
		{
			switch (cclass)
			{
			case PLAYERINFO_DRTL_CLASS_WARRIOR:
				return "warrior";
			case PLAYERINFO_DRTL_CLASS_ROGUE:
				return "rogue";
			case PLAYERINFO_DRTL_CLASS_SORCERER:
				return "sorcerer";
			default:
				return "unknown";
			}
		}

		/*
		* Split text by spaces and return array of arguments.
		*   First text argument is a command name (index = 0)
		*   Last text argument always reads to end
		*/
		extern std::vector<std::string> split_command(char const* text, int args_count)
		{
			std::vector<std::string> result(args_count + 1);

			std::string s(text);
			// remove slash from the command
			if (!s.empty())
				s.erase(0, 1);

			std::istringstream iss(s);

			int i = 0;
			std::string tmp = std::string(); // to end
			do
			{
				std::string sub;
				iss >> sub;

				if (sub.empty())
					continue;

				if (i < args_count)
				{
					result[i] = sub;
					i++;
				}
				else
				{
					if (!tmp.empty())
						tmp += " ";
					tmp += sub;
				}

			} while (iss);

			// push remaining text at the end
			if (tmp.length() > 0)
				result[args_count] = tmp;

			return result;
		}
		std::string msgt;
		static void do_whisper(t_connection* user_c, char const* dest, char const* text)
		{
			t_connection* dest_c;
			char const* tname;

			if (account_get_auth_mute(conn_get_account(user_c)) == 1)
			{
				message_send_text(user_c, message_type_error, user_c, localize(user_c, "Your account has been muted, you can't whisper to other users."));
				return;
			}

			if (!(dest_c = connlist_find_connection_by_name(dest, conn_get_realm(user_c))))
			{
				message_send_text(user_c, message_type_error, user_c, localize(user_c, "That user is not logged on."));
				return;
			}


#ifdef WITH_LUA
			if (lua_handle_user(user_c, dest_c, text, luaevent_user_whisper) == 1)
				return;
#endif

			if (conn_get_dndstr(dest_c))
			{
				msgtemp = localize(user_c, "{} is unavailable ({})", conn_get_username(dest_c), conn_get_dndstr(dest_c));
				message_send_text(user_c, message_type_info, user_c, msgtemp);
				return;
			}

			message_send_text(user_c, message_type_whisperack, dest_c, text);

			if (conn_get_awaystr(dest_c))
			{
				msgtemp = localize(user_c, "{} is away ({})", conn_get_username(dest_c), conn_get_awaystr(dest_c));
				message_send_text(user_c, message_type_info, user_c, msgtemp);
			}

			message_send_text(dest_c, message_type_whisper, user_c, text);

			if ((tname = conn_get_username(user_c)))
			{
				char username[1 + MAX_USERNAME_LEN]; /* '*' + username (including NUL) */

				if (std::strlen(tname) < MAX_USERNAME_LEN)
				{
					std::sprintf(username, "*%s", tname);
					conn_set_lastsender(dest_c, username);
				}
			}
			t_account* whisperacc = conn_get_account(user_c);
			t_account* dstacc = conn_get_account(dest_c);
			account_add_newevent(whisperacc, 3);
			account_add_newevent(dstacc, 4);
		}


		static void do_whois(t_connection* c, char const* dest)
		{
			t_connection* dest_c;
			std::string  namepart, verb; /* 64 + " (" + 64 + ")" + NUL */
			t_game const* game;
			t_channel const* channel;

			if ((!(dest_c = connlist_find_connection_by_accountname(dest))) &&
				(!(dest_c = connlist_find_connection_by_name(dest, conn_get_realm(c)))))
			{
				t_account* dest_a;
				t_bnettime btlogin;

				if (!(dest_a = accountlist_find_account(dest))) {
					message_send_text(c, message_type_error, c, localize(c, "Unknown user."));
					return;
				}

				if (conn_get_class(c) == conn_class_bnet) {
					btlogin = time_to_bnettime((std::time_t)account_get_ll_time(dest_a), 0);
					btlogin = bnettime_add_tzbias(btlogin, conn_get_tzbias(c));
					std::time_t ulogin = bnettime_to_time(btlogin);
					struct std::tm tmlogin = {};
					if (pvpgn::gmtime_s(&ulogin, &tmlogin) == nullptr)
						std::strcpy(msgtemp0, "?");
					else
						std::strftime(msgtemp0, sizeof(msgtemp0), "%a %b %d %H:%M:%S", &tmlogin);
					msgtemp = localize(c, "User was last seen on: {}", msgtemp0);
				}
				else
					msgtemp = localize(c, "User is offline");
				message_send_text(c, message_type_info, c, msgtemp);
				return;
			}

			if (c == dest_c)
			{
				namepart = localize(c, "You");
				verb = localize(c, "are");
			}
			else
			{
				char const* tname;

				namepart = (tname = conn_get_chatcharname(dest_c, c));
				conn_unget_chatcharname(dest_c, tname);
				verb = localize(c, "is");
			}

			if ((game = conn_get_game(dest_c)))
			{

				msgtemp = localize(c, "{} {} using {} and {} currently in {} game \"{}\".",
					namepart,
					verb,
					clienttag_get_title(conn_get_clienttag(dest_c)),
					verb,

					game_get_flag(game) == game_flag_private ? localize(c, "private") : "",
					game_get_name(game));
			}
			else if ((channel = conn_get_channel(dest_c)))
			{
				msgtemp = localize(c, "{} {} using {} and {} currently in channel \"{}\".",
					namepart,
					verb,
					clienttag_get_title(conn_get_clienttag(dest_c)),
					verb,
					channel_get_name(channel));
			}
			else
				msgtemp = localize(c, "{} {} using {}.",
					namepart,
					verb,
					clienttag_get_title(conn_get_clienttag(dest_c)));
			message_send_text(c, message_type_info, c, msgtemp);

			if (conn_get_dndstr(dest_c))
			{
				msgtemp = localize(c, "{} {} refusing messages ({})",
					namepart,
					verb,
					conn_get_dndstr(dest_c));
				message_send_text(c, message_type_info, c, msgtemp);
			}
			else
				if (conn_get_awaystr(dest_c))
				{
					msgtemp = localize(c, "{} away ({})",
						namepart,
						conn_get_awaystr(dest_c));
					message_send_text(c, message_type_info, c, msgtemp);
				}
		}


		static void user_timer_cb(t_connection* c, std::time_t now, t_timer_data str)
		{
			if (!c)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return;
			}
			if (!str.p)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
				return;
			}

			if (now != (std::time_t)0) /* zero means user logged out before expiration */
				message_send_text(c, message_type_info, c, (char*)str.p);
			xfree(str.p);
		}

		typedef int(*t_command)(t_connection* c, char const* text);

		typedef struct {
			const char* command_string;
			t_command    command_handler;
		} t_command_table_row;

		static int command_set_flags(t_connection* c); // [Omega]
		// command handler prototypes
		static int _handle_clan_command(t_connection* c, char const* text);
		static int _handle_admin_command(t_connection* c, char const* text);
		static int _handle_aop_command(t_connection* c, char const* text);
		static int _handle_op_command(t_connection* c, char const* text);
		static int _handle_tmpop_command(t_connection* c, char const* text);
		static int _handle_deop_command(t_connection* c, char const* text);
		static int _handle_voice_command(t_connection* c, char const* text);
		static int _handle_devoice_command(t_connection* c, char const* text);
		static int _handle_vop_command(t_connection* c, char const* text);
		static int _handle_friends_command(t_connection* c, char const* text);
		static int _handle_me_command(t_connection* c, char const* text);
		static int _handle_whisper_command(t_connection* c, char const* text);
		static int _handle_status_command(t_connection* c, char const* text);
		static int _handle_who_command(t_connection* c, char const* text);
		static int _handle_whois_command(t_connection* c, char const* text);
		static int _handle_whoami_command(t_connection* c, char const* text);
		static int _handle_announce_command(t_connection* c, char const* text);
		static int _handle_beep_command(t_connection* c, char const* text);
		static int _handle_nobeep_command(t_connection* c, char const* text);
		static int _handle_version_command(t_connection* c, char const* text);
		static int _handle_copyright_command(t_connection* c, char const* text);
		static int _handle_uptime_command(t_connection* c, char const* text);
		static int _handle_stats_command(t_connection* c, char const* text);
		static int _handle_time_command(t_connection* c, char const* text);
		static int _handle_channel_command(t_connection* c, char const* text);
		static int _handle_rejoin_command(t_connection* c, char const* text);
		static int _handle_away_command(t_connection* c, char const* text);
		static int _handle_dnd_command(t_connection* c, char const* text);
		static int _handle_squelch_command(t_connection* c, char const* text);
		static int _handle_unsquelch_command(t_connection* c, char const* text);
		static int _handle_kick_command(t_connection* c, char const* text);
		static int _handle_ban_command(t_connection* c, char const* text);
		static int _handle_unban_command(t_connection* c, char const* text);
		static int _handle_reply_command(t_connection* c, char const* text);
		static int _handle_realmann_command(t_connection* c, char const* text);
		static int _handle_watch_command(t_connection* c, char const* text);
		static int _handle_unwatch_command(t_connection* c, char const* text);
		static int _handle_watchall_command(t_connection* c, char const* text);
		static int _handle_unwatchall_command(t_connection* c, char const* text);
		static int _handle_lusers_command(t_connection* c, char const* text);
		static int _handle_news_command(t_connection* c, char const* text);
		static int _handle_games_command(t_connection* c, char const* text);
		static int _handle_channels_command(t_connection* c, char const* text);
		static int _handle_addacct_command(t_connection* c, char const* text);
		static int _handle_chpass_command(t_connection* c, char const* text);
		static int _handle_connections_command(t_connection* c, char const* text);
		static int _handle_finger_command(t_connection* c, char const* text);
		static int _handle_operator_command(t_connection* c, char const* text);
		static int _handle_admins_command(t_connection* c, char const* text);
		static int _handle_quit_command(t_connection* c, char const* text);
		static int _handle_kill_command(t_connection* c, char const* text);
		static int _handle_killsession_command(t_connection* c, char const* text);
		static int _handle_gameinfo_command(t_connection* c, char const* text);
		static int _handle_ladderactivate_command(t_connection* c, char const* text);
		static int _handle_rehash_command(t_connection* c, char const* text);
		static int _handle_find_command(t_connection* c, char const* text);
		static int _handle_save_command(t_connection* c, char const* text);

		static int _handle_shutdown_command(t_connection* c, char const* text);
		static int _handle_ladderinfo_command(t_connection* c, char const* text);
		static int _handle_timer_command(t_connection* c, char const* text);
		static int _handle_serverban_command(t_connection* c, char const* text);
		static int _handle_netinfo_command(t_connection* c, char const* text);
		static int _handle_quota_command(t_connection* c, char const* text);
		static int _handle_lockacct_command(t_connection* c, char const* text);
		static int _handle_unlockacct_command(t_connection* c, char const* text);
		static int _handle_muteacct_command(t_connection* c, char const* text);
		static int _handle_unmuteacct_command(t_connection* c, char const* text);
		static int _handle_flag_command(t_connection* c, char const* text);
		static int _handle_tag_command(t_connection* c, char const* text);
		static int _handle_ipscan_command(t_connection* c, char const* text);
		static int _handle_set_command(t_connection* c, char const* text);
		static int _handle_motd_command(t_connection* c, char const* text);
		static int _handle_ping_command(t_connection* c, char const* text);
		static int _handle_commandgroups_command(t_connection* c, char const* text);
		static int _handle_topic_command(t_connection* c, char const* text);
		static int _handle_moderate_command(t_connection* c, char const* text);
		static int _handle_clearstats_command(t_connection* c, char const* text);
		static int _handle_tos_command(t_connection* c, char const* text);
		static int _handle_alert_command(t_connection* c, char const* text);
		static int _handle_email_command(t_connection * c, char const * text);
		//here
		static int _handle_setahversion_command(t_connection* c, char const* text);
		static int _handle_setlauncherversion_command(t_connection* c, char const* text);
		static int _handle_updatewhitelistfile_command(t_connection* c, char const* text);
		static int _handle_updatecrcwhitelistfile_command(t_connection* c, char const* text);
		static int _handle_updateblacklistfile_command(t_connection* c, char const* text);
		static int _handle_fmt_command(t_connection* c, char const* text);
		static int _handle_yellow_command(t_connection* c, char const* text);
		static int _handle_blue_command(t_connection* c, char const* text);
		static int _handle_green_command(t_connection* c, char const* text);
		static int _handle_usercolor_command(t_connection* c, char const* text);
		static int _handle_setstats_command(t_connection* c, char const* text);
		static int _handle_addstats_command(t_connection* c, char const* text);
		static int _handle_cstats_command(t_connection* c, char const* text);
		static int _handle_setstatstype_command(t_connection* c, char const* text);
		static int _handle_setgametop_command(t_connection* c, char const* text);
		static int _handle_setptslimit_command(t_connection* c, char const* text);
		static int _handle_follow_command(t_connection* c, char const* text);
		static int _handle_hidegame_command(t_connection* c, char const* text);
		static int _handle_addcomment_command(t_connection* c, char const* text);
		static int _handle_delcomment_command(t_connection* c, char const* text);
		static int _handle_showcomment_command(t_connection* c, char const* text);
		static int _handle_reloadmaplist_command(t_connection* c, char const* text);
		static int _handle_maplist_command(t_connection* c, char const* text);
		static int _handle_host_command(t_connection* c, char const* text);
		static int _handle_chost_command(t_connection* c, char const* text);
		static int _handle_phost_command(t_connection* c, char const* text);
		static int _handle_unhost_command(t_connection* c, char const* text);
		static int _handle_swap_command(t_connection* c, char const* text);
		static int _handle_hcl_command(t_connection* c, char const* text);
		static int _handle_open_command(t_connection* c, char const* text);
		static int _handle_comp_command(t_connection* c, char const* text);
		static int _handle_corace_command(t_connection* c, char const* text);
		static int _handle_start_command(t_connection* c, char const* text);
		static int _handle_abort_command(t_connection* c, char const* text);
		static int _handle_hold_command(t_connection* c, char const* text);
		static int _handle_holdslots_command(t_connection* c, char const* text);
		static int _handle_gamemessage_command(t_connection* c, char const* text);
		static int _handle_banrequest_command(t_connection* c, char const* text);
		static int _handle_cclearstats_command(t_connection* c, char const* text);
		static int _handle_setahcrcvalue_command(t_connection* c, char const* text);
		static int _handle_rename_warcis_acc_command(t_connection* c, char const* text);
		static int _handle_check_warcis_acc_command(t_connection* c, char const* text);
		static int _handle_activate_warcis_acc_command(t_connection* c, char const* text);
		static int _handle_set_warcis_acc_command(t_connection* c, char const* text);
		static int _handle_testmessagebox_command(t_connection* c, char const* text);

		static const t_command_table_row standard_command_table[] =
		{
			{ "/clan", _handle_clan_command },
			{ "/c", _handle_clan_command },
			{ "/admin", _handle_admin_command },
			{ "/f", _handle_friends_command },
			{ "/friends", _handle_friends_command },
			{ "/me", _handle_me_command },
			{ "/emote", _handle_me_command },
			{ "/msg", _handle_whisper_command },
			{ "/whisper", _handle_whisper_command },
			{ "/w", _handle_whisper_command },
			{ "/m", _handle_whisper_command },
			{ "/status", _handle_status_command },
			{ "/users", _handle_status_command },
			{ "/who", _handle_who_command },
			{ "/whois", _handle_whois_command },
			{ "/whereis", _handle_whois_command },
			{ "/where", _handle_whois_command },
			{ "/whoami", _handle_whoami_command },
			{ "/announce", _handle_announce_command },
			{ "/beep", _handle_beep_command },
			{ "/nobeep", _handle_nobeep_command },
			{ "/version", _handle_version_command },
			{ "/copyright", _handle_copyright_command },
			{ "/warranty", _handle_copyright_command },
			{ "/license", _handle_copyright_command },
			{ "/uptime", _handle_uptime_command },
			{ "/stats", _handle_stats_command },
			{ "/astat", _handle_stats_command },
			{ "/time", _handle_time_command },
			{ "/channel", _handle_channel_command },
			{ "/join", _handle_channel_command },
			{ "/j", _handle_channel_command },
			{ "/rejoin", _handle_rejoin_command },
			{ "/away", _handle_away_command },
			{ "/dnd", _handle_dnd_command },
			{ "/ignore", _handle_squelch_command },
			{ "/squelch", _handle_squelch_command },
			{ "/unignore", _handle_unsquelch_command },
			{ "/unsquelch", _handle_unsquelch_command },
			{ "/kick", _handle_kick_command },
			{ "/ban", _handle_ban_command },
			{ "/unban", _handle_unban_command },
			{ "/tos", _handle_tos_command },

			{ "/ann", _handle_announce_command },
			{ "/r", _handle_reply_command },
			{ "/reply", _handle_reply_command },
			{ "/realmann", _handle_realmann_command },
			{ "/watch", _handle_watch_command },
			{ "/unwatch", _handle_unwatch_command },
			/*	{ "/watchall", _handle_watchall_command },  //disabled*/
			{ "/unwatchall", _handle_unwatchall_command },
			{ "/lusers", _handle_lusers_command },
			{ "/news", _handle_news_command },
			{ "/games", _handle_games_command },
			{ "/channels", _handle_channels_command },
			{ "/chs", _handle_channels_command },
			{ "/register_warcis_acc", _handle_addacct_command },
			{ "/rename_warcis_acc", _handle_rename_warcis_acc_command },
			{ "/check_warcis_acc", _handle_check_warcis_acc_command },
			{ "/activate_warcis_acc", _handle_activate_warcis_acc_command },
			{ "/set_warcis_acc", _handle_set_warcis_acc_command },

			{ "/chpass", _handle_chpass_command },
			/*	{ "/connections", _handle_connections_command },  //disabled*/
			/*{ "/con", _handle_connections_command },  //disabled*/
			{ "/finger", _handle_finger_command },
			{ "/operator", _handle_operator_command },
			{ "/aop", _handle_aop_command },
			{ "/op", _handle_op_command },
			{ "/tmpop", _handle_tmpop_command },
			{ "/deop", _handle_deop_command },
			{ "/voice", _handle_voice_command },
			{ "/devoice", _handle_devoice_command },
			{ "/vop", _handle_vop_command },
			{ "/admins", _handle_admins_command },
			{ "/logout", _handle_quit_command },
			{ "/quit", _handle_quit_command },
			{ "/exit", _handle_quit_command },
			{ "/kill", _handle_kill_command },
			{ "/killsession", _handle_killsession_command },
			{ "/gameinfo", _handle_gameinfo_command },
			{ "/ladderactivate", _handle_ladderactivate_command },
			{ "/rehash", _handle_rehash_command },
			{ "/find", _handle_find_command },
			{ "/save", _handle_save_command },
			{ "/shutdown", _handle_shutdown_command },
			{ "/ladderinfo", _handle_ladderinfo_command },
			{ "/timer", _handle_timer_command },
			{ "/serverban", _handle_serverban_command },
			//{ "/netinfo", _handle_netinfo_command },
			{ "/quota", _handle_quota_command },
			{ "/lockacct", _handle_lockacct_command },
			{ "/lock", _handle_lockacct_command },
			{ "/unlockacct", _handle_unlockacct_command },
			{ "/unlock", _handle_unlockacct_command },
			{ "/muteacct", _handle_muteacct_command },
			{ "/mute", _handle_muteacct_command },
			{ "/unmuteacct", _handle_unmuteacct_command },
			{ "/unmute", _handle_unmuteacct_command },
			{ "/flag", _handle_flag_command },
			{ "/tag", _handle_tag_command },
			{ "/help", handle_help_command },
			{ "/?", handle_help_command },
			{ "/mail", handle_mail_command },
			{ "/ipban", handle_ipban_command }, // in ipban.c
			{ "/ipscan", _handle_ipscan_command },
			{ "/set", _handle_set_command },
			{ "/motd", _handle_motd_command },
			{ "/latency", _handle_ping_command },
			{ "/ping", _handle_ping_command },
			{ "/p", _handle_ping_command },
			{ "/commandgroups", _handle_commandgroups_command },
			{ "/cg", _handle_commandgroups_command },
			{ "/topic", _handle_topic_command },
			{ "/moderate", _handle_moderate_command },
			{ "/clearstats", _handle_clearstats_command },
			{ "/icon", handle_icon_command },
			{ "/alert", _handle_alert_command },
			{ "/language", handle_language_command },
			{ "/lang", handle_language_command },
			{ "/log", handle_log_command },
			{ "/email", _handle_email_command},
			//here
			{ "/setahversion", _handle_setahversion_command },
			{ "/setlauncherversion", _handle_setlauncherversion_command },
			{ "/updatewhitelist", _handle_updatewhitelistfile_command },
			{ "/updatecrcwhitelist", _handle_updatecrcwhitelistfile_command },
			{ "/updateblacklist", _handle_updateblacklistfile_command },
			{ "/green", _handle_green_command },
			{ "/blue", _handle_blue_command },
			{ "/yellow", _handle_yellow_command },
			{ "/fmt", _handle_fmt_command },
			{ "/usercolor", _handle_usercolor_command },
			{ "/setstats",_handle_setstats_command },
			{ "/addstats",_handle_addstats_command },
			{ "/cstats",_handle_cstats_command },
			{ "/setgametop",_handle_setgametop_command },
			{ "/setptslimit",_handle_setptslimit_command },
			{ "/follow",_handle_follow_command },
			{ "/setstatstype",_handle_setstatstype_command },
			{ "/hidegame",_handle_hidegame_command },
			{ "/addcomment",_handle_addcomment_command },
			{ "/showcomments",_handle_showcomment_command },
			{ "/delcomment",_handle_delcomment_command },
			{ "/chost",_handle_chost_command },
			{ "/host",_handle_host_command },
			{ "/phost",_handle_phost_command },
			{ "/unhost",_handle_unhost_command },
			{ "/swap",_handle_swap_command },
			{ "/open",_handle_open_command },
			{ "/hcl",_handle_hcl_command },
			{ "/comp",_handle_comp_command },
			{ "/corace",_handle_corace_command },
			{ "/start",_handle_start_command },
			{ "/abort",_handle_abort_command },
			{ "/hold", _handle_hold_command },
			{ "/holdslots", _handle_holdslots_command },
			{ "/reloadmaplist",_handle_reloadmaplist_command },
			{ "/maplist",_handle_maplist_command },
			{ "/gamemessage",_handle_gamemessage_command },
			{ "/banrequest",_handle_banrequest_command },
			{ "/cclearstats", _handle_cclearstats_command },
			{ "/setahcrcvalue", _handle_setahcrcvalue_command },
			{ "/testmessagebox", _handle_testmessagebox_command },

			//
			{ NULL, NULL }

		};

		extern int handle_command(t_connection* c, char const* text)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got command \"{}\"", text);

			t_account* sender = conn_get_account(c);
			int result = 0;
			t_command_table_row const* p;

#ifdef WITH_LUA
			// feature to ignore flood protection
			result = lua_handle_command(c, text, luaevent_command_before);
#endif
			if (result == -1)
			{
				if (sender && account_get_auth_botlogin(sender))
				{
					//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
				}

				return result;
			}

			if (result == 0)
				if ((text[0] != '\0') && (conn_quota_exceeded(c, text)))
				{
					msgtemp = localize(c, "You are sending commands to {} too quickly and risk being disconnected for flooding. Please slow down.", prefs_get_servername());
					message_send_text(c, message_type_error, c, msgtemp);
					if (sender && account_get_auth_botlogin(sender))
					{
						//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
					}

					return 0;
				}

#ifdef WITH_LUA
			result = lua_handle_command(c, text, luaevent_command);
			// -1 = unsuccess, 0 = success, 1 = execute next c++ code
			if (result == 0)
			{
				// log command
				if (t_account* account = conn_get_account(c))
					userlog_append(account, text);
			}
			if (result == 0 || result == -1)
				return result;
#endif

			for (p = standard_command_table; p->command_string != NULL; p++)
			{
				if (strstart(text, p->command_string) == 0)
				{
					if (!(command_get_group(p->command_string)))
					{
						message_send_text(c, message_type_error, c, localize(c, "This command has been deactivated"));
						if (sender && account_get_auth_botlogin(sender))
						{
							//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
						}

						return 0;
					}
					if (!((command_get_group(p->command_string) & account_get_command_groups(conn_get_account(c)))))
					{
						message_send_text(c, message_type_error, c, localize(c, "This command is reserved for admins."));
						if (sender && account_get_auth_botlogin(sender))
						{
							//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
						}

						return 0;
					}
					if (p->command_handler != NULL)
					{
						result = ((p->command_handler)(c, text));
						// -1 = unsuccess, 0 = success
						if (result == 0)
						{
							// log command
							if (t_account* account = conn_get_account(c))
								userlog_append(account, text);
						}

						if (sender && account_get_auth_botlogin(sender))
						{
							//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
						}

						return result;
					}
				}
			}

			/*if ( std::strlen( text ) >= 2 && std::strncmp( text, "//", 2 ) == 0 )
			{
				handle_alias_command( c, text );
				return 0;
			}*/

			message_send_text(c, message_type_error, c, localize(c, "Unknown command."));
			eventlog(eventlog_level_debug, __FUNCTION__, "got unknown command \"{}\"", text);
			if (sender && account_get_auth_botlogin(sender))
			{
				//message_send_text(c, message_type_error, c, "BOTCMDHANDLEEND");
			}

			return -1;
		}





		// +++++++++++++++++++++++++++++++++ command implementations +++++++++++++++++++++++++++++++++++++++

		static int _handle_clan_command(t_connection* c, char const* text)
		{
			t_account* acc;
			t_clanmember* member;
			t_clan* clan;

			if (!(acc = conn_get_account(c))) {
				ERROR0("got NULL account");
			}

			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}

			/* FIXME: can get clan as is in creating process */

			// user in clan
			if ((member = account_get_clanmember_forced(acc)) && (clan = clanmember_get_clan(member)))
			{
				// user is full member of clan
				if (clanmember_get_fullmember(member) == 1)
				{
					if (args[1] == "msg" || args[1] == "m" || args[1] == "w" || args[1] == "whisper")
					{
						if (args[2].empty()) {
							describe_command(c, args[0].c_str());
							return -1;
						}
						char const* msg = args[2].c_str(); // message

						if (clan_send_message_to_online_members(clan, message_type_whisper, c, msg) >= 1)
							message_send_text(c, message_type_info, c, localize(c, "Message was sent to all currently available clan members."));
						else
							message_send_text(c, message_type_info, c, localize(c, "All fellow members of your clan are currently offline."));

						return 0;
					}

					if (clanmember_get_status(member) >= CLAN_SHAMAN)
					{
						if (args[1] == "public" || args[1] == "pub")
						{
							if (clan_get_channel_type(clan) != 0) {
								clan_set_channel_type(clan, 0);
								message_send_text(c, message_type_info, c, localize(c, "Clan channel is opened up!"));
							}
							else
								message_send_text(c, message_type_error, c, localize(c, "Clan channel has already been opened up!"));
							return 0;
						}
						else if (args[1] == "private" || args[1] == "priv")
						{
							if (clan_get_channel_type(clan) != 1) {
								clan_set_channel_type(clan, 1);
								message_send_text(c, message_type_info, c, localize(c, "Clan channel is closed!"));
							}
							else
								message_send_text(c, message_type_error, c, localize(c, "Clan channel has already been closed!"));
							return 0;
						}
						else if (args[1] == "motd")
						{
							if (args[2].empty())
							{
								describe_command(c, args[0].c_str());
								return 0;
							}
							const char* msg = args[2].c_str(); // message

							clan_set_motd(clan, msg);
							message_send_text(c, message_type_info, c, localize(c, "Clan message of day is updated!"));
							return 0;
						}
						else if (args[1] == "invite" || args[1] == "inv")
						{
							t_account* dest_account;
							t_connection* dest_conn;
							if (args[2].empty())
							{
								describe_command(c, args[0].c_str());
								return -1;
							}
							const char* username = args[2].c_str();

							if ((dest_account = accountlist_find_account(username)) && (dest_conn = account_get_conn(dest_account))
								&& (account_get_clan(dest_account) == NULL) && (account_get_creating_clan(dest_account) == NULL))
							{
								if (prefs_get_clan_newer_time() > 0)
									clan_add_member(clan, dest_account, CLAN_NEW);
								else
									clan_add_member(clan, dest_account, CLAN_PEON);
								msgtemp = localize(c, "User {} was invited to your clan!", username);
								message_send_text(c, message_type_error, c, msgtemp);
								msgtemp = localize(c, "You are invited to {} by {}!", clan_get_name(clan), conn_get_chatname(c));
								message_send_text(dest_conn, message_type_error, c, msgtemp);
							}
							else {
								msgtemp = localize(c, "User {} is not online or is already member of clan!", username);
								message_send_text(c, message_type_error, c, msgtemp);
							}
							return 0;
						}
						else if (args[1] == "disband" || args[1] == "dis")
						{
							if (args[2] != "yes") {
								message_send_text(c, message_type_info, c, localize(c, "This is one-way action! If you really want"));
								message_send_text(c, message_type_info, c, localize(c, "to disband your clan, type /clan disband yes"));
							}
							/* PELISH: fixme - Find out better solution! */
							if (clanlist_remove_clan(clan) == 0) {
								if (clan_get_created(clan) == 1)
									clan_remove(clan_get_clantag(clan));
								clan_destroy(clan);
								message_send_text(c, message_type_info, c, localize(c, "Your clan was disbanded."));
							}
							return 0;
						}
					}
				}
				// user is not full member (invitation was not accepted yet)
				else if (clanmember_get_fullmember(member) == 0)
				{
					/* User is not in clan, but he can accept invitation to someone */
					if (args[1] != "invite" && args[1] != "inv")
					{
						describe_command(c, args[0].c_str());
						return -1;
					}

					if (args[2] == "get") {
						msgtemp = localize(c, "You have been invited to {}", clan_get_name(clan));
						message_send_text(c, message_type_info, c, msgtemp);
						return 0;
					}
					else if (args[2] == "accept" || args[2] == "acc") {
						int created = clan_get_created(clan);

						clanmember_set_fullmember(member, 1);
						clanmember_set_join_time(member, std::time(NULL));
						msgtemp = localize(c, "You are now a clan member of {}", clan_get_name(clan));
						message_send_text(c, message_type_info, c, msgtemp);
						if (created > 0) {
							DEBUG1("clan {} has already been created", clan_get_name(clan));
							return -1;
						}
						created++;
						if (created >= 0) {
							clan_set_created(clan, 1);
							clan_set_creation_time(clan, std::time(NULL));
							/* FIXME: send message "CLAN was be created" to members */
							msgtemp = localize(c, "Clan {} was be created", clan_get_name(clan));
							clan_send_message_to_online_members(clan, message_type_whisper, c, msgtemp.c_str()); /* Send message to all members */
							message_send_text(c, message_type_whisper, c, msgtemp);                      /* also to self */
							clan_save(clan);
						}
						else
							clan_set_created(clan, created);
						return 0;
					}
					else if (args[2] == "decline" || args[2] == "dec") {
						clan_remove_member(clan, member);
						msgtemp = localize(c, "You are no longer ivited to {}", clan_get_name(clan));
						message_send_text(c, message_type_info, c, msgtemp);
						return 0;
					}
				}

				if ((args[1] == "create" || args[1] == "cre"))
				{
					message_send_text(c, message_type_error, c, localize(c, "You are already in clan \"{}\"", clan_get_name(clan)));
					return -1;
				}
			}
			// user not in clan
			else
			{
				if ((args[1] == "create" || args[1] == "cre"))
				{
					char const* clantag, * clanname;
					std::vector<std::string> args = split_command(text, 3);

					if (args[3].empty())
					{
						describe_command(c, args[0].c_str());
						return -1;
					}
					clantag = args[2].c_str(); // clan tag
					clanname = args[3].c_str(); // clan name

					if (clan = clanlist_find_clan_by_clantag(str_to_clantag(clantag))) {
						message_send_text(c, message_type_error, c, localize(c, "Clan with your specified <clantag> already exist!"));
						message_send_text(c, message_type_error, c, localize(c, "Please choice another one."));
						return -1;
					}

					if ((clan = clan_create(conn_get_account(c), str_to_clantag(clantag), clanname, NULL)) && clanlist_add_clan(clan))
					{
						member = account_get_clanmember_forced(acc);
						if (prefs_get_clan_min_invites() == 0) {
							clan_set_created(clan, 1);
							clan_set_creation_time(clan, std::time(NULL));
							msgtemp = localize(c, "Clan {} is created!", clan_get_name(clan));
							message_send_text(c, message_type_info, c, msgtemp);
							clan_save(clan);
						}
						else {
							clan_set_created(clan, -prefs_get_clan_min_invites() + 1); //Pelish: +1 means that creator of clan is already invited
							msgtemp = localize(c, "Clan {} is pre-created, please invite", clan_get_name(clan));
							message_send_text(c, message_type_info, c, msgtemp);
							msgtemp = localize(c, "at least {} players to your clan by using", prefs_get_clan_min_invites());
							message_send_text(c, message_type_info, c, msgtemp);
							message_send_text(c, message_type_info, c, localize(c, "/clan invite <username> command."));
						}
					}
					return 0;
				}
			}

			describe_command(c, args[0].c_str());
			return -1;
		}

		static int command_set_flags(t_connection* c)
		{
			return channel_set_userflags(c);
		}

		static int _handle_admin_command(t_connection* c, char const* text)
		{
			char const* username;
			char		command;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty() || (args[1][0] != '+' && args[1][0] != '-')) {
				describe_command(c, args[0].c_str());
				return -1;
			}

			text = args[1].c_str();
			command = text[0]; // command type (+/-)
			username = &text[1]; // username

			if (!*username) {
				message_send_text(c, message_type_info, c, localize(c, "You must supply a username."));
				return -1;
			}

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}
			dst_c = account_get_conn(acc);

			if (command == '+') {
				if (account_get_auth_admin(acc, NULL) == 1) {
					msgtemp = localize(c, "{} is already a Server Admin", username);
				}
				else {
					account_set_auth_admin(acc, NULL, 1);
					msgtemp = localize(c, "{} has been promoted to a Server Admin", username);
					msgtemp2 = localize(c, "{} has promoted you to a Server Admin", conn_get_loggeduser(c));
					changed = 1;
				}
			}
			else {
				if (account_get_auth_admin(acc, NULL) != 1)
					msgtemp = localize(c, "{} is not a Server Admin.", username);
				else {
					account_set_auth_admin(acc, NULL, 0);
					msgtemp = localize(c, "{} has been demoted from a Server Admin", username);
					msgtemp2 = localize(c, "{} has demoted you from a Server Admin", conn_get_loggeduser(c));
					changed = 1;
				}
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_operator_command(t_connection* c, char const* text)
		{
			char const* username;
			char		command;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty() || (args[1][0] != '+' && args[1][0] != '-')) {
				describe_command(c, args[0].c_str());
				return -1;
			}

			text = args[1].c_str();
			command = text[0]; // command type (+/-)
			username = &text[1]; // username

			if (!*username) {
				message_send_text(c, message_type_info, c, localize(c, "You must supply a username."));
				return -1;
			}

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}
			dst_c = account_get_conn(acc);

			if (command == '+') {
				if (account_get_auth_operator(acc, NULL) == 1)
					msgtemp = localize(c, "{} is already a Server Operator", username);
				else {
					account_set_auth_operator(acc, NULL, 1);
					msgtemp = localize(c, "{} has been promoted to a Server Operator", username);
					msgtemp2 = localize(c, "{} has promoted you to a Server Operator", conn_get_loggeduser(c));
					changed = 1;
				}
			}
			else {
				if (account_get_auth_operator(acc, NULL) != 1)
					msgtemp = localize(c, "{} is no Server Operator, so you can't demote him", username);
				else {
					account_set_auth_operator(acc, NULL, 0);
					msgtemp = localize(c, "{} has been demoted from a Server Operator", username);
					msgtemp2 = localize(c, "{} has promoted you to a Server Operator", conn_get_loggeduser(c));
					changed = 1;
				}
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_aop_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (account_get_auth_admin(conn_get_account(c), NULL) != 1 && account_get_auth_admin(conn_get_account(c), channel) != 1) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Admin to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}

			dst_c = account_get_conn(acc);

			if (account_get_auth_admin(acc, channel) == 1)
				msgtemp = localize(c, "{} is already a Channel Admin", username);
			else {
				account_set_auth_admin(acc, channel, 1);
				msgtemp = localize(c, "{} has been promoted to a Channel Admin", username);
				msgtemp2 = localize(c, "{} has promoted you to a Channel Admin for channel \"{}\"", conn_get_loggeduser(c), channel);
				changed = 1;
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_vop_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (account_get_auth_admin(conn_get_account(c), NULL) != 1 && account_get_auth_admin(conn_get_account(c), channel) != 1) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Admin to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}

			dst_c = account_get_conn(acc);

			if (account_get_auth_voice(acc, channel) == 1)
				msgtemp = localize(c, "{} is already on VOP list", username);
			else {
				account_set_auth_voice(acc, channel, 1);
				msgtemp = localize(c, "{} has been added to the VOP list", username);
				msgtemp2 = localize(c, "{} has added you to the VOP list of channel \"{}\"", conn_get_loggeduser(c), channel);
				changed = 1;
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_voice_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (!(account_is_operator_or_admin(conn_get_account(c), channel_get_name(conn_get_channel(c))))) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}
			dst_c = account_get_conn(acc);
			if (account_get_auth_voice(acc, channel) == 1)
				msgtemp = localize(c, "{} is already on VOP list, no need to Voice him", username);
			else
			{
				if ((!dst_c) || conn_get_channel(c) != conn_get_channel(dst_c))
				{
					msgtemp = localize(c, "{} must be on the same channel to voice him", username);
				}
				else
				{
					if (channel_conn_has_tmpVOICE(conn_get_channel(c), dst_c))
						msgtemp = localize(c, "{} already has Voice in this channel", username);
					else {
						if (account_is_operator_or_admin(acc, channel))
							msgtemp = localize(c, "{} is already an operator or admin.", username);
						else
						{
							conn_set_tmpVOICE_channel(dst_c, channel);
							msgtemp = localize(c, "{} has been granted Voice in this channel", username);
							msgtemp2 = localize(c, "{} has granted you Voice in this channel", conn_get_loggeduser(c));
							changed = 1;
						}
					}
				}
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_devoice_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			t_connection* dst_c;
			int			done = 0;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (!(account_is_operator_or_admin(conn_get_account(c), channel_get_name(conn_get_channel(c))))) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}
			dst_c = account_get_conn(acc);

			if (account_get_auth_voice(acc, channel) == 1)
			{
				if ((account_get_auth_admin(conn_get_account(c), channel) == 1) || (account_get_auth_admin(conn_get_account(c), NULL) == 1))
				{
					account_set_auth_voice(acc, channel, 0);
					msgtemp = localize(c, "{} has been removed from VOP list.", username);
					msgtemp2 = localize(c, "{} has removed you from VOP list of channel \"{}\"", conn_get_loggeduser(c), channel);
					changed = 1;
				}
				else
				{
					msgtemp = localize(c, "You must be at least Channel Admin to remove {} from the VOP list", username);
				}
				done = 1;
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			changed = 0;

			if ((dst_c) && channel_conn_has_tmpVOICE(conn_get_channel(c), dst_c) == 1)
			{
				conn_set_tmpVOICE_channel(dst_c, NULL);
				msgtemp = localize(c, "Voice has been taken from {} in this channel", username);
				msgtemp2 = localize(c, "{} has taken your Voice in channel \"{}\"", conn_get_loggeduser(c), channel);
				changed = 1;
				done = 1;
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);

			if (!done)
			{
				msgtemp = localize(c, "{} has no Voice in this channel, so it can't be taken away", username);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_op_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			int			OP_lvl;
			t_connection* dst_c;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			acc = conn_get_account(c);
			OP_lvl = 0;

			if (account_is_operator_or_admin(acc, channel))
				OP_lvl = 1;
			else if (channel_conn_is_tmpOP(conn_get_channel(c), c))
				OP_lvl = 2;

			if (OP_lvl == 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator or tempOP to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}

			dst_c = account_get_conn(acc);

			if (OP_lvl == 1) // user is full op so he may fully op others
			{
				if (account_get_auth_operator(acc, channel) == 1)
					msgtemp = localize(c, "{} is already a Channel Operator", username);
				else {
					account_set_auth_operator(acc, channel, 1);
					msgtemp = localize(c, "{} has been promoted to a Channel Operator", username);
					msgtemp2 = localize(c, "{} has promoted you to a Channel Operator in channel \"{}\"", conn_get_loggeduser(c), channel);
					changed = 1;
				}
			}
			else { // user is only tempOP so he may only tempOP others
				if ((!(dst_c)) || (conn_get_channel(c) != conn_get_channel(dst_c)))
					msgtemp = localize(c, "{} must be on the same channel to tempOP him", username);
				else
				{
					if (account_is_operator_or_admin(acc, channel))
						msgtemp = localize(c, "{} already is operator or admin, no need to tempOP him", username);
					else
					{
						conn_set_tmpOP_channel(dst_c, channel);
						msgtemp = localize(c, "{} has been promoted to a tempOP", username);
						msgtemp2 = localize(c, "{} has promoted you to a tempOP in this channel", conn_get_loggeduser(c));
						changed = 1;
					}
				}
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_tmpop_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			t_connection* dst_c;
			int			changed = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (!(account_is_operator_or_admin(conn_get_account(c), channel_get_name(conn_get_channel(c))) || channel_conn_is_tmpOP(conn_get_channel(c), c))) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator or tmpOP to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}

			dst_c = account_get_conn(acc);

			if (channel_conn_is_tmpOP(conn_get_channel(c), dst_c))
				msgtemp = localize(c, "{} has already tmpOP in this channel", username);
			else
			{
				if ((!(dst_c)) || (conn_get_channel(c) != conn_get_channel(dst_c)))
					msgtemp = localize(c, "{} must be on the same channel to tempOP him", username);
				else
				{
					if (account_is_operator_or_admin(acc, channel))
						msgtemp = localize(c, "{} already is operator or admin, no need to tempOP him", username);
					else
					{
						conn_set_tmpOP_channel(dst_c, channel);
						msgtemp = localize(c, "{} has been promoted to tmpOP in this channel", username);
						msgtemp2 = localize(c, "{} has promoted you to a tempOP in this channel", conn_get_loggeduser(c));
						changed = 1;
					}
				}
			}

			if (changed && dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
			message_send_text(c, message_type_info, c, msgtemp);
			command_set_flags(dst_c);
			return 0;
		}

		static int _handle_deop_command(t_connection* c, char const* text)
		{
			char const* username;
			char const* channel;
			t_account* acc;
			int			OP_lvl;
			t_connection* dst_c;
			int			done = 0;

			if (!(conn_get_channel(c)) || !(channel = channel_get_name(conn_get_channel(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			acc = conn_get_account(c);
			OP_lvl = 0;

			if (account_is_operator_or_admin(acc, channel))
				OP_lvl = 1;
			else if (channel_conn_is_tmpOP(conn_get_channel(c), account_get_conn(acc)))
				OP_lvl = 2;

			if (OP_lvl == 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator or tempOP to use this command."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username


			if (!(acc = accountlist_find_account(username))) {
				msgtemp = localize(c, "There's no account with username {}.", username);
				message_send_text(c, message_type_info, c, msgtemp);
				return -1;
			}

			dst_c = account_get_conn(acc);

			if (OP_lvl == 1) // user is real OP and allowed to deOP
			{
				if (account_get_auth_admin(acc, channel) == 1 || account_get_auth_operator(acc, channel) == 1) {
					if (account_get_auth_admin(acc, channel) == 1) {
						if (account_get_auth_admin(conn_get_account(c), channel) != 1 && account_get_auth_admin(conn_get_account(c), NULL) != 1)
							message_send_text(c, message_type_info, c, localize(c, "You must be at least a Channel Admin to demote another Channel Admin"));
						else {
							account_set_auth_admin(acc, channel, 0);
							msgtemp = localize(c, "{} has been demoted from a Channel Admin.", username);
							message_send_text(c, message_type_info, c, msgtemp);
							if (dst_c)
							{
								msgtemp2 = localize(c, "{} has demoted you from a Channel Admin of channel \"{}\"", conn_get_loggeduser(c), channel);
								message_send_text(dst_c, message_type_info, c, msgtemp2);
							}
						}
					}
					if (account_get_auth_operator(acc, channel) == 1) {
						account_set_auth_operator(acc, channel, 0);
						msgtemp = localize(c, "{} has been demoted from a Channel Operator", username);
						message_send_text(c, message_type_info, c, msgtemp);
						if (dst_c)
						{
							msgtemp2 = localize(c, "{} has demoted you from a Channel Operator of channel \"{}\"", conn_get_loggeduser(c), channel);
							message_send_text(dst_c, message_type_info, c, msgtemp2);
						}
					}
					done = 1;
				}
				if ((dst_c) && channel_conn_is_tmpOP(conn_get_channel(c), dst_c))
				{
					conn_set_tmpOP_channel(dst_c, NULL);
					msgtemp = localize(c, "{} has been demoted from a tempOP of this channel", username);
					message_send_text(c, message_type_info, c, msgtemp);
					if (dst_c)
					{
						msgtemp2 = localize(c, "{} has demoted you from a tmpOP of channel \"{}\"", conn_get_loggeduser(c), channel);
						message_send_text(dst_c, message_type_info, c, msgtemp2);
					}
					done = 1;
				}
				if (!done) {
					msgtemp = localize(c, "{} is no Channel Admin or Channel Operator or tempOP, so you can't demote him.", username);
					message_send_text(c, message_type_info, c, msgtemp);
				}
			}
			else //user is just a tempOP and may only deOP other tempOPs
			{
				if (dst_c && channel_conn_is_tmpOP(conn_get_channel(c), dst_c))
				{
					conn_set_tmpOP_channel(account_get_conn(acc), NULL);
					msgtemp = localize(c, "{} has been demoted from a tempOP of this channel", username);
					message_send_text(c, message_type_info, c, msgtemp);
					msgtemp2 = localize(c, "{} has demoted you from a tempOP of channel \"{}\"", conn_get_loggeduser(c), channel);
					if (dst_c) message_send_text(dst_c, message_type_info, c, msgtemp2);
				}
				else
				{
					msgtemp = localize(c, "{} is no tempOP in this channel, so you can't demote him", username);
					message_send_text(c, message_type_info, c, msgtemp);
				}
			}

			command_set_flags(connlist_find_connection_by_accountname(username));
			return 0;
		}

		static int _handle_friends_command(t_connection* c, char const* text)
		{
			int i;
			t_account* my_acc = conn_get_account(c);

			std::vector<std::string> args = split_command(text, 2);

			if (args[1] == "add" || args[1] == "a") {
				t_packet* rpacket;
				t_connection* dest_c;
				t_account* friend_acc;
				t_server_friendslistreply_status status;
				t_game* game;
				t_channel* channel;
				char stat;
				t_list* flist;
				t_friend* fr;

				if (args[2].empty()) {
					describe_command(c, args[0].c_str());
					return -1;
				}
				text = args[2].c_str(); // username

				if (!(friend_acc = accountlist_find_account(text))) {
					message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
					return -1;
				}

				switch (account_add_friend(my_acc, friend_acc)) {
				case -1:
					message_send_text(c, message_type_error, c, localize(c, "Server error."));
					return -1;
				case -2:
					message_send_text(c, message_type_info, c, localize(c, "You can't add yourself to your friends list."));
					return -1;
				case -3:
					msgtemp = localize(c, "You can only have a maximum of {} friends.", prefs_get_max_friends());
					message_send_text(c, message_type_info, c, msgtemp);
					return -1;
				case -4:
					msgtemp = localize(c, "{} is already on your friends list!", text);
					message_send_text(c, message_type_info, c, msgtemp);
					return -1;
				}

				msgtemp = localize(c, "Added {} to your friends list.", text);
				message_send_text(c, message_type_info, c, msgtemp);
				dest_c = connlist_find_connection_by_account(friend_acc);
				if (dest_c != NULL) {
					msgtemp = localize(c, "{} added you to his/her friends list.", conn_get_username(c));
					message_send_text(dest_c, message_type_info, dest_c, msgtemp);
				}

				if ((conn_get_class(c) != conn_class_bnet) || (!(rpacket = packet_create(packet_class_bnet))))
					return 0;

				packet_set_size(rpacket, sizeof(t_server_friendadd_ack));
				packet_set_type(rpacket, SERVER_FRIENDADD_ACK);

				packet_append_string(rpacket, account_get_name(friend_acc));

				game = NULL;
				channel = NULL;

				if (!(dest_c))
				{
					bn_byte_set(&status.location, FRIENDSTATUS_OFFLINE);
					bn_byte_set(&status.status, 0);
					bn_int_set(&status.clienttag, 0);
				}
				else
				{
					bn_int_set(&status.clienttag, conn_get_clienttag(dest_c));
					stat = 0;
					flist = account_get_friends(my_acc);
					fr = friendlist_find_account(flist, friend_acc);
					if ((friend_get_mutual(fr)))    stat |= FRIEND_TYPE_MUTUAL;
					if ((conn_get_dndstr(dest_c)))  stat |= FRIEND_TYPE_DND;
					if ((conn_get_awaystr(dest_c))) stat |= FRIEND_TYPE_AWAY;
					bn_byte_set(&status.status, stat);
					if ((game = conn_get_game(dest_c)))
					{
						if (game_get_flag(game) != game_flag_private)
							bn_byte_set(&status.location, FRIENDSTATUS_PUBLIC_GAME);
						else
							bn_byte_set(&status.location, FRIENDSTATUS_PRIVATE_GAME);
					}
					else if ((channel = conn_get_channel(dest_c)))
					{
						bn_byte_set(&status.location, FRIENDSTATUS_CHAT);
					}
					else
					{
						bn_byte_set(&status.location, FRIENDSTATUS_ONLINE);
					}
				}

				packet_append_data(rpacket, &status, sizeof(status));

				if (game) packet_append_string(rpacket, game_get_name(game));
				else if (channel) packet_append_string(rpacket, channel_get_name(channel));
				else packet_append_string(rpacket, "");

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			else if (args[1] == "msg" || args[1] == "w" || args[1] == "whisper" || args[1] == "m")
			{
				char const* msg;
				int cnt = 0;
				t_connection* dest_c;
				t_elem* curr;
				t_friend* fr;
				t_list* flist;

				if (args[2].empty()) {
					describe_command(c, args[0].c_str());
					return -1;
				}
				msg = args[2].c_str(); // message

				flist = account_get_friends(my_acc);
				if (flist == NULL)
					return -1;

				LIST_TRAVERSE(flist, curr)
				{
					if (!(fr = (t_friend*)elem_get_data(curr))) {
						eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
						continue;
					}
					if (friend_get_mutual(fr)) {
						dest_c = connlist_find_connection_by_account(friend_get_account(fr));
						if (!dest_c) continue;
						message_send_text(dest_c, message_type_whisper, c, msg);
						cnt++;
					}
				}
				if (cnt)
					message_send_text(c, message_type_friendwhisperack, c, msg);
				else
					message_send_text(c, message_type_info, c, localize(c, "All of your friends are offline."));
			}
			else if (args[1] == "r" || args[1] == "remove" || args[1] == "del" || args[1] == "delete")
			{
				int num;
				t_packet* rpacket;

				if (args[2].empty()) {
					describe_command(c, args[0].c_str());
					return -1;
				}
				text = args[2].c_str(); // username

				switch ((num = account_remove_friend2(my_acc, text))) {
				case -1: return -1;
				case -2:
					msgtemp = localize(c, "{} was not found on your friends list.", text);
					message_send_text(c, message_type_info, c, msgtemp);
					return -1;
				default:
					msgtemp = localize(c, "Removed {} from your friends list.", text);
					message_send_text(c, message_type_info, c, msgtemp);

					if ((conn_get_class(c) != conn_class_bnet) || (!(rpacket = packet_create(packet_class_bnet))))
						return 0;

					packet_set_size(rpacket, sizeof(t_server_frienddel_ack));
					packet_set_type(rpacket, SERVER_FRIENDDEL_ACK);

					bn_byte_set(&rpacket->u.server_frienddel_ack.friendnum, num);

					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);

					return 0;
				}
			}
			else if (args[1] == "p" || args[1] == "promote") {
				int num;
				int n;
				char const* dest_name;
				t_packet* rpacket;
				t_list* flist;
				t_friend* fr;
				t_account* dest_acc;
				unsigned int dest_uid;

				if (args[2].empty()) {
					describe_command(c, args[0].c_str());
					return -1;
				}
				text = args[2].c_str(); // username

				num = account_get_friendcount(my_acc);
				flist = account_get_friends(my_acc);
				for (n = 1; n < num; n++)
					if ((dest_uid = account_get_friend(my_acc, n)) &&
						(fr = friendlist_find_uid(flist, dest_uid)) &&
						(dest_acc = friend_get_account(fr)) &&
						(dest_name = account_get_name(dest_acc)) &&
						(strcasecmp(dest_name, text) == 0))
					{
						account_set_friend(my_acc, n, account_get_friend(my_acc, n - 1));
						account_set_friend(my_acc, n - 1, dest_uid);
						msgtemp = localize(c, "Promoted {} in your friends list.", dest_name);
						message_send_text(c, message_type_info, c, msgtemp);

						if ((conn_get_class(c) != conn_class_bnet) || (!(rpacket = packet_create(packet_class_bnet))))
							return 0;

						packet_set_size(rpacket, sizeof(t_server_friendmove_ack));
						packet_set_type(rpacket, SERVER_FRIENDMOVE_ACK);
						bn_byte_set(&rpacket->u.server_friendmove_ack.pos1, n - 1);
						bn_byte_set(&rpacket->u.server_friendmove_ack.pos2, n);

						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
						return 0;
					}
			}
			else if (args[1] == "d" || args[1] == "demote") {
				int num;
				int n;
				char const* dest_name;
				t_packet* rpacket;
				t_list* flist;
				t_friend* fr;
				t_account* dest_acc;
				unsigned int dest_uid;

				if (args[2].empty()) {
					describe_command(c, args[0].c_str());
					return -1;
				}
				text = args[2].c_str(); // username

				num = account_get_friendcount(my_acc);
				flist = account_get_friends(my_acc);
				for (n = 0; n < num - 1; n++)
					if ((dest_uid = account_get_friend(my_acc, n)) &&
						(fr = friendlist_find_uid(flist, dest_uid)) &&
						(dest_acc = friend_get_account(fr)) &&
						(dest_name = account_get_name(dest_acc)) &&
						(strcasecmp(dest_name, text) == 0))
					{
						account_set_friend(my_acc, n, account_get_friend(my_acc, n + 1));
						account_set_friend(my_acc, n + 1, dest_uid);
						msgtemp = localize(c, "Demoted {} in your friends list.", dest_name);
						message_send_text(c, message_type_info, c, msgtemp);

						if ((conn_get_class(c) != conn_class_bnet) || (!(rpacket = packet_create(packet_class_bnet))))
							return 0;

						packet_set_size(rpacket, sizeof(t_server_friendmove_ack));
						packet_set_type(rpacket, SERVER_FRIENDMOVE_ACK);
						bn_byte_set(&rpacket->u.server_friendmove_ack.pos1, n);
						bn_byte_set(&rpacket->u.server_friendmove_ack.pos2, n + 1);

						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
						return 0;
					}
			}
			else if (args[1] == "list" || args[1] == "l" || args[1] == "online" || args[1] == "o") {
				char const* frienduid;
				std::string status, software;
				t_connection* dest_c;
				t_account* friend_acc;
				t_game const* game;
				t_channel const* channel;
				t_friend* fr;
				t_list* flist;
				int num;
				unsigned int uid;
				bool online_only = false;

				if (args[1] == "online" || args[1] == "o")
				{
					online_only = true;
				}
				if (!online_only)
				{
					msgtemp = localize(c, "Your {} - Friends List", prefs_get_servername());
					message_send_text(c, message_type_info, c, msgtemp);
				}
				else
				{
					msgtemp = localize(c, "Your {} - Online Friends List", prefs_get_servername());
					message_send_text(c, message_type_info, c, msgtemp);
				}
				message_send_text(c, message_type_info, c, "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
				num = account_get_friendcount(my_acc);

				flist = account_get_friends(my_acc);
				if (flist != NULL) {
					for (i = 0; i < num; i++)
					{
						if ((!(uid = account_get_friend(my_acc, i))) || (!(fr = friendlist_find_uid(flist, uid))))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "friend uid in list");
							continue;
						}
						friend_acc = friend_get_account(fr);
						if (!(dest_c = connlist_find_connection_by_account(friend_acc))) {
							if (online_only) {
								continue;
							}
							status = localize(c, ", offline");
						}
						else {
							software = localize(c, " using {}", clienttag_get_title(conn_get_clienttag(dest_c)));

							if (friend_get_mutual(fr)) {
								if ((game = conn_get_game(dest_c)))
									status = localize(c, ", in game \"{}\"", game_get_name(game));
								else if ((channel = conn_get_channel(dest_c))) {
									if (strcasecmp(channel_get_name(channel), "Arranged Teams") == 0)
										status = localize(c, ", in game AT Preparation");
									else
										status = localize(c, ", in channel \"{}\",", channel_get_name(channel));
								}
								else
									status = localize(c, ", is in AT Preparation");
							}
							else {
								if ((game = conn_get_game(dest_c)))
									status = localize(c, ", is in a game");
								else if ((channel = conn_get_channel(dest_c)))
									status = localize(c, ", is in a chat channel");
								else
									status = localize(c, ", is in AT Preparation");
							}
						}

						frienduid = account_get_name(friend_acc);
						if (!software.empty()) std::snprintf(msgtemp0, sizeof(msgtemp0), "%d: %s%.16s%.128s, %.64s", i + 1, friend_get_mutual(fr) ? "*" : " ", frienduid, status.c_str(), software.c_str());
						else std::snprintf(msgtemp0, sizeof(msgtemp0), "%d: %.16s%.128s", i + 1, frienduid, status.c_str());
						message_send_text(c, message_type_info, c, msgtemp0);
					}
				}
				message_send_text(c, message_type_info, c, "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
				message_send_text(c, message_type_info, c, localize(c, "End of Friends List"));
			}
			else {
				describe_command(c, args[0].c_str());
				return -1;
			}

			return 0;
		}

		static int _handle_me_command(t_connection* c, char const* text)
		{
			t_channel const* channel;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			if (!conn_quota_exceeded(c, text))
				channel_message_send(channel, message_type_emote, c, text);
			return 0;
		}


		static int _handle_green_command(t_connection* c, char const* text)
		{
			t_channel const* channel;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			if (!conn_quota_exceeded(c, text))
				channel_message_send(channel, message_type_talk_green, c, text);
			return 0;
		}

		static int _handle_blue_command(t_connection* c, char const* text)
		{
			t_channel const* channel;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			if (!conn_quota_exceeded(c, text))
				channel_message_send(channel, message_type_talk_blue, c, text);
			return 0;
		}

		static int _handle_yellow_command(t_connection* c, char const* text)
		{
			t_channel const* channel;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			if (!conn_quota_exceeded(c, text))
				channel_message_send(channel, message_type_talk_yellow, c, text);
			return 0;
		}

		static int _handle_fmt_command(t_connection* c, char const* text)
		{
			t_channel const* channel;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty()) {
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			if (!conn_quota_exceeded(c, text))
				channel_message_send(channel, message_type_talk_custom, c, text);
			return 0;
		}

		static int _handle_whisper_command(t_connection* c, char const* text)
		{
			char const* username; /* both include NUL, so no need to add one for middle @ or * */

			std::vector<std::string> args = split_command(text, 2);

			if (args[2].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			text = args[2].c_str(); // message

			do_whisper(c, username, text);

			return 0;
		}

		static int _handle_status_command(t_connection* c, char const* text)
		{
			t_clienttag clienttag;

			// get clienttag
			std::vector<std::string> args = split_command(text, 1);

			if (!args[1].empty() && (clienttag = tag_validate_client(args[1].c_str())))
			{
				// clienttag status
				msgtemp = localize(c, "There are currently {} user(s) in {} games of {}",
					conn_get_user_count_by_clienttag(clienttag),
					game_get_count_by_clienttag(clienttag),
					clienttag_get_title(clienttag));
				message_send_text(c, message_type_info, c, msgtemp);
			}
			else
			{
				// overall status
				msgtemp = localize(c, "There are currently {} users online, in {} games, and in {} channels.",
					connlist_login_get_length(),
					gamelist_get_length(),
					channellist_get_length());
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_who_command(t_connection* c, char const* text)
		{
			t_connection const* conn;
			t_channel const* channel;
			unsigned int         i;
			char const* tname;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			char const* cname = args[1].c_str(); // channel name


			if (!(channel = channellist_find_channel_by_name(cname, conn_get_country(c), realm_get_name(conn_get_realm(c)))))
			{
				message_send_text(c, message_type_error, c, localize(c, "That channel does not exist."));
				message_send_text(c, message_type_error, c, localize(c, "(If you are trying to search for a user, use the /whois command.)"));
				return -1;
			}
			if (channel_check_banning(channel, c) == 1)
			{
				message_send_text(c, message_type_error, c, localize(c, "You are banned from that channel."));
				return -1;
			}

			std::snprintf(msgtemp0, sizeof msgtemp0, "%s", localize(c, "Users in channel {}:", cname).c_str());
			i = std::strlen(msgtemp0);
			for (conn = channel_get_first(channel); conn; conn = channel_get_next())
			{
				if (i + std::strlen((tname = conn_get_username(conn))) + 2 > sizeof(msgtemp0)) /* " ", name, '\0' */
				{
					message_send_text(c, message_type_info, c, msgtemp0);
					i = 0;
				}
				std::sprintf(&msgtemp0[i], " %s", tname);
				i += std::strlen(&msgtemp0[i]);
			}
			if (i > 0)
				message_send_text(c, message_type_info, c, msgtemp0);

			return 0;
		}

		static int _handle_whois_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			do_whois(c, text);

			return 0;
		}

		static int _handle_whoami_command(t_connection* c, char const* text)
		{
			char const* tname;

			if (!(tname = conn_get_username(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Unable to obtain your account name."));
				return -1;
			}

			do_whois(c, tname);

			return 0;
		}

		static int _handle_announce_command(t_connection* c, char const* text)
		{
			t_message* message;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			msgtemp = localize(c, "Announcement from {}: {}", conn_get_username(c), text);
			if (!(message = message_create(message_type_broadcast, c, msgtemp.c_str())))
				message_send_text(c, message_type_info, c, localize(c, "Could not broadcast message."));
			else
			{
				if (message_send_all(message) < 0)
					message_send_text(c, message_type_info, c, localize(c, "Could not broadcast message."));
				message_destroy(message);
			}

			return 0;
		}

		static int _handle_beep_command(t_connection* c, char const* text)
		{
			message_send_text(c, message_type_info, c, localize(c, "Audible notification on.")); /* FIXME: actually do something */
			return 0; /* FIXME: these only affect CHAT clients... I think they prevent ^G from being sent */
		}

		static int _handle_nobeep_command(t_connection* c, char const* text)
		{
			message_send_text(c, message_type_info, c, localize(c, "Audible notification off.")); /* FIXME: actually do something */
			return 0;
		}

		static int _handle_version_command(t_connection* c, char const* text)
		{
			message_send_text(c, message_type_info, c, PVPGN_SOFTWARE " " PVPGN_VERSION);
			return 0;
		}

		static int _handle_copyright_command(t_connection* c, char const* text)
		{
			static char const* const info[] =
			{
				" Copyright (C) 2002 - 2014  See source for details",
				" ",
				" PvPGN is free software; you can redistribute it and/or",
				" modify it under the terms of the GNU General Public License",
				" as published by the Free Software Foundation; either version 2",
				" of the License, or (at your option) any later version.",
				" ",
				" This program is distributed in the hope that it will be useful,",
				" but WITHOUT ANY WARRANTY; without even the implied warranty of",
				" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
				" GNU General Public License for more details.",
				" ",
				" You should have received a copy of the GNU General Public License",
				" along with this program; if not, write to the Free Software",
				" Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.",
				NULL
			};
			unsigned int i;

			for (i = 0; info[i]; i++)
				message_send_text(c, message_type_info, c, info[i]);

			return 0;
		}

		static int _handle_uptime_command(t_connection* c, char const* text)
		{

			msgtemp = localize(c, "Uptime: {}", seconds_to_timestr(server_get_uptime()));
			message_send_text(c, message_type_info, c, msgtemp);

			return 0;
		}

		static int _handle_stats_command(t_connection* c, char const* text)
		{
			char const* username;
			t_account* account;
			char const* clienttag = NULL;
			t_clienttag  clienttag_uint;
			char         clienttag_str[5];

			std::vector<std::string> args = split_command(text, 2);

			// username
			username = args[1].c_str();
			if (args[1].empty()) {
				account = conn_get_account(c);
			}
			else if (!(account = accountlist_find_account(username))) {
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			// clienttag
			if (!args[2].empty() && args[2].length() == 4)
				clienttag = args[2].c_str();
			else if (!(clienttag = tag_uint_to_str(clienttag_str, conn_get_clienttag(c)))) {
				message_send_text(c, message_type_error, c, localize(c, "Unable to determine client game."));
				return -1;
			}

			clienttag_uint = tag_case_str_to_uint(clienttag);


			// custom stats
			if (prefs_get_custom_icons() == 1 && customicons_allowed_by_client(clienttag_uint))
			{
				const char* text;

				// if text is not empty
				if (text = customicons_get_stats_text(account, clienttag_uint))
				{
					// split by lines
					char* output_array = strtok((char*)text, "\n");
					while (output_array)
					{
						message_send_text(c, message_type_info, c, output_array);
						output_array = strtok(NULL, "\n");
					}
					xfree((char*)text);

					return 0;
				}
			}


			switch (clienttag_uint)
			{
			case CLIENTTAG_BNCHATBOT_UINT:
				message_send_text(c, message_type_error, c, localize(c, "This game does not support win/loss records."));
				message_send_text(c, message_type_error, c, localize(c, "You must supply a user name and a valid program ID."));
				message_send_text(c, message_type_error, c, localize(c, "Example: /stats joe STAR"));
				return -1;
			case CLIENTTAG_DIABLORTL_UINT:
			case CLIENTTAG_DIABLOSHR_UINT:
				msgtemp = localize(c, "{}'s record:", account_get_name(account));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "level: {}", account_get_normal_level(account, clienttag_uint));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "class: {}", bnclass_get_str(account_get_normal_class(account, clienttag_uint)));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "stats: {} str  {} mag  {} dex  {} vit  {} gld",
					account_get_normal_strength(account, clienttag_uint),
					account_get_normal_magic(account, clienttag_uint),
					account_get_normal_dexterity(account, clienttag_uint),
					account_get_normal_vitality(account, clienttag_uint),
					account_get_normal_gold(account, clienttag_uint));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Diablo kills: {}", account_get_normal_diablo_kills(account, clienttag_uint));
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			case CLIENTTAG_WARCIIBNE_UINT:
				msgtemp = localize(c, "{}'s record:", account_get_name(account));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Normal games: {}-{}-{}",
					account_get_normal_wins(account, clienttag_uint),
					account_get_normal_losses(account, clienttag_uint),
					account_get_normal_disconnects(account, clienttag_uint));
				message_send_text(c, message_type_info, c, msgtemp);
				if (account_get_ladder_rating(account, clienttag_uint, ladder_id_normal) > 0)
					msgtemp = localize(c, "Ladder games: {}-{}-{} (rating {})",
						account_get_ladder_wins(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_losses(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_disconnects(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_rating(account, clienttag_uint, ladder_id_normal));
				else
					msgtemp = localize(c, "Ladder games: 0-0-0");
				message_send_text(c, message_type_info, c, msgtemp);
				if (account_get_ladder_rating(account, clienttag_uint, ladder_id_ironman) > 0)
					msgtemp = localize(c, "IronMan games: {}-{}-{} (rating {})",
						account_get_ladder_wins(account, clienttag_uint, ladder_id_ironman),
						account_get_ladder_losses(account, clienttag_uint, ladder_id_ironman),
						account_get_ladder_disconnects(account, clienttag_uint, ladder_id_ironman),
						account_get_ladder_rating(account, clienttag_uint, ladder_id_ironman));
				else
					msgtemp = localize(c, "IronMan games: 0-0-0");
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			case CLIENTTAG_WARCRAFT3_UINT:
			case CLIENTTAG_WAR3XP_UINT:
				msgtemp = localize(c, "{}'s Ladder Record's:", account_get_name(account));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Users Solo Level: {}, Experience: {}",
					account_get_ladder_level(account, clienttag_uint, ladder_id_solo),
					account_get_ladder_xp(account, clienttag_uint, ladder_id_solo));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "SOLO Ladder Record: {}-{}-0",
					account_get_ladder_wins(account, clienttag_uint, ladder_id_solo),
					account_get_ladder_losses(account, clienttag_uint, ladder_id_solo));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "SOLO Rank: {}",
					account_get_ladder_rank(account, clienttag_uint, ladder_id_solo));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Users Team Level: {}, Experience: {}",
					account_get_ladder_level(account, clienttag_uint, ladder_id_team),
					account_get_ladder_xp(account, clienttag_uint, ladder_id_team));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "TEAM Ladder Record: {}-{}-0",
					account_get_ladder_wins(account, clienttag_uint, ladder_id_team),
					account_get_ladder_losses(account, clienttag_uint, ladder_id_team));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "TEAM Rank: {}",
					account_get_ladder_rank(account, clienttag_uint, ladder_id_team));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Users FFA Level: {}, Experience: {}",
					account_get_ladder_level(account, clienttag_uint, ladder_id_ffa),
					account_get_ladder_xp(account, clienttag_uint, ladder_id_ffa));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "FFA Ladder Record: {}-{}-0",
					account_get_ladder_wins(account, clienttag_uint, ladder_id_ffa),
					account_get_ladder_losses(account, clienttag_uint, ladder_id_ffa));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "FFA Rank: {}",
					account_get_ladder_rank(account, clienttag_uint, ladder_id_ffa));
				message_send_text(c, message_type_info, c, msgtemp);
				if (account_get_teams(account)) {
					t_elem* curr;
					t_list* list;
					t_team* team;
					int teamcount = 0;

					list = account_get_teams(account);

					LIST_TRAVERSE(list, curr)
					{
						if (!(team = (t_team*)elem_get_data(curr)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
							continue;
						}

						if (team_get_clienttag(team) != clienttag_uint)
							continue;

						teamcount++;
						msgtemp = localize(c, "Users AT Team No. {}", teamcount);
						message_send_text(c, message_type_info, c, msgtemp);
						msgtemp = localize(c, "Users AT TEAM Level: {}, Experience: {}",
							team_get_level(team), team_get_xp(team));
						message_send_text(c, message_type_info, c, msgtemp);
						msgtemp = localize(c, "AT TEAM Ladder Record: {}-{}-0",
							team_get_wins(team), team_get_losses(team));
						message_send_text(c, message_type_info, c, msgtemp);
						msgtemp = localize(c, "AT TEAM Rank: {}",
							team_get_rank(team));
						message_send_text(c, message_type_info, c, msgtemp);
					}
				}
				return 0;
			default:
				msgtemp = localize(c, "{}'s record:", account_get_name(account));
				message_send_text(c, message_type_info, c, msgtemp);
				msgtemp = localize(c, "Normal games: {}-{}-{}",
					account_get_normal_wins(account, clienttag_uint),
					account_get_normal_losses(account, clienttag_uint),
					account_get_normal_disconnects(account, clienttag_uint));
				message_send_text(c, message_type_info, c, msgtemp);
				if (account_get_ladder_rating(account, clienttag_uint, ladder_id_normal) > 0)
					msgtemp = localize(c, "Ladder games: {}-{}-{} (rating {})",
						account_get_ladder_wins(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_losses(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_disconnects(account, clienttag_uint, ladder_id_normal),
						account_get_ladder_rating(account, clienttag_uint, ladder_id_normal));
				else
					msgtemp = localize(c, "Ladder games: 0-0-0");
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			}
		}

		static int _handle_time_command(t_connection* c, char const* text)
		{
			t_bnettime  btsystem;
			t_bnettime  btlocal;

			btsystem = bnettime();

			/* Battle.net time: Wed Jun 23 15:15:29 */
			btlocal = bnettime_add_tzbias(btsystem, local_tzbias());
			std::time_t now = bnettime_to_time(btlocal);
			struct std::tm tmnow = {};
			if (pvpgn::gmtime_s(&now, &tmnow) == nullptr)
				std::strcpy(msgtemp0, "?");
			else
				std::strftime(msgtemp0, sizeof(msgtemp0), "%a %b %d %H:%M:%S", &tmnow);
			msgtemp = localize(c, "Server Time: {}", msgtemp0);
			message_send_text(c, message_type_info, c, msgtemp);
			if (conn_get_class(c) == conn_class_bnet)
			{
				btlocal = bnettime_add_tzbias(btsystem, conn_get_tzbias(c));
				now = bnettime_to_time(btlocal);
				if (pvpgn::gmtime_s(&now, &tmnow) == nullptr)
					std::strcpy(msgtemp0, "?");
				else
					std::strftime(msgtemp0, sizeof(msgtemp0), "%a %b %d %H:%M:%S", &tmnow);
				msgtemp = localize(c, "Your local time: {}", msgtemp0);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_channel_command(t_connection* c, char const* text)
		{
			t_channel* channel;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // channelname

			if (!conn_get_game(c)) {
				if (strcasecmp(text, "Arranged Teams") == 0)
				{
					message_send_text(c, message_type_error, c, msgtemp = localize(c, "Channel Arranged Teams is a RESTRICTED Channel!"));
					return -1;
				}

				if (!(std::strlen(text) < MAX_CHANNELNAME_LEN))
				{
					msgtemp = localize(c, "Max channel name length exceeded (max {} symbols)", MAX_CHANNELNAME_LEN - 1);
					message_send_text(c, message_type_error, c, msgtemp);
					return -1;
				}

				if ((channel = conn_get_channel(c)) && (strcasecmp(channel_get_name(channel), text) == 0))
					return -1; // we don't have to do anything, we are already in this channel

				if (conn_set_channel(c, text) < 0)
					conn_set_channel(c, CHANNEL_NAME_BANNED); /* should not fail */
				command_set_flags(c);
			}
			else
				message_send_text(c, message_type_error, c, localize(c, "Command disabled while inside a game."));

			return 0;
		}

		static int _handle_rejoin_command(t_connection* c, char const* text)
		{

			if (channel_rejoin(c) != 0)
				message_send_text(c, message_type_error, c, localize(c, "You are not in a channel."));
			command_set_flags(c);

			return 0;
		}

		static int _handle_away_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			text = args[1].c_str(); // message

			if (text[0] == '\0') /* toggle away mode */
			{
				if (!conn_get_awaystr(c))
				{
					message_send_text(c, message_type_info, c, localize(c, "You are now marked as being away."));
					conn_set_awaystr(c, "Currently not available");
				}
				else
				{
					message_send_text(c, message_type_info, c, localize(c, "You are no longer marked as away."));
					conn_set_awaystr(c, NULL);
				}
			}
			else
			{
				message_send_text(c, message_type_info, c, localize(c, "You are now marked as being away."));
				conn_set_awaystr(c, text);
			}

			return 0;
		}

		static int _handle_dnd_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			text = args[1].c_str(); // message

			if (text[0] == '\0') /* toggle dnd mode */
			{
				if (!conn_get_dndstr(c))
				{
					message_send_text(c, message_type_info, c, localize(c, "Do Not Disturb mode engaged."));
					conn_set_dndstr(c, localize(c, "Not available").c_str());
				}
				else
				{
					message_send_text(c, message_type_info, c, localize(c, "Do Not Disturb mode canceled."));
					conn_set_dndstr(c, NULL);
				}
			}
			else
			{
				message_send_text(c, message_type_info, c, localize(c, "Do Not Disturb mode engaged."));
				conn_set_dndstr(c, text);
			}

			return 0;
		}

		static int _handle_squelch_command(t_connection* c, char const* text)
		{
			t_account* account;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			/* D2 std::puts * before username */
			if (text[0] == '*')
				text++;

			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_error, c, localize(c, "No such user."));
				return -1;
			}

			if (conn_get_account(c) == account)
			{
				message_send_text(c, message_type_error, c, localize(c, "You can't squelch yourself."));
				return -1;
			}

			if (conn_add_ignore(c, account) < 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "Could not squelch user."));
				return -1;
			}
			else
			{
				msgtemp = localize(c, "{} has been squelched.", account_get_name(account));
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_unsquelch_command(t_connection* c, char const* text)
		{
			t_account* account;
			t_connection* dest_c;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			/* D2 std::puts * before username */
			if (text[0] == '*')
				text++;

			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_info, c, localize(c, "No such user."));
				return -1;
			}

			if (conn_del_ignore(c, account) < 0)
			{
				message_send_text(c, message_type_info, c, localize(c, "User was not being ignored."));
				return -1;
			}
			else
			{
				t_message* message;

				message_send_text(c, message_type_info, c, localize(c, "No longer ignoring."));

				if ((dest_c = account_get_conn(account)))
				{
					if (!(message = message_create(message_type_userflags, dest_c, NULL))) /* handles NULL text */
						return -1;
					message_send(message, c);
					message_destroy(message);
				}
			}

			return 0;
		}

		static int _handle_kick_command(t_connection* c, char const* text)
		{
			char const* username;
			t_channel const* channel;
			t_connection* kuc;
			t_account* acc;

			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			text = args[2].c_str(); // reason

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			acc = conn_get_account(c);
			if (account_get_auth_admin(acc, NULL) != 1 && /* default to false */
				account_get_auth_admin(acc, channel_get_name(channel)) != 1 && /* default to false */
				account_get_auth_operator(acc, NULL) != 1 && /* default to false */
				account_get_auth_operator(acc, channel_get_name(channel)) != 1 && /* default to false */
				!channel_conn_is_tmpOP(channel, account_get_conn(acc)))
			{
				message_send_text(c, message_type_error, c, localize(c, "You have to be at least a Channel Operator or tempOP to use this command."));
				return -1;
			}
			if (!(kuc = connlist_find_connection_by_accountname(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "That user is not logged in."));
				return -1;
			}
			if (conn_get_channel(kuc) != channel)
			{
				message_send_text(c, message_type_error, c, localize(c, "That user is not in this channel."));
				return -1;
			}
			if (account_get_auth_admin(conn_get_account(kuc), NULL) == 1 ||
				account_get_auth_admin(conn_get_account(kuc), channel_get_name(channel)) == 1)
			{
				message_send_text(c, message_type_error, c, localize(c, "You cannot kick administrators."));
				return -1;
			}
			else if (account_get_auth_operator(conn_get_account(kuc), NULL) == 1 ||
				account_get_auth_operator(conn_get_account(kuc), channel_get_name(channel)) == 1)
			{
				message_send_text(c, message_type_error, c, localize(c, "You cannot kick operators."));
				return -1;
			}

			{
				char const* tname1;
				char const* tname2;

				tname1 = conn_get_loggeduser(kuc);
				tname2 = conn_get_loggeduser(c);
				if (!tname1 || !tname2) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL username");
					return -1;
				}

				if (text[0] != '\0')
					msgtemp = localize(c, "{} has been kicked by {} ({}).", tname1, tname2, text);
				else
					msgtemp = localize(c, "{} has been kicked by {}.", tname1, tname2);
				channel_message_send(channel, message_type_info, c, msgtemp.c_str());
			}
			conn_kick_channel(kuc, "Bye");
			if (conn_get_class(kuc) == conn_class_bnet)
				conn_set_channel(kuc, CHANNEL_NAME_KICKED); /* should not fail */

			return 0;
		}

		static int _handle_ban_command(t_connection* c, char const* text)
		{
			char const* username;
			t_channel* channel;
			t_connection* buc;

			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			text = args[2].c_str(); // reason

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}
			if (account_get_auth_admin(conn_get_account(c), NULL) != 1 && /* default to false */
				account_get_auth_admin(conn_get_account(c), channel_get_name(channel)) != 1 && /* default to false */
				account_get_auth_operator(conn_get_account(c), NULL) != 1 && /* default to false */
				account_get_auth_operator(conn_get_account(c), channel_get_name(channel)) != 1) /* default to false */
			{
				message_send_text(c, message_type_error, c, localize(c, "You have to be at least a Channel Operator to use this command."));
				return -1;
			}
			{
				t_account* account;

				if (!(account = accountlist_find_account(username)))
				{
					message_send_text(c, message_type_info, c, localize(c, "That account doesn't currently exist."));
					return -1;
				}
				else if (account_get_auth_admin(account, NULL) == 1 || account_get_auth_admin(account, channel_get_name(channel)) == 1)
				{
					message_send_text(c, message_type_error, c, localize(c, "You cannot ban administrators."));
					return -1;
				}
				else if (account_get_auth_operator(account, NULL) == 1 ||
					account_get_auth_operator(account, channel_get_name(channel)) == 1)
				{
					message_send_text(c, message_type_error, c, localize(c, "You cannot ban operators."));
					return -1;
				}
			}

			if (channel_ban_user(channel, username) < 0)
			{
				msgtemp = localize(c, "Unable to ban {}.", username);
				message_send_text(c, message_type_error, c, msgtemp);
			}
			else
			{
				char const* tname;

				tname = conn_get_loggeduser(c);
				if (text[0] != '\0')
					msgtemp = localize(c, "{} has been banned by {} ({}).", username, tname ? tname : "unknown", text);
				else
					msgtemp = localize(c, "{} has been banned by {}.", username, tname ? tname : "unknown");
				channel_message_send(channel, message_type_info, c, msgtemp.c_str());
			}
			if ((buc = connlist_find_connection_by_accountname(username)) &&
				conn_get_channel(buc) == channel)
				conn_set_channel(buc, CHANNEL_NAME_BANNED);

			return 0;
		}

		static int _handle_unban_command(t_connection* c, char const* text)
		{
			t_channel* channel;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}
			if (account_get_auth_admin(conn_get_account(c), NULL) != 1 && /* default to false */
				account_get_auth_admin(conn_get_account(c), channel_get_name(channel)) != 1 && /* default to false */
				account_get_auth_operator(conn_get_account(c), NULL) != 1 && /* default to false */
				account_get_auth_operator(conn_get_account(c), channel_get_name(channel)) != 1) /* default to false */
			{
				message_send_text(c, message_type_error, c, localize(c, "You are not a channel operator."));
				return -1;
			}

			if (channel_unban_user(channel, text) < 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "That user is not banned."));
				return -1;
			}
			else
			{
				msgtemp = localize(c, "{} is no longer banned from this channel.", text);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_reply_command(t_connection* c, char const* text)
		{
			char const* dest;

			if (!(dest = conn_get_lastsender(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "No one messaged you, use /m instead"));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			do_whisper(c, dest, text);
			return 0;
		}

		static int _handle_realmann_command(t_connection* c, char const* text)
		{
			t_realm* realm;
			t_realm* trealm;
			t_connection* tc;
			t_elem const* curr;
			t_message* message;

			if (!(realm = conn_get_realm(c))) {
				message_send_text(c, message_type_info, c, localize(c, "You must join a realm first"));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // message

			msgtemp = localize(c, "Announcement from {}@{}: {}", conn_get_username(c), realm_get_name(realm), text);
			if (!(message = message_create(message_type_broadcast, c, msgtemp.c_str())))
			{
				message_send_text(c, message_type_info, c, "Could not broadcast message.");
				return -1;
			}
			else
			{
				LIST_TRAVERSE_CONST(connlist(), curr)
				{
					tc = (t_connection*)elem_get_data(curr);
					if (!tc)
						continue;
					if ((trealm = conn_get_realm(tc)) && (trealm == realm))
					{
						message_send(message, tc);
					}
				}
			}

			message_destroy(message);

			return 0;
		}

		static int _handle_watch_command(t_connection* c, char const* text)
		{
			t_account* account;
			unsigned int events = Watch::ET_login | Watch::ET_logout | Watch::ET_joingame | Watch::ET_leavegame;
			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				message_send_text(c, message_type_info, c, localize(c, "/watch username 0011 (login = off,logout = off,joingame = yes,leavegame = yes)"));
				return -1;
			}

			text = args[1].c_str(); // username

			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
				return -1;
			}
			//here
			if (!args[2].empty())
			{
				events = 0;

				if (args[2].length() > 3)
				{
					for (int i = 0; i < 4; i++)
					{
						if (args[2][i] == '1')
						{
							if (i == 0)
								events += Watch::ET_login;
							else if (i == 1)
								events += Watch::ET_logout;
							else if (i == 2)
								events += Watch::ET_joingame;
							else if (i == 3)
								events += Watch::ET_leavegame;
						}
					}

				}
			}

			if (conn_add_watch(c, account, 0, events) < 0) /* FIXME: adds all events for now */
			{
				message_send_text(c, message_type_error, c, localize(c, "Add to watch list failed."));
				return -1;
			}
			else
			{
				msgtemp = localize(c, "User {} added to your watch list.", text);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_unwatch_command(t_connection* c, char const* text)
		{
			t_account* account;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username
			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
				return -1;
			}

			if (conn_del_watch(c, account, 0) < 0) /* FIXME: deletes all events for now */
			{
				message_send_text(c, message_type_error, c, localize(c, "Removal from watch list failed."));
				return -1;
			}
			else
			{
				msgtemp = localize(c, "User {} removed from your watch list.", text);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_watchall_command(t_connection* c, char const* text)
		{
			t_clienttag clienttag = 0;
			char const* clienttag_str;

			std::vector<std::string> args = split_command(text, 1);
			clienttag_str = args[1].c_str(); // clienttag

			if (clienttag_str[0] != '\0')
			{
				if (!(clienttag = tag_validate_client(args[1].c_str())))
				{
					describe_command(c, args[0].c_str());
					return -1;
				}
			}

			if (conn_add_watch(c, NULL, clienttag, Watch::ET_login | Watch::ET_logout | Watch::ET_joingame | Watch::ET_leavegame) < 0) /* FIXME: adds all events for now */
				message_send_text(c, message_type_error, c, localize(c, "Add to watch list failed."));
			else
				if (clienttag) {
					msgtemp = localize(c, "All {} users added to your watch list.", tag_uint_to_str((char*)clienttag_str, clienttag));
					message_send_text(c, message_type_info, c, msgtemp);
				}
				else
					message_send_text(c, message_type_info, c, localize(c, "All users added to your watch list."));

			return 0;
		}

		static int _handle_unwatchall_command(t_connection* c, char const* text)
		{
			t_clienttag clienttag = 0;
			char const* clienttag_str;

			std::vector<std::string> args = split_command(text, 1);
			clienttag_str = args[1].c_str(); // clienttag

			if (clienttag_str[0] != '\0')
			{
				if (!(clienttag = tag_validate_client(args[1].c_str())))
				{
					describe_command(c, args[0].c_str());
					return -1;
				}
			}

			if (conn_del_watch(c, NULL, clienttag) < 0) /* FIXME: deletes all events for now */
				message_send_text(c, message_type_error, c, localize(c, "Removal from watch list failed."));
			else
				if (clienttag) {
					msgtemp = localize(c, "All {} users removed from your watch list.", tag_uint_to_str((char*)clienttag_str, clienttag));
					message_send_text(c, message_type_info, c, msgtemp);
				}
				else
					message_send_text(c, message_type_info, c, localize(c, "All users removed from your watch list."));

			return 0;
		}

		static int _handle_lusers_command(t_connection* c, char const* text)
		{
			t_channel* channel;
			t_elem const* curr;
			char const* banned;
			unsigned int   i;

			if (!(channel = conn_get_channel(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			std::snprintf(msgtemp0, sizeof msgtemp0, "%s", localize(c, "Banned users:").c_str());
			i = std::strlen(msgtemp0);
			LIST_TRAVERSE_CONST(channel_get_banlist(channel), curr)
			{
				banned = (char*)elem_get_data(curr);
				if (i + std::strlen(banned) + 2 > sizeof(msgtemp0)) /* " ", name, '\0' */
				{
					message_send_text(c, message_type_info, c, msgtemp0);
					i = 0;
				}
				std::sprintf(&msgtemp0[i], " %s", banned);
				i += std::strlen(&msgtemp0[i]);
			}
			if (i > 0)
				message_send_text(c, message_type_info, c, msgtemp0);

			return 0;
		}

		static int _news_cb(std::time_t date, t_lstr* lstr, void* data)
		{
			char	save, * p, * q;
			t_connection* c = (t_connection*)data;

			{
				char strdate[64] = {};
				struct std::tm tm = {};
				if (pvpgn::localtime_s(&date, &tm) != nullptr)
				{
					std::strftime(strdate, sizeof(strdate), "%B %d, %Y", &tm);
					message_send_text(c, message_type_info, c, strdate);
				}
			else
				{
					message_send_text(c, message_type_info, c, localize(c, "(invalid date)"));
				}
			}

			for (p = lstr_get_str(lstr); *p;) {
				for (q = p; *q && *q != '\r' && *q != '\n'; q++);
				save = *q;
				*q = '\0';
				message_send_text(c, message_type_info, c, p);
				*q = save;
				p = q;
				for (; *p == '\n' || *p == '\r'; p++);
			}

			return 0;
		}

		static int _handle_news_command(t_connection* c, char const* text)
		{
			news_traverse(_news_cb, c);
			return 0;
		}

		struct glist_cb_struct {
			t_game_difficulty diff;
			t_clienttag tag;
			t_connection* c;
			bool lobby;
		};

		static int _glist_cb(t_game* game, void* data)
		{
			auto cbdata = reinterpret_cast<struct glist_cb_struct*>(data);

			// when cbdata->tag != 0, user is requesting games of the same client tag
			// return early when the game's client tag does not match the user's client tag
			if (cbdata->tag != 0 && cbdata->tag != game_get_clienttag(game))
			{
				return 0;
			}

			// when cbdata->lobby == true, only display list of all games in the lobby
			// return early if game has started or is already finished
			if (cbdata->lobby == true && (game_get_status(game) == game_status_started || game_get_status(game) == game_status_done))
			{
				return 0;
			}

			// when cbdata->diff != game_difficulty_none, user is requesting a specific game difficulty
			// return early if user is requesting a specific game difficulty that does not match the game's difficulty
			if (cbdata->diff != game_difficulty_none && cbdata->diff != game_get_difficulty(game))
			{
				return 0;
			}

			unsigned int pref = prefs_get_hide_pass_games();
			if (pref && game_get_flag(game) == game_flag_private)
			{
				// return early if hide_pass_games is true and the game is private
				if (cbdata->tag != 0)
				{
					return 0;
				}
				// if user used /games all and is not an admin, return early
				else if (cbdata->tag == 0 && account_get_auth_admin(conn_get_account(cbdata->c), nullptr) != 1)
				{
					return 0;
				}
			}
			

				std::snprintf(msgtemp0, sizeof(msgtemp0), " %-16.16s %1.1s %-8.8s %-21.21s %5u ",
					game_get_name(game),
					game_get_flag(game) != game_flag_private ? "n" : "y",
					game_status_get_str(game_get_status(game)),
					game_type_get_str(game_get_type(game)),
					game_get_ref(game));

				if (!cbdata->tag)
				{

					std::strcat(msgtemp0, clienttag_uint_to_str(game_get_clienttag(game)));
					std::strcat(msgtemp0, " ");
				}

				if ((!prefs_get_hide_addr()) || (account_get_command_groups(conn_get_account(cbdata->c)) & command_get_group("/admin-addr"))) /* default to false */
					std::strcat(msgtemp0, addr_num_to_addr_str(game_get_addr(game), game_get_port(game)));

				message_send_text(cbdata->c, message_type_info, cbdata->c, msgtemp0);

			return 0;
		}

		static int _handle_games_command(t_connection* c, char const* text)
		{
			char           clienttag_str[5];
			char const* dest;
			char const* difficulty;
			struct glist_cb_struct cbdata;

			std::vector<std::string> args = split_command(text, 2);

			dest = args[1].c_str(); // clienttag
			difficulty = args[1].c_str(); // difficulty (only for diablo)

			cbdata.c = c;
			cbdata.lobby = false;

			if (strcasecmp(difficulty, "norm") == 0)
				cbdata.diff = game_difficulty_normal;
			else if (strcasecmp(difficulty, "night") == 0)
				cbdata.diff = game_difficulty_nightmare;
			else if (strcasecmp(difficulty, "hell") == 0)
				cbdata.diff = game_difficulty_hell;
			else
				cbdata.diff = game_difficulty_none;

			if (dest[0] == '\0')
			{
				cbdata.tag = conn_get_clienttag(c);
				message_send_text(c, message_type_info, c, localize(c, "Currently accessible games:"));
			}
			else if (strcasecmp(dest, "all") == 0)
			{
				cbdata.tag = 0;
				message_send_text(c, message_type_info, c, localize(c, "All current games:"));
			}
			else if (strcasecmp(dest, "lobby") == 0 || strcasecmp(dest, "l") == 0)
			{
				cbdata.tag = conn_get_clienttag(c);
				cbdata.lobby = true;
				message_send_text(c, message_type_info, c, localize(c, "Games in lobby:"));
			}
			else
			{
				if (!(cbdata.tag = tag_validate_client(dest)))
				{
					describe_command(c, args[0].c_str());
					return -1;
				}

				if (cbdata.diff == game_difficulty_none)
					msgtemp = localize(c, "Current games of type {}", tag_uint_to_str(clienttag_str, cbdata.tag));
				else
					msgtemp = localize(c, "Current games of type {} {}", tag_uint_to_str(clienttag_str, cbdata.tag), difficulty);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			msgtemp = localize(c, " ------name------ p -status- --------type--------- count ");
			if (!cbdata.tag)
				msgtemp += localize(c, "ctag ");
			if ((!prefs_get_hide_addr()) || (account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
				msgtemp += localize(c, "--------addr--------");
			message_send_text(c, message_type_info, c, msgtemp);
			gamelist_traverse(c, _glist_cb, &cbdata, gamelist_source_none);

			return 0;
		}

		static int _handle_channels_command(t_connection* c, char const* text)
		{
			t_elem const* curr;
			t_channel const* channel;
			t_clienttag       clienttag;
			t_connection const* conn;
			t_account* acc;
			char const* name;
			int first;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // clienttag

			if (text[0] == '\0')
			{
				clienttag = conn_get_clienttag(c);
				message_send_text(c, message_type_info, c, localize(c, "Currently accessible channels:"));
			}
			else if (strcasecmp(text, "all") == 0)
			{
				clienttag = 0;
				message_send_text(c, message_type_info, c, localize(c, "All current channels:"));
			}
			else
			{
				if (!(clienttag = tag_validate_client(text)))
				{
					describe_command(c, args[0].c_str());
					return -1;
				}
				msgtemp = localize(c, "Current channels of type {}", text);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			msgtemp = localize(c, " -----------name----------- users ----admin/operator----");
			message_send_text(c, message_type_info, c, msgtemp);
			LIST_TRAVERSE_CONST(channellist(), curr)
			{
				channel = (t_channel*)elem_get_data(curr);
				if ((!(channel_get_flags(channel) & channel_flags_clan)) && (!clienttag || !prefs_get_hide_temp_channels() || channel_get_permanent(channel)) &&
					(!clienttag || !channel_get_clienttag(channel) ||
						channel_get_clienttag(channel) == clienttag) &&
						((channel_get_max(channel) != 0) || //only show restricted channels to OPs and Admins
					((channel_get_max(channel) == 0 && account_is_operator_or_admin(conn_get_account(c), NULL)))) &&
							(!(channel_get_flags(channel) & channel_flags_thevoid)) // don't list TheVoid
					)
				{

					std::snprintf(msgtemp0, sizeof(msgtemp0), " %-26.26s %5d - ",
						channel_get_name(channel),
						channel_get_length(channel));

					first = 1;

					for (conn = channel_get_first(channel); conn; conn = channel_get_next())
					{
						acc = conn_get_account(conn);
						if (account_is_operator_or_admin(acc, channel_get_name(channel)) ||
							channel_conn_is_tmpOP(channel, account_get_conn(acc)))
						{
							name = conn_get_loggeduser(conn);
							if (std::strlen(msgtemp0) + std::strlen(name) + 6 >= MAX_MESSAGE_LEN) break;
							if (!first) std::strcat(msgtemp0, " ,");
							std::strcat(msgtemp0, name);
							if (account_get_auth_admin(acc, NULL) == 1) std::strcat(msgtemp0, "(A)");
							else if (account_get_auth_operator(acc, NULL) == 1) std::strcat(msgtemp0, "(O)");
							else if (account_get_auth_admin(acc, channel_get_name(channel)) == 1) std::strcat(msgtemp0, "(a)");
							else if (account_get_auth_operator(acc, channel_get_name(channel)) == 1) std::strcat(msgtemp0, "(o)");
							first = 0;
						}
					}

					message_send_text(c, message_type_info, c, msgtemp0);
				}
			}

			return 0;
		}

		static int _handle_rename_warcis_acc_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			char const* username, * username2;

			if (args[2].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			username = args[1].c_str(); // username
			username2 = args[2].c_str();

			t_account* src = NULL;

			if (src = accountlist_find_account(username))
			{
				if (account_check_name(username2) < 0) {
					message_send_text(c, message_type_error, c, "Invalid.");
					return -1;
				}
				if (accountlist_find_account(username2))
				{
					message_send_text(c, message_type_error, c, "AlreadyRegistered.");
					return -1;
				}
				account_set_name_real(src, username);
				message_send_text(c, message_type_error, c, "Success.");
			}
			message_send_text(c, message_type_error, c, "NoRegistered.");
			return 0;
		}

		static int _handle_check_warcis_acc_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			char const* username;

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			username = args[1].c_str();
			t_account* src = NULL;
			if (account_check_name(username) < 0) {
				message_send_text(c, message_type_error, c, "Invalid.");
				return -1;
			}

			if (src = accountlist_find_account(username))
			{
				message_send_text(c, message_type_error, c, "AlreadyRegistered.");
				return -1;
			}


			message_send_text(c, message_type_error, c, "Success.");
			return 0;
		}

		static int _handle_activate_warcis_acc_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			char const* username;

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			username = args[1].c_str();
			t_account* account = NULL;
			if (account_check_name(username) < 0) {
				message_send_text(c, message_type_error, c, "Invalid.");
				return -1;
			}

			if (!(account = accountlist_find_account(username)))
			{
				message_send_text(c, message_type_error, c, "Not found.");
				return -1;
			}

			account_set_auth_lock(account, 0);
			account_set_auth_locktime(account, 0);
			account_set_auth_lockreason(account, "");
			account_set_auth_lockby(account, "");


			message_send_text(c, message_type_error, c, "Success.");
			return 0;
		}



		static int _handle_set_warcis_acc_command(t_connection* c, char const* text)
		{
			unsigned int i;
			t_account* temp;

			char const* username, * hash, * forumusername;

			std::vector<std::string> args = split_command(text, 3);

			if (args[3].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			username = args[1].c_str(); // username
			hash = args[2].c_str();
			forumusername = args[3].c_str(); // username
			if (account_check_name(username) < 0) {
				message_send_text(c, message_type_error, c, "Invalid.");
				return -1;
			}


			msgtemp = localize(c, "Trying to add account \"{}\" with hash \"{}\"", username, hash);
			message_send_text(c, message_type_info, c, msgtemp);


			if (!(temp = accountlist_find_account(username)))
			{
				message_send_text(c, message_type_info, c, "No account found.");
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" not created (allready)", conn_get_socket(c), username);
				return -1;
			}


			account_set_name_forum(temp, forumusername);
			account_set_auth_lock(temp, 1);
			account_set_auth_locktime(temp, 0);
			account_set_auth_lockreason(temp, "Need confirm email!");
			account_set_auth_lockby(temp, "Venmade");
			account_set_strattr(temp, "BNET\\acct\\passhash1", hash);

			message_send_text(c, message_type_info, c, "Success.");
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" udpated", conn_get_socket(c), username);

			return 0;
		}
		static int _handle_addacct_command(t_connection* c, char const* text)
		{
			unsigned int i;
			t_account* temp;

			char const* username, * hash, * forumusername;

			std::vector<std::string> args = split_command(text, 4);

			if (args[4].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			username = args[1].c_str(); // username
			hash = args[2].c_str();
			forumusername = args[3].c_str(); // username
			if (account_check_name(username) < 0) {
				message_send_text(c, message_type_error, c, "Invalid.");
				return -1;
			}


			msgtemp = localize(c, "Trying to add account \"{}\" with hash \"{}\"", username, hash);
			message_send_text(c, message_type_info, c, msgtemp);


			if (accountlist_find_account(username))
			{
				message_send_text(c, message_type_info, c, "AlreadyRegistered.");
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" not created (allready)", conn_get_socket(c), username);
				return -1;
			}

			temp = accountlist_create_account(username, hash);
			if (!temp) {
				message_send_text(c, message_type_info, c, "Failed.");
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" not created (failed)", conn_get_socket(c), username);
				return -1;
			}

			account_set_name_forum(temp, forumusername);

			account_set_auth_lock(temp, 1);
			account_set_auth_locktime(temp, 0);
			account_set_auth_lockreason(temp, "Need confirm email!");
			account_set_auth_lockby(temp, "Venmade");
			account_set_email(temp, args[4].c_str());


			message_send_text(c, message_type_info, c, "Success.");
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" created", conn_get_socket(c), username);

			return 0;
		}

		static int _handle_chpass_command(t_connection* c, char const* text)
		{
			unsigned int i;
			t_account* account;
			t_account* temp;
			t_hash       passhash;
			char const* username;
			std::string       pass;

			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			if (args[2].empty())
			{
				username = conn_get_username(c);
				pass = args[1];
			}
			else
			{
				username = args[1].c_str();
				pass = args[2];
			}

			temp = accountlist_find_account(username);

			account = conn_get_account(c);

			if ((temp == account && account_get_auth_changepass(account) == 0) || /* default to true */
				(temp != account && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-chpass")))) /* default to false */
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (no change access)", conn_get_socket(c), username);
				message_send_text(c, message_type_error, c, localize(c, "Only admins may change passwords for other accounts."));
				return -1;
			}

			if (!temp)
			{
				message_send_text(c, message_type_error, c, localize(c, "Account does not exist."));
				return -1;
			}

			if (pass.length() > MAX_USERPASS_LEN)
			{
				msgtemp = localize(c, "Maximum password length allowed is {}", MAX_USERPASS_LEN);
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			for (i = 0; i < pass.length(); i++)
				pass[i] = safe_tolower(pass[i]);

			bnet_hash(&passhash, pass.length(), pass.c_str());

			msgtemp = localize(c, "Trying to change password for account \"{}\" to \"{}\"", username, pass.c_str());
			message_send_text(c, message_type_info, c, msgtemp);

			if (account_set_pass(temp, hash_get_str(passhash)) < 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "Unable to set password."));
				return -1;
			}

			if (account_get_auth_admin(account, NULL) == 1 ||
				account_get_auth_operator(account, NULL) == 1) {
				msgtemp = localize(c, "Password for account {} updated.", account_get_uid(temp));
				message_send_text(c, message_type_info, c, msgtemp);

				msgtemp = localize(c, "Hash is: {}", hash_get_str(passhash));
				message_send_text(c, message_type_info, c, msgtemp);
			}
			else {
				msgtemp = localize(c, "Password for account {} updated.", username);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_connections_command(t_connection* c, char const* text)
		{
			t_elem const* curr;
			t_connection* conn;
			char           name[19];
			char           clienttag_str[5];

			if (!prefs_get_enable_conn_all() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-con"))) /* default to false */
			{
				message_send_text(c, message_type_error, c, localize(c, "This command is only enabled for admins."));
				return -1;
			}

			message_send_text(c, message_type_info, c, localize(c, "Current connections:"));

			std::vector<std::string> args = split_command(text, 1);
			text = args[1].c_str();

			if (text[0] == '\0')
			{
				msgtemp = localize(c, " -class -tag -----name------ -lat(ms)- ----channel---- --game--");
				message_send_text(c, message_type_info, c, msgtemp);
			}
			else
				if (std::strcmp(text, "all") == 0) /* print extended info */
				{
					if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr")))
						msgtemp = localize(c, " -#- -class ----state--- -tag -----name------ -session-- -flag- -lat(ms)- ----channel---- --game--");
					else
						msgtemp = localize(c, " -#- -class ----state--- -tag -----name------ -session-- -flag- -lat(ms)- ----channel---- --game-- ---------addr--------");
					message_send_text(c, message_type_info, c, msgtemp);
				}
				else
				{
					message_send_text(c, message_type_error, c, localize(c, "Unknown option."));
					return -1;
				}

			LIST_TRAVERSE_CONST(connlist(), curr)
			{
				conn = (t_connection*)elem_get_data(curr);
				std::snprintf(name, sizeof name, "%s", conn_get_account(conn) ? conn_get_username(conn) : "(none)");

				std::string channel_name;
				if (conn_get_channel(conn) != NULL && channel_get_name(conn_get_channel(conn)) != nullptr)
				{
					channel_name = channel_get_name(conn_get_channel(conn));
				}
				else
				{
					channel_name = localize(c, "none");
				}

				std::string game_name;
				if (conn_get_game(conn) != NULL)
					game_name = game_get_name(conn_get_game(conn));
				else
					game_name = localize(c, "none");

				if (text[0] == '\0')
					std::snprintf(msgtemp0, sizeof(msgtemp0), " %-6.6s %4.4s %-15.15s %9u %-16.16s %-8.8s",
						conn_class_get_str(conn_get_class(conn)),
						tag_uint_to_str(clienttag_str, conn_get_fake_clienttag(conn)),
						name,
						conn_get_latency(conn),
					channel_name.c_str(),
						game_name.c_str());
				else
					if (prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
						std::snprintf(msgtemp0, sizeof(msgtemp0), " %3d %-6.6s %-12.12s %4.4s %-15.15s 0x%08x 0x%04x %9u %-16.16s %-8.8s",
							conn_get_socket(conn),
							conn_class_get_str(conn_get_class(conn)),
							conn_state_get_str(conn_get_state(conn)),
							tag_uint_to_str(clienttag_str, conn_get_fake_clienttag(conn)),
							name,
							conn_get_sessionkey(conn),
							conn_get_flags(conn),
							conn_get_latency(conn),
					channel_name.c_str(),
							game_name.c_str());
					else
						std::snprintf(msgtemp0, sizeof(msgtemp0), " %3d %-6.6s %-12.12s %4.4s %-15.15s 0x%08x 0x%04x %9u %-16.16s %-8.8s %.16s",
							conn_get_socket(conn),
							conn_class_get_str(conn_get_class(conn)),
							conn_state_get_str(conn_get_state(conn)),
							tag_uint_to_str(clienttag_str, conn_get_fake_clienttag(conn)),
							name,
							conn_get_sessionkey(conn),
							conn_get_flags(conn),
							conn_get_latency(conn),
					channel_name.c_str(),
							game_name.c_str(),
							addr_num_to_addr_str(conn_get_addr(conn), conn_get_port(conn)));

				message_send_text(c, message_type_info, c, msgtemp0);
			}

			return 0;
		}

		static int _handle_finger_command(t_connection* c, char const* text)
		{
			char const* dest;
			t_account* account;
			t_connection* conn;
			char* tok;
			t_clanmember* clanmemb;

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			dest = args[1].c_str(); // username;

			if (!(account = accountlist_find_account(dest)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			// do not display sex if empty
			std::string sex = account_get_sex(account);
			std::string pattern = "Login: {} {} Sex: {}";
			pattern = (sex.length() > 0)
				? pattern
				: pattern.substr(0, pattern.find("Sex: ", 0));

			msgtemp = localize(c, pattern.c_str(),
				account_get_name(account),
				account_get_uid(account),
				account_get_sex(account));
			message_send_text(c, message_type_info, c, msgtemp);

			{
				std::time_t then = account_get_ll_ctime(account);
				struct std::tm tmthen = {};
				if (pvpgn::localtime_s(&then, &tmthen) == nullptr)
				{
					msgtemp = localize(c, "Created: {}", "?");
				}
				else
				{
					std::strftime(msgtemp0, sizeof(msgtemp0), "%a %b %d %H:%M %Y", &tmthen);
			msgtemp = localize(c, "Created: {}", msgtemp0);
				}

			message_send_text(c, message_type_info, c, msgtemp);
			}

			if ((clanmemb = account_get_clanmember(account)))
			{
				t_clan* clan;
				char	 status;

				if ((clan = clanmember_get_clan(clanmemb)))
				{
					msgtemp = localize(c, "Clan: {}", clan_get_name(clan));
					if ((status = clanmember_get_status(clanmemb)))
					{
						switch (status)
						{
						case CLAN_CHIEFTAIN:
							msgtemp += localize(c, "  Rank: Chieftain");
							break;
						case CLAN_SHAMAN:
							msgtemp += localize(c, "  Rank: Shaman");
							break;
						case CLAN_GRUNT:
							msgtemp += localize(c, "  Rank: Grunt");
							break;
						case CLAN_PEON:
							msgtemp += localize(c, "  Rank: Peon");
							break;
						default:;
						}
					}
					message_send_text(c, message_type_info, c, msgtemp);

				}
			}

			// do not display age if empty
			std::string age = account_get_age(account);
			pattern = "Location: {} Age: {}";
			pattern = (age.length() > 0)
				? pattern
				: pattern.substr(0, pattern.find("Age: ", 0));
			std::string loc = account_get_loc(account);
			msgtemp = localize(c, pattern.c_str(),
				(!loc.empty()) ? loc : "unknown",
				account_get_age(account));
			message_send_text(c, message_type_info, c, msgtemp);

			if ((conn = connlist_find_connection_by_accountname(dest)))
			{
				msgtemp = localize(c, "Client: {}    Ver: {}   Country: {}",
					clienttag_get_title(conn_get_clienttag(conn)),
					conn_get_clientver(conn),
					conn_get_country(conn));
				message_send_text(c, message_type_info, c, msgtemp);
			}

			const char* const ip_tmp = account_get_ll_ip(account);
			std::string ip(ip_tmp ? ip_tmp : "");
			if (ip.empty() == true ||
				!(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
				ip = localize(c, "unknown");

			{
				std::time_t then = account_get_ll_time(account);
				struct std::tm tmthen = {};

				/* FIXME: determine user's timezone */
				if (pvpgn::localtime_s(&then, &tmthen) != nullptr)
					std::strftime(msgtemp0, sizeof(msgtemp0), "%a %b %d %H:%M %Y", &tmthen);
				else
					std::strcpy(msgtemp0, "?");
			}

				if (!(conn))
					msgtemp = localize(c, "Last login {} from ", msgtemp0);
				else
					msgtemp = localize(c, "On since {} from ", msgtemp0);

			msgtemp += ip;
			message_send_text(c, message_type_info, c, msgtemp);

			/* check /admin-addr for admin privileges */
			if ((account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr")))
			{
				std::string yes = localize(c, "Yes");
				std::string no = localize(c, "No");
				/* the player who requested /finger has admin privileges
				give him more info about the one he queries;
				is_admin, is_operator, is_locked, email */
				msgtemp = localize(c, "Operator: {}, Admin: {}, Locked: {}, Muted: {}",
					account_get_auth_operator(account, NULL) == 1 ? yes : no,
					account_get_auth_admin(account, NULL) == 1 ? yes : no,
					account_get_auth_lock(account) == 1 ? yes : no,
					account_get_auth_mute(account) == 1 ? yes : no);
				message_send_text(c, message_type_info, c, msgtemp);

				msgtemp = localize(c, "Email: {} {}",
					account_get_email(account),
					account_get_email_verified(account) == 1 ? localize(c, "(Verified)") : localize(c, "(Unverified)"));
				message_send_text(c, message_type_info, c, msgtemp);

				msgtemp = localize(c, "Last login Owner: {}", account_get_ll_owner(account));
				message_send_text(c, message_type_info, c, msgtemp);
			}


			if (conn)
			{
				msgtemp = localize(c, "Idle {}", seconds_to_timestr(conn_get_idletime(conn)));
				message_send_text(c, message_type_info, c, msgtemp);
			}

			std::strncpy(msgtemp0, account_get_desc(account).c_str(), sizeof(msgtemp0));
			msgtemp0[sizeof(msgtemp0) - 1] = '\0';
			for (tok = std::strtok(msgtemp0, "\r\n"); tok; tok = std::strtok(NULL, "\r\n"))
				message_send_text(c, message_type_info, c, tok);
			message_send_text(c, message_type_info, c, "");

			return 0;
		}


		/* FIXME: do we want to show just Server Admin or Channel Admin Also? [Omega] */
		static int _handle_admins_command(t_connection* c, char const* text)
		{
			unsigned int    i;
			t_elem const* curr;
			t_connection* tc;
			char const* nick;

			std::snprintf(msgtemp0, sizeof msgtemp0, "%s", localize(c, "Currently logged on Administrators:").c_str());
			i = std::strlen(msgtemp0);
			LIST_TRAVERSE_CONST(connlist(), curr)
			{
				tc = (t_connection*)elem_get_data(curr);
				if (!tc)
					continue;
				if (!conn_get_account(tc))
					continue;
				if (account_get_auth_admin(conn_get_account(tc), NULL) == 1)
				{
					if ((nick = conn_get_username(tc)))
					{
						if (i + std::strlen(nick) + 2 > sizeof(msgtemp0)) /* " ", name, '\0' */
						{
							message_send_text(c, message_type_info, c, msgtemp0);
							i = 0;
						}
						std::sprintf(&msgtemp0[i], " %s", nick);
						i += std::strlen(&msgtemp0[i]);
					}
				}
			}
			if (i > 0)
				message_send_text(c, message_type_info, c, msgtemp0);

			return 0;
		}

		static int _handle_quit_command(t_connection* c, char const* text)
		{
			if (conn_get_game(c))
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] user '{}' tried to disconnect while in game, cheat attempt ?", conn_get_socket(c), conn_get_loggeduser(c));
			else {
				message_send_text(c, message_type_info, c, localize(c, "Connection closed."));
				conn_set_state(c, conn_state_destroy);
			}

			return 0;
		}

		static int _handle_kill_command(t_connection* c, char const* text)
		{
			t_connection* user;
			char const* username, * min;

			std::vector<std::string> args = split_command(text, 2);
			username = args[1].c_str(); // username
			min = args[2].c_str(); // minutes of ban

			if (username[0] == '\0' || (username[0] == '#' && (username[1] < '0' || username[1] > '9')))
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			if (username[0] == '#') {
				if (!(user = connlist_find_connection_by_socket(std::atoi(username + 1)))) {
					message_send_text(c, message_type_error, c, localize(c, "That connection doesn't exist."));
					return -1;
				}
			}
			else {
				if (!(user = connlist_find_connection_by_accountname(username))) {
					message_send_text(c, message_type_error, c, localize(c, "That user is not logged in?"));
					return -1;
				}
			}

			if (min[0] != '\0' && ipbanlist_add(c, addr_num_to_ip_str(conn_get_addr(user)), ipbanlist_str_to_time_t(c, min)) == 0)
			{
				ipbanlist_save(prefs_get_ipbanfile());
				message_send_text(user, message_type_info, user, localize(c, "An admin has closed your connection and banned your IP address."));
			}
			else
				message_send_text(user, message_type_info, user, localize(c, "Connection closed by admin."));
			conn_set_state(user, conn_state_destroy);

			message_send_text(c, message_type_info, c, localize(c, "Operation successful."));

			return 0;
		}

		static int _handle_killsession_command(t_connection* c, char const* text)
		{
			t_connection* user;
			char const* session, * min;


			std::vector<std::string> args = split_command(text, 2);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			session = args[1].c_str(); // session id
			min = args[1].c_str(); // minutes of ban

			if (!std::isxdigit((int)session[0]))
			{
				message_send_text(c, message_type_error, c, localize(c, "That is not a valid session."));
				return -1;
			}
			if (!(user = connlist_find_connection_by_sessionkey((unsigned int)std::strtoul(session, NULL, 16))))
			{
				message_send_text(c, message_type_error, c, localize(c, "That session does not exist."));
				return -1;
			}
			if (min[0] != '\0' && ipbanlist_add(c, addr_num_to_ip_str(conn_get_addr(user)), ipbanlist_str_to_time_t(c, min)) == 0)
			{
				ipbanlist_save(prefs_get_ipbanfile());
				message_send_text(user, message_type_info, user, localize(c, "Connection closed by admin and banned your IP's."));
			}
			else
				message_send_text(user, message_type_info, user, localize(c, "Connection closed by admin."));
			conn_set_state(user, conn_state_destroy);
			return 0;
		}

		static int _handle_gameinfo_command(t_connection* c, char const* text)
		{
			t_game const* game;
			char clienttag_str[5];

			std::vector<std::string> args = split_command(text, 1);
			text = args[1].c_str();

			if (text[0] == '\0')
			{
				// current user game
				if (!(game = conn_get_game(c)))
				{
					message_send_text(c, message_type_error, c, localize(c, "You are not in a game."));
					return -1;
				}
			}
			else
				if (!(game = gamelist_find_game_available(text, conn_get_clienttag(c), game_type_all)))
				{
					message_send_text(c, message_type_error, c, localize(c, "That game does not exist."));
					return -1;
				}
			std::string pub = localize(c, "public");
			std::string prv = localize(c, "private");
			msgtemp = localize(c, "Name: {}    ID: {} ({})", game_get_name(game), game_get_id(game), game_get_flag(game) != game_flag_private ? pub : prv);
			message_send_text(c, message_type_info, c, msgtemp);

			{
				std::string namestr;

				t_account* owner = conn_get_account(game_get_owner(game));
				if (owner != nullptr)
				{
					char const* tname = conn_get_loggeduser(game_get_owner(game));
					if (tname != nullptr)
				{
						namestr = tname;
				}
				else
					{
						namestr = localize(c, "unknown");
					}
				}
					else
				{
					namestr = localize(c, "none");
				}

				msgtemp = localize(c, "Owner: {}", namestr);

			}
			message_send_text(c, message_type_info, c, msgtemp);

			if (!prefs_get_hide_addr() || (account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) /* default to false */
			{
				unsigned int   addr;
				unsigned short port;
				unsigned int   taddr;
				unsigned short tport;

				taddr = addr = game_get_addr(game);
				tport = port = game_get_port(game);
				trans_net(conn_get_addr(c), &taddr, &tport);

				if (taddr == addr && tport == port)
					msgtemp = localize(c, "Address: {}",
						addr_num_to_addr_str(addr, port));
				else
					msgtemp = localize(c, "Address: {} (trans {})",
						addr_num_to_addr_str(addr, port),
						addr_num_to_addr_str(taddr, tport));
				message_send_text(c, message_type_info, c, msgtemp);
			}

			msgtemp = localize(c, "Client: {} (version {}, startver {})", tag_uint_to_str(clienttag_str, game_get_clienttag(game)), vernum_to_verstr(game_get_version(game)), game_get_startver(game));
			message_send_text(c, message_type_info, c, msgtemp);

			{
				struct std::tm gmgametime = {};
				std::time_t gametime = game_get_create_time(game);
				if (pvpgn::localtime_s(&gametime, &gmgametime) == nullptr)
					std::strcpy(msgtemp0, "?");
				else
					std::strftime(msgtemp0, sizeof(msgtemp0), GAME_TIME_FORMAT, &gmgametime);
				msgtemp = localize(c, "Created: {}", msgtemp0);
				message_send_text(c, message_type_info, c, msgtemp);

				gametime = game_get_start_time(game);
				if (gametime != (std::time_t)0)
				{
					if (pvpgn::localtime_s(&gametime, &gmgametime) == nullptr)
						std::strcpy(msgtemp0, "?");
					else
						std::strftime(msgtemp0, sizeof(msgtemp0), GAME_TIME_FORMAT, &gmgametime);
				}
				else
					std::strcpy(msgtemp0, "");
				msgtemp = localize(c, "Started: {}", msgtemp0);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			msgtemp = localize(c, "Status: {}", game_status_get_str(game_get_status(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Type: {}", game_type_get_str(game_get_type(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Speed: {}", game_speed_get_str(game_get_speed(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Difficulty: {}", game_difficulty_get_str(game_get_difficulty(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Option: {}", game_option_get_str(game_get_option(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			{
				std::string mapname;

				if (game_get_mapname(game) != nullptr)
				{
					mapname = game_get_mapname(game);
					
				}
				else
				{
					mapname = localize(c, "unknown");
				}

				msgtemp = localize(c, "Map: {}", mapname);
				message_send_text(c, message_type_info, c, msgtemp);
			}

			msgtemp = localize(c, "Map Size: {}x{}", game_get_mapsize_x(game), game_get_mapsize_y(game));
			message_send_text(c, message_type_info, c, msgtemp);
			msgtemp = localize(c, "Map Tileset: {}", game_tileset_get_str(game_get_tileset(game)));
			message_send_text(c, message_type_info, c, msgtemp);
			msgtemp = localize(c, "Map Type: {}", game_maptype_get_str(game_get_maptype(game)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Players: {} current, {} total, {} max", game_get_ref(game), game_get_count(game), game_get_maxplayers(game));
			message_send_text(c, message_type_info, c, msgtemp);

			{
				char const* description;

				if (!(description = game_get_description(game)))
					description = "";
				msgtemp = localize(c, "Description: {}", description);
			}

			return 0;
		}

		static int _handle_ladderactivate_command(t_connection* c, char const* text)
		{
			ladders.activate();
			message_send_text(c, message_type_info, c, localize(c, "Copied current scores to active scores on all ladders."));
			return 0;
		}

		static int _handle_rehash_command(t_connection* c, char const* text)
		{
			int mode = restart_mode_all; // all by default

			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			std::string mode_str = args[1];

			if (mode_str == "all")
				mode = restart_mode_all;
			else if (mode_str == "i18n")
				mode = restart_mode_i18n;
			else if (mode_str == "channels")
				mode = restart_mode_channels;
			else if (mode_str == "realms")
				mode = restart_mode_realms;
			else if (mode_str == "autoupdate")
				mode = restart_mode_autoupdate;
			else if (mode_str == "news")
				mode = restart_mode_news;
			else if (mode_str == "versioncheck")
				mode = restart_mode_versioncheck;
			else if (mode_str == "ipbans")
				mode = restart_mode_ipbans;
			else if (mode_str == "helpfile")
				mode = restart_mode_helpfile;
			else if (mode_str == "banners")
				mode = restart_mode_banners;
			else if (mode_str == "tracker")
				mode = restart_mode_tracker;
			else if (mode_str == "commandgroups")
				mode = restart_mode_commandgroups;
			else if (mode_str == "aliasfile")
				mode = restart_mode_aliasfile;
			else if (mode_str == "transfile")
				mode = restart_mode_transfile;
			else if (mode_str == "tournament")
				mode = restart_mode_tournament;
			else if (mode_str == "icons")
				mode = restart_mode_icons;
			else if (mode_str == "anongame")
				mode = restart_mode_anongame;
			else if (mode_str == "lua")
				mode = restart_mode_lua;
			else if (mode_str == "smtp")
				mode = restart_mode_smtp;
			else if (mode_str == "accountemailverification")
				mode = restart_mode_accountemailverification;
			else
			{
				message_send_text(c, message_type_info, c, localize(c, "Invalid mode."));
				return -1;
			}

			server_restart_wraper(mode);
			msgtemp = localize(c, "Rehash of \"{}\" is complete!", mode_str.c_str());
			message_send_text(c, message_type_info, c, msgtemp);
			return 0;
		}

		/**
		* /find <substr to search for inside username>
		*/
		static int _handle_find_command(t_connection* c, char const* text)
		{
			unsigned int  i = 0;
			t_account* account;
			char const* tname;
			t_entry* curr;
			t_hashtable* accountlist_head = accountlist();

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str();

			msgtemp = localize(c, " -- name -- similar to {}", text);
			message_send_text(c, message_type_info, c, msgtemp);


			HASHTABLE_TRAVERSE(accountlist_head, curr)
			{
				if (!curr)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "found NULL account in list");
				}
				else
				{
					account = (t_account*)entry_get_data(curr);
					if ((tname = accountlist_find_vague_account(account, text)) != NULL) {
						message_send_text(c, message_type_info, c, tname);
						return 0;
					}
				}
			}
			return 0;
		}

		/**
		* Save changes of accounts and clans from the cache to a storage
		*/
		static int _handle_save_command(t_connection* c, char const* text)
		{
			clanlist_save();

			accountlist_save(FS_FORCE | FS_ALL);
			accountlist_flush(FS_FORCE | FS_ALL);

			message_send_text(c, message_type_info, c, localize(c, "Pending changes has been saved into the database."));
			return 0;
		}

		static int _handle_shutdown_command(t_connection* c, char const* text)
		{
			char const* dest;
			unsigned int delay;

			std::vector<std::string> args = split_command(text, 1);
			dest = args[1].c_str(); // delay

			if (dest[0] == '\0')
				delay = prefs_get_shutdown_delay();
			else
				if (clockstr_to_seconds(dest, &delay) < 0)
				{
					message_send_text(c, message_type_error, c, localize(c, "Invalid delay."));
					return -1;
				}

			server_quit_delay(delay);

			if (delay)
				message_send_text(c, message_type_info, c, localize(c, "You've initialized the shutdown sequence."));
			else
				message_send_text(c, message_type_info, c, localize(c, "You've canceled the shutdown sequence."));

			return 0;
		}

		static int _handle_ladderinfo_command(t_connection* c, char const* text)
		{
			char const* rank_s, * tag_s;
			unsigned int rank;
			t_account* account;
			t_team* team;
			t_clienttag clienttag;
			const LadderReferencedObject* referencedObject;
			LadderList* ladderList;

			std::vector<std::string> args = split_command(text, 2);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			rank_s = args[1].c_str(); // rank
			tag_s = args[2].c_str(); // clienttag

			if (str_to_uint(rank_s, &rank) < 0 || rank < 1)
			{
				message_send_text(c, message_type_error, c, "Invalid rank.");
				return -1;
			}

			if (!(clienttag = tag_validate_client(tag_s)))
			{
				if (!(clienttag = conn_get_clienttag(c)))
				{
					message_send_text(c, message_type_error, c, localize(c, "Unable to determine client game."));
					return -1;
				}
			}

			if (clienttag == CLIENTTAG_STARCRAFT_UINT)
			{
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_active));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "StarCraft active %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_active_wins(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_active_losses(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_active_disconnects(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_active_rating(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "StarCraft active %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_current));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "StarCraft current %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_losses(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_disconnects(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal),
						account_get_ladder_rating(account, CLIENTTAG_STARCRAFT_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "StarCraft current %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);
			}
			else if (clienttag == CLIENTTAG_BROODWARS_UINT)
			{
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_active));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "Brood War active %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_active_wins(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_active_losses(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_active_disconnects(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_active_rating(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "Brood War active %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_current));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "Brood War current %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_losses(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_disconnects(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal),
						account_get_ladder_rating(account, CLIENTTAG_BROODWARS_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "Brood War current %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);
			}
			else if (clienttag == CLIENTTAG_WARCIIBNE_UINT)
			{
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_active));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II standard active %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_active_wins(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_active_losses(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_active_disconnects(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_active_rating(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II standard active %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, clienttag, ladder_sort_highestrated, ladder_time_active));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II IronMan active %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_active_wins(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_active_losses(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_active_disconnects(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_active_rating(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II IronMan active %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, clienttag, ladder_sort_highestrated, ladder_time_current));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II standard current %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_losses(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_disconnects(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal),
						account_get_ladder_rating(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_normal));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II standard current %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, clienttag, ladder_sort_highestrated, ladder_time_current));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II IronMan current %5u: %-20.20s %u/%u/%u rating %u",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_losses(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_disconnects(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman),
						account_get_ladder_rating(account, CLIENTTAG_WARCIIBNE_UINT, ladder_id_ironman));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft II IronMan current %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);
			}
			// --> aaron
			else if (clienttag == CLIENTTAG_WARCRAFT3_UINT || clienttag == CLIENTTAG_WAR3XP_UINT)
			{
				ladderList = ladders.getLadderList(LadderKey(ladder_id_solo, clienttag, ladder_sort_default, ladder_time_default));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 Solo %5u: %-20.20s %u/%u/0",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, clienttag, ladder_id_solo),
						account_get_ladder_losses(account, clienttag, ladder_id_solo));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 Solo %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_team, clienttag, ladder_sort_default, ladder_time_default));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 Team %5u: %-20.20s %u/%u/0",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, clienttag, ladder_id_team),
						account_get_ladder_losses(account, clienttag, ladder_id_team));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 Team %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_ffa, clienttag, ladder_sort_default, ladder_time_default));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (account = referencedObject->getAccount()))
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 FFA %5u: %-20.20s %u/%u/0",
						rank,
						account_get_name(account),
						account_get_ladder_wins(account, clienttag, ladder_id_ffa),
						account_get_ladder_losses(account, clienttag, ladder_id_ffa));
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 FFA %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);

				ladderList = ladders.getLadderList(LadderKey(ladder_id_ateam, clienttag, ladder_sort_default, ladder_time_default));
				referencedObject = ladderList->getReferencedObject(rank);
				if ((referencedObject) && (team = referencedObject->getTeam()))
				{
					t_xstr* membernames = xstr_alloc();
					for (unsigned char i = 0; i < team_get_size(team); i++) {
						xstr_cat_str(membernames, account_get_name(team_get_member(team, i)));
						if ((i)) xstr_cat_char(membernames, ',');
					}
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 AT Team %5u: %-80.80s %d/%d/0",
						rank,
						xstr_get_str(membernames),
						team_get_wins(team),
						team_get_losses(team));
					xstr_free(membernames);
				}
				else
					std::snprintf(msgtemp0, sizeof(msgtemp0), "WarCraft3 AT Team %5u: <none>", rank);
				message_send_text(c, message_type_info, c, msgtemp0);
			}
			//<---
			else
			{
				message_send_text(c, message_type_error, c, localize(c, "This game does not support win/loss records."));
				message_send_text(c, message_type_error, c, localize(c, "You must supply a rank and a valid program ID."));
				message_send_text(c, message_type_error, c, localize(c, "Example: /ladderinfo 1 STAR"));
			}

			return 0;
		}

		static int _handle_timer_command(t_connection* c, char const* text)
		{
			unsigned int delta;
			t_timer_data data;

			char const* delta_s, * msgtext_s;
			std::vector<std::string> args = split_command(text, 2);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			delta_s = args[1].c_str(); // timer delta
			msgtext_s = args[2].c_str(); // message text display when timer elapsed

			if (clockstr_to_seconds(delta_s, &delta) < 0)
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid duration."));
				return -1;
			}

			if (msgtext_s[0] == '\0')
				data.p = xstrdup(localize(c, "Your timer has expired.").c_str());
			else
				data.p = xstrdup(msgtext_s);

			if (timerlist_add_timer(c, std::time(NULL) + (std::time_t)delta, user_timer_cb, data) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "Could not add timer");
				xfree(data.p);
				message_send_text(c, message_type_error, c, localize(c, "Could not set timer."));
			}
			else
			{
				msgtemp = localize(c, "Timer set for {} second(s)", seconds_to_timestr(delta));
				message_send_text(c, message_type_info, c, msgtemp);
			}

			return 0;
		}

		static int _handle_serverban_command(t_connection* c, char const* text)
		{
			char const* username;
			t_connection* dest_c;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username

			if (!(dest_c = connlist_find_connection_by_accountname(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "That user is not logged on."));
				return -1;
			}
			msgtemp = localize(c, "Banning {} who is using IP address {}", conn_get_username(dest_c), addr_num_to_ip_str(conn_get_game_addr(dest_c)));
			message_send_text(c, message_type_info, c, msgtemp);
			message_send_text(c, message_type_info, c, localize(c, "User's account is also LOCKED! Only an admin can unlock it!"));
			msgtemp = localize(c, "/ipban a {}", addr_num_to_ip_str(conn_get_game_addr(dest_c)));
			handle_ipban_command(c, msgtemp.c_str());
			account_set_auth_lock(conn_get_account(dest_c), 1);
			//now kill the connection
			msgtemp = localize(c, "You have been banned by Admin: {}", conn_get_username(c));
			message_send_text(dest_c, message_type_error, dest_c, msgtemp);
			message_send_text(dest_c, message_type_error, dest_c, localize(c, "Your account is also LOCKED! Only an admin can UNLOCK it!"));
			conn_set_state(dest_c, conn_state_destroy);
			return 0;
		}

		static int _handle_netinfo_command(t_connection* c, char const* text)
		{
			char const* username;
			t_connection* conn;
			t_game const* game;
			unsigned int   addr;
			unsigned short port;
			unsigned int   taddr;
			unsigned short tport;

			std::vector<std::string> args = split_command(text, 1);
			username = args[1].c_str(); // username

			if (username[0] == '\0')
				username = conn_get_username(c);

			if (!(conn = connlist_find_connection_by_accountname(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "That user is not logged on."));
				return -1;
			}

			if (conn_get_account(conn) != conn_get_account(c) &&
				prefs_get_hide_addr() && !(account_get_command_groups(conn_get_account(c)) & command_get_group("/admin-addr"))) // default to false
			{
				message_send_text(c, message_type_error, c, localize(c, "Address information for other users is only available to admins."));
				return -1;
			}

			msgtemp = localize(c, "Server TCP: {} (bind {})", addr_num_to_addr_str(conn_get_real_local_addr(conn), conn_get_real_local_port(conn)), addr_num_to_addr_str(conn_get_local_addr(conn), conn_get_local_port(conn)));
			message_send_text(c, message_type_info, c, msgtemp);

			msgtemp = localize(c, "Client TCP: {}", addr_num_to_addr_str(conn_get_addr(conn), conn_get_port(conn)));
			message_send_text(c, message_type_info, c, msgtemp);

			taddr = addr = conn_get_game_addr(conn);
			tport = port = conn_get_game_port(conn);
			trans_net(conn_get_addr(c), &taddr, &tport);

			if (taddr == addr && tport == port)
				msgtemp = localize(c, "Client UDP: {}",
					addr_num_to_addr_str(addr, port));
			else
				msgtemp = localize(c, "Client UDP: {} (trans {})",
					addr_num_to_addr_str(addr, port),
					addr_num_to_addr_str(taddr, tport));
			message_send_text(c, message_type_info, c, msgtemp);

			if ((game = conn_get_game(conn)))
			{
				taddr = addr = game_get_addr(game);
				tport = port = game_get_port(game);
				trans_net(conn_get_addr(c), &taddr, &tport);

				if (taddr == addr && tport == port)
					msgtemp = localize(c, "Game UDP: {}",
						addr_num_to_addr_str(addr, port));
				else
					msgtemp = localize(c, "Game UDP: {} (trans {})",
						addr_num_to_addr_str(addr, port),
						addr_num_to_addr_str(taddr, tport));
			}
			else
				msgtemp = localize(c, "Game UDP: none");
			message_send_text(c, message_type_info, c, msgtemp);

			return 0;
		}

		static int _handle_quota_command(t_connection* c, char const* text)
		{
			msgtemp = localize(c, "Your quota allows you to write {} line(s) per {} second(s).", prefs_get_quota_lines(), prefs_get_quota_time());
			message_send_text(c, message_type_info, c, msgtemp);
			msgtemp = localize(c, "Long lines will be wrapped every {} characters.", prefs_get_quota_wrapline());
			message_send_text(c, message_type_info, c, msgtemp);
			msgtemp = localize(c, "You are not allowed to send lines with more than {} characters.", prefs_get_quota_maxline());
			message_send_text(c, message_type_info, c, msgtemp);

			return 0;
		}

		static int _handle_lockacct_command(t_connection* c, char const* text)
		{
			t_connection* user;
			t_account* account;
			char const* username, * reason = "", * hours = "24"; // default time 24 hours
			unsigned int sectime;

			std::vector<std::string> args = split_command(text, 3);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			if (!args[2].empty())
				hours = args[2].c_str(); // hours
			if (!args[3].empty())
				reason = args[3].c_str(); // reason

			if (!(account = accountlist_find_account(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			account_set_auth_lock(account, 1);
			sectime = (atoi(hours) == 0) ? 0 : (atoi(hours) * 60 * 60) + now; // get unlock time in the future
			account_set_auth_locktime(account, sectime);
			account_set_auth_lockreason(account, reason);
			account_set_auth_lockby(account, conn_get_username(c));


			// send message to author
			msgtemp = localize(c, "Account {} is now locked", account_get_name(account));
			msgtemp += account_get_locktext(c, account, false);
			message_send_text(c, message_type_error, c, msgtemp);

			// send message to locked user
			if ((user = connlist_find_connection_by_accountname(username)))
			{
				msgtemp = localize(c, "Your account has just been locked");
				msgtemp += account_get_locktext(c, account, true);
				message_send_text(user, message_type_error, user, msgtemp);
			}

			return 0;
		}

		static int _handle_unlockacct_command(t_connection* c, char const* text)
		{
			t_connection* user;
			t_account* account;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			if ((user = connlist_find_connection_by_accountname(text)))
			{
				msgtemp = localize(c, "Your account has just been unlocked by {}", conn_get_username(c));
				message_send_text(user, message_type_info, user, msgtemp);
			}

			account_set_auth_lock(account, 0);
			message_send_text(c, message_type_error, c, localize(c, "That user's account is now unlocked."));
			return 0;
		}


		static int _handle_muteacct_command(t_connection* c, char const* text)
		{
			t_connection* user;
			t_account* account;
			char const* username, * reason = "", * hours = "1"; // default time 1 hour
			unsigned int sectime;

			std::vector<std::string> args = split_command(text, 3);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			if (!args[2].empty())
				hours = args[2].c_str(); // hours
			if (!args[3].empty())
				reason = args[3].c_str(); // reason

			if (!(account = accountlist_find_account(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			account_set_auth_mute(account, 1);
			// get unlock time in the future
			sectime = (atoi(hours) == 0) ? 0 : (atoi(hours) * 60 * 60) + now;
			account_set_auth_mutetime(account, sectime);
			account_set_auth_mutereason(account, reason);
			account_set_auth_muteby(account, conn_get_username(c));

			// send message to author
			msgtemp = localize(c, "Account {} is now muted", account_get_name(account));
			msgtemp += account_get_mutetext(c, account, false);
			message_send_text(c, message_type_error, c, msgtemp);

			// send message to muted user
			if ((user = connlist_find_connection_by_accountname(username)))
			{
				msgtemp = localize(c, "Your account has just been muted");
				msgtemp += account_get_mutetext(c, account, true);
				message_send_text(user, message_type_error, user, msgtemp);
			}

			return 0;
		}

		static int _handle_unmuteacct_command(t_connection* c, char const* text)
		{
			t_connection* user;
			t_account* account;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // username

			if (!(account = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			if ((user = connlist_find_connection_by_accountname(text)))
			{
				msgtemp = localize(c, "Your account has just been unmuted by {}", conn_get_username(c));
				message_send_text(user, message_type_info, user, msgtemp);
			}

			account_set_auth_mute(account, 0);
			message_send_text(c, message_type_error, c, localize(c, "That user's account is now unmuted."));
			return 0;
		}

		static int _handle_flag_command(t_connection* c, char const* text)
		{
			char const* flag_s;
			unsigned int newflag;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			flag_s = args[1].c_str(); // flag

			newflag = std::strtoul(flag_s, NULL, 0);
			conn_set_flags(c, newflag);

			std::snprintf(msgtemp0, sizeof(msgtemp0), "0x%08x.", newflag);

			msgtemp = localize(c, "Flags set to {}.", msgtemp0);
			message_send_text(c, message_type_info, c, msgtemp);
			return 0;
		}

		static int _handle_tag_command(t_connection* c, char const* text)
		{
			char const* tag_s;
			t_clienttag clienttag;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			tag_s = args[1].c_str(); // flag

			if (clienttag = tag_validate_client(tag_s))
			{
				unsigned int oldflags = conn_get_flags(c);
				conn_set_clienttag(c, clienttag);
				channel_rejoin(c);
				conn_set_flags(c, oldflags);
				channel_update_userflags(c);
				msgtemp = localize(c, "Client tag set to {}.", tag_s);
			}
			else
				msgtemp = localize(c, "Invalid clienttag {} specified", tag_s);
			message_send_text(c, message_type_info, c, msgtemp);
			return 0;
		}

		static int _handle_ipscan_command(t_connection* c, char const* text)
		{
			/*
			Description of _handle_ipscan_command
			---------------------------------------
			Finds all currently logged in users with the given ip address.
			*/

			t_account* account;
			t_connection* conn;

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			text = args[1].c_str(); // ip or username

			std::string ip;
			if (account = accountlist_find_account(text))
			{
				conn = account_get_conn(account);
				if (conn)
				{
					// conn_get_addr returns int, so there can never be a NULL string construct
					ip = addr_num_to_ip_str(conn_get_addr(conn));
				}
				else
				{
					message_send_text(c, message_type_info, c, localize(c, "Warning: That user is not online, using last known address."));
					ip = account_get_ll_ip(account);
					if (ip.empty())
					{
						message_send_text(c, message_type_error, c, localize(c, "Sorry, no IP address could be retrieved."));
						return 0;
					}
				}
			}
			else
			{
				ip = text;
			}

			message_send_text(c, message_type_info, c, localize(c, "Scanning online users for IP {}...", ip));

			t_elem const* curr;
			int count = 0;
			LIST_TRAVERSE_CONST(connlist(), curr) {
				conn = (t_connection*)elem_get_data(curr);
				if (!conn) {
					// got empty element
					continue;
				}

				if (ip.compare(addr_num_to_ip_str(conn_get_addr(conn))) == 0)
				{
					std::snprintf(msgtemp0, sizeof(msgtemp0), "   %s", conn_get_loggeduser(conn));
					message_send_text(c, message_type_info, c, msgtemp0);
					count++;
				}
			}

			if (count == 0) {
				message_send_text(c, message_type_error, c, localize(c, "There are no online users with that IP address"));
			}

			return 0;
		}

		static int _handle_set_command(t_connection* c, char const* text)
		{
			t_account* account;
			char const* username, * key, * value;

			std::vector<std::string> args = split_command(text, 3);
			if (args[2].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str(); // username
			key = args[2].c_str(); // key
			value = args[3].c_str(); // value


			// disallow get/set value for password hash and username (hash can be cracked easily, account name should be permanent)
			if (strcasecmp(key, "bnet\\acct\\passhash1") == 0 || strcasecmp(key, "bnet\\acct\\username") == 0 || strcasecmp(key, "bnet\\acct\\verifier") == 0 || strcasecmp(key, "bnet\\acct\\salt") == 0 || strcasecmp(key, "bnet\\username") == 0)
			{
				message_send_text(c, message_type_info, c, localize(c, "Access denied due to security reasons."));
				return -1;
			}

			if (!(account = accountlist_find_account(username)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			if (*value == '\0')
			{
				if (account_get_strattr(account, key))
				{
					msgtemp = localize(c, "Current value of {} is \"{}\"", key, account_get_strattr(account, key));
					message_send_text(c, message_type_error, c, msgtemp);
				}
				else
					message_send_text(c, message_type_error, c, localize(c, "Value currently not set"));
				return 0;
			}

			// unset value
			if (strcasecmp(value, "null") == 0)
				value = NULL;

			std::snprintf(msgtemp0, sizeof(msgtemp0), " \"%.64s\" (%.128s = \"%.128s\")", account_get_name(account), key, value);

			if (account_set_strattr(account, key, value) < 0)
			{
				msgtemp = localize(c, "Unable to set key for");
				msgtemp += msgtemp0;
				message_send_text(c, message_type_error, c, msgtemp);
			}
			else
			{
				msgtemp = localize(c, "Key set successfully for");
				msgtemp += msgtemp0;
				message_send_text(c, message_type_error, c, msgtemp);
				eventlog(eventlog_level_warn, __FUNCTION__, "Key set by \"{}\" for {}", account_get_name(conn_get_account(c)), msgtemp0);
			}
			return 0;
		}

		static int _handle_motd_command(t_connection* c, char const* text)
		{
			std::string filename = i18n_filename(prefs_get_motdfile(), conn_get_gamelang_localized(c));

			std::FILE* fp = std::fopen(filename.c_str(), "r");
			if (fp)
			{
				message_send_file(c, fp);
				if (std::fclose(fp) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not close motd file \"{}\" after reading (std::fopen: {})", filename, std::strerror(errno));
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not open motd file \"{}\" for reading (std::fopen: {})", filename, std::strerror(errno));
				message_send_text(c, message_type_error, c, localize(c, "Unable to open motd."));
			}

			return 0;
		}

		static int _handle_tos_command(t_connection* c, char const* text)
		{
			/* handle /tos - shows terms of service by user request -raistlinthewiz */

			std::string filename = i18n_filename(prefs_get_tosfile(), conn_get_gamelang_localized(c));
			/* FIXME: if user enters relative path to tos file in config,
			   above routine will fail */
			std::FILE* fp = std::fopen(filename.c_str(), "r");
			if (fp)
			{

				char* buff;
				unsigned len;

				while ((buff = file_get_line(fp)))
				{

					if ((len = std::strlen(buff)) < MAX_MESSAGE_LEN)
					{
						i18n_convert(c, buff);
						message_send_text(c, message_type_info, c, buff);
					}
					else {
						/*  lines in TOS file can be > MAX_MESSAGE_LEN, so split them
						truncating is not an option for TOS -raistlinthewiz
						*/

						while (len > MAX_MESSAGE_LEN - 1)
						{
							std::strncpy(msgtemp0, buff, MAX_MESSAGE_LEN - 1);
							msgtemp0[MAX_MESSAGE_LEN - 1] = '\0';
							buff += MAX_MESSAGE_LEN - 1;
							len -= MAX_MESSAGE_LEN - 1;
							message_send_text(c, message_type_info, c, msgtemp0);
						}

						if (len > 0) /* does it exist a small last part ? */
						{
							i18n_convert(c, buff);
							message_send_text(c, message_type_info, c, buff);
						}

					}
				}


				if (std::fclose(fp) < 0)
					eventlog(eventlog_level_error, __FUNCTION__, "could not close tos file \"{}\" after reading (std::fopen: {})", filename, std::strerror(errno));
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not open tos file \"{}\" for reading (std::fopen: {})", filename, std::strerror(errno));
				message_send_text(c, message_type_error, c, localize(c, "Unable to send TOS (Terms of Service)."));
			}

			return 0;

		}


		static int _handle_ping_command(t_connection* c, char const* text)
		{
			unsigned int i;
			t_connection* user;
			t_game* game;

			std::vector<std::string> args = split_command(text, 1);
			text = args[1].c_str(); // username

			if (text[0] == '\0')
			{
				if ((game = conn_get_game(c)))
				{
					for (i = 0; i < game_get_count(game); i++)
					{
						if ((user = game_get_player_conn(game, i)))
						{
							msgtemp = localize(c, "{} latency: {}", conn_get_username(user), conn_get_latency(user));
							message_send_text(c, message_type_info, c, msgtemp);
						}
					}
					return 0;
				}
				msgtemp = localize(c, "Your latency {}", conn_get_latency(c));
			}
			else if ((user = connlist_find_connection_by_accountname(text)))
				msgtemp = localize(c, "{} latency {}", text, conn_get_latency(user));
			else
			{
				msgtemp = localize(c, "Invalid user.");
				return -1;
			}

			message_send_text(c, message_type_info, c, msgtemp);
			return 0;
		}

		static int _handle_commandgroups_command(t_connection* c, char const* text)
		{
			t_account* account;
			char const* command, * username;

			unsigned int usergroups;	// from user account
			unsigned int groups = 0;	// converted from arg3
			char	tempgroups[9];	// converted from usergroups

			std::vector<std::string> args = split_command(text, 3);
			// display help if [list] without [username], or not [list] without [groups]
			if (((args[1] == "list" || args[1] == "l") && args[2].empty())
				|| (!(args[1] == "list" || args[1] == "l") && args[3].empty()))
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			command = args[1].c_str(); // command
			username = args[2].c_str(); // username
			//args[3]; // groups


			if (!(account = accountlist_find_account(username))) {
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			usergroups = account_get_command_groups(account);

			if (!std::strcmp(command, "list") || !std::strcmp(command, "l")) {
				if (usergroups & 1) tempgroups[0] = '1'; else tempgroups[0] = ' ';
				if (usergroups & 2) tempgroups[1] = '2'; else tempgroups[1] = ' ';
				if (usergroups & 4) tempgroups[2] = '3'; else tempgroups[2] = ' ';
				if (usergroups & 8) tempgroups[3] = '4'; else tempgroups[3] = ' ';
				if (usergroups & 16) tempgroups[4] = '5'; else tempgroups[4] = ' ';
				if (usergroups & 32) tempgroups[5] = '6'; else tempgroups[5] = ' ';
				if (usergroups & 64) tempgroups[6] = '7'; else tempgroups[6] = ' ';
				if (usergroups & 128) tempgroups[7] = '8'; else tempgroups[7] = ' ';
				tempgroups[8] = '\0';
				msgtemp = localize(c, "{}'s command group(s): {}", username, tempgroups);
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			}

			// iterate chars in string
			for (std::string::iterator g = args[3].begin(); g != args[3].end(); ++g) {
				if (*g == '1') groups |= 1;
				else if (*g == '2') groups |= 2;
				else if (*g == '3') groups |= 4;
				else if (*g == '4') groups |= 8;
				else if (*g == '5') groups |= 16;
				else if (*g == '6') groups |= 32;
				else if (*g == '7') groups |= 64;
				else if (*g == '8') groups |= 128;
				else {
					msgtemp = localize(c, "Got bad group: {}", *g);
					message_send_text(c, message_type_info, c, msgtemp);
					return -1;
				}
			}

			if (!std::strcmp(command, "add") || !std::strcmp(command, "a")) {
				account_set_command_groups(account, usergroups | groups);
				msgtemp = localize(c, "Groups {} has been added to {}", args[3].c_str(), username);
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			}

			if (!std::strcmp(command, "del") || !std::strcmp(command, "d")) {
				account_set_command_groups(account, usergroups & (255 - groups));
				msgtemp = localize(c, "Groups {} has been deleted from {}", args[3].c_str(), username);
				message_send_text(c, message_type_info, c, msgtemp);
				return 0;
			}

			msgtemp = localize(c, "Got unknown command: {}", command);
			message_send_text(c, message_type_info, c, msgtemp);
			return -1;
		}

		static int _handle_topic_command(t_connection* c, char const* text)
		{
			t_channel* channel = conn_get_channel(c);
			if (channel == nullptr)
			{
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			const char* channel_name = channel_get_name(channel);
			if (channel_name == nullptr)
				{
				message_send_text(c, message_type_error, c, localize(c, "An error has occurred."));
					return -1;
				}

			if (std::strlen(text) > std::strlen("/topic "))
			{
				// set channel topic

				if (!(account_is_operator_or_admin(conn_get_account(c), channel_name)))
				{
					msgtemp = localize(c, "You must be at least a Channel Operator of {} to set the topic", channel_name);
					message_send_text(c, message_type_error, c, msgtemp);
					return -1;
				}

				const char* topic = text + std::strlen("/topic ");
				if ((std::strlen(topic) + 1) > MAX_TOPIC_LEN)
				{
					msgtemp = localize(c, "Max topic length exceeded (max {} symbols)", MAX_TOPIC_LEN);
					message_send_text(c, message_type_error, c, msgtemp);

					return -1;
			}

				channel_set_topic(channel, topic);
			}
			else
			{
				// get channel topic

				channel_display_topic(channel, c);
			}

			return 0;
		}

		static int _handle_moderate_command(t_connection* c, char const* text)
		{
			unsigned oldflags;
			t_channel* channel;

			if (!(channel = conn_get_channel(c))) {
				message_send_text(c, message_type_error, c, localize(c, "This command can only be used inside a channel."));
				return -1;
			}

			if (!(account_is_operator_or_admin(conn_get_account(c), channel_get_name(channel)))) {
				message_send_text(c, message_type_error, c, localize(c, "You must be at least a Channel Operator to use this command."));
				return -1;
			}

			oldflags = channel_get_flags(channel);

			if (channel_set_flags(channel, oldflags ^ channel_flags_moderated)) {
				eventlog(eventlog_level_error, __FUNCTION__, "could not set channel {} flags", channel_get_name(channel));
				message_send_text(c, message_type_error, c, localize(c, "Unable to change channel flags."));
				return -1;
			}
			else {
				if (oldflags & channel_flags_moderated)
					channel_message_send(channel, message_type_info, c, localize(c, "Channel is now unmoderated.").c_str());
				else
					channel_message_send(channel, message_type_info, c, localize(c, "Channel is now moderated.").c_str());
			}

			return 0;
		}

		static void _reset_d1_stats(t_account* account, t_clienttag ctag, t_connection* c)
		{
			account_set_normal_level(account, ctag, 0);
			account_set_normal_strength(account, ctag, 0),
				account_set_normal_magic(account, ctag, 0),
				account_set_normal_dexterity(account, ctag, 0),
				account_set_normal_vitality(account, ctag, 0),
				account_set_normal_gold(account, ctag, 0);

			msgtemp = localize(c, "Reset {}'s {} stats", account_get_name(account), clienttag_get_title(ctag));
			message_send_text(c, message_type_info, c, msgtemp);
		}

		static void _reset_scw2_stats(t_account* account, t_clienttag ctag, t_connection* c)
		{
			LadderList* ladderList;
			unsigned int uid = account_get_uid(account);

			account_set_normal_wins(account, ctag, 0);
			account_set_normal_losses(account, ctag, 0);
			account_set_normal_draws(account, ctag, 0);
			account_set_normal_disconnects(account, ctag, 0);

			// normal, current
			if (account_get_ladder_rating(account, ctag, ladder_id_normal) > 0) {
				account_set_ladder_wins(account, ctag, ladder_id_normal, 0);
				account_set_ladder_losses(account, ctag, ladder_id_normal, 0);
				account_set_ladder_draws(account, ctag, ladder_id_normal, 0);
				account_set_ladder_disconnects(account, ctag, ladder_id_normal, 0);
				account_set_ladder_rating(account, ctag, ladder_id_normal, 0);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_highestrated, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_mostwins, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_mostgames, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
			}

			// ironman, current
			if (account_get_ladder_rating(account, ctag, ladder_id_ironman) > 0) {
				account_set_ladder_wins(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_losses(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_draws(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_disconnects(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_rating(account, ctag, ladder_id_ironman, 0);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_highestrated, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_mostwins, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_mostgames, ladder_time_current));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
			}

			// normal, active
			if (account_get_ladder_active_rating(account, ctag, ladder_id_normal) > 0) {
				account_set_ladder_active_wins(account, ctag, ladder_id_normal, 0);
				account_set_ladder_active_losses(account, ctag, ladder_id_normal, 0);
				account_set_ladder_active_draws(account, ctag, ladder_id_normal, 0);
				account_set_ladder_active_disconnects(account, ctag, ladder_id_normal, 0);
				account_set_ladder_active_rating(account, ctag, ladder_id_normal, 0);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_highestrated, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_mostwins, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_normal, ctag, ladder_sort_mostgames, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
			}

			// ironman, active
			if (account_get_ladder_active_rating(account, ctag, ladder_id_ironman) > 0) {
				account_set_ladder_active_wins(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_active_losses(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_active_draws(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_active_disconnects(account, ctag, ladder_id_ironman, 0);
				account_set_ladder_active_rating(account, ctag, ladder_id_ironman, 0);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_highestrated, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_mostwins, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
				ladderList = ladders.getLadderList(LadderKey(ladder_id_ironman, ctag, ladder_sort_mostgames, ladder_time_active));
				if (ladderList != NULL)
					ladderList->delEntry(uid);
			}

			msgtemp = localize(c, "Reset {}'s {} stats", account_get_name(account), clienttag_get_title(ctag));
			message_send_text(c, message_type_info, c, msgtemp);
		}

		static void _reset_w3_stats(t_account* account, t_clienttag ctag, t_connection* c)
		{
			LadderList* ladderList;
			unsigned int uid = account_get_uid(account);

			account_set_ladder_level(account, ctag, ladder_id_solo, 0);
			account_set_ladder_xp(account, ctag, ladder_id_solo, 0);
			account_set_ladder_wins(account, ctag, ladder_id_solo, 0);
			account_set_ladder_losses(account, ctag, ladder_id_solo, 0);
			account_set_ladder_rank(account, ctag, ladder_id_solo, 0);
			ladderList = ladders.getLadderList(LadderKey(ladder_id_solo, ctag, ladder_sort_default, ladder_time_default));
			if (ladderList != NULL)
				ladderList->delEntry(uid);

			account_set_ladder_level(account, ctag, ladder_id_team, 0);
			account_set_ladder_xp(account, ctag, ladder_id_team, 0);
			account_set_ladder_wins(account, ctag, ladder_id_team, 0);
			account_set_ladder_losses(account, ctag, ladder_id_team, 0);
			account_set_ladder_rank(account, ctag, ladder_id_team, 0);
			ladderList = ladders.getLadderList(LadderKey(ladder_id_team, ctag, ladder_sort_default, ladder_time_default));
			if (ladderList != NULL)
				ladderList->delEntry(uid);

			account_set_ladder_level(account, ctag, ladder_id_ffa, 0);
			account_set_ladder_xp(account, ctag, ladder_id_ffa, 0);
			account_set_ladder_wins(account, ctag, ladder_id_ffa, 0);
			account_set_ladder_losses(account, ctag, ladder_id_ffa, 0);
			account_set_ladder_rank(account, ctag, ladder_id_ffa, 0);
			ladderList = ladders.getLadderList(LadderKey(ladder_id_ffa, ctag, ladder_sort_default, ladder_time_default));
			if (ladderList != NULL)
				ladderList->delEntry(uid);
			// this would now need a way to delete the team for all members now
			//account_set_atteamcount(account,ctag,0);

			msgtemp = localize(c, "Reset {}'s {} stats", account_get_name(account), clienttag_get_title(ctag));
			message_send_text(c, message_type_info, c, msgtemp);
		}

		static int _handle_clearstats_command(t_connection* c, char const* text)
		{
			char const* username, * tag;
			unsigned int all;
			t_account* account;
			t_clienttag  ctag = 0;

			std::vector<std::string> args = split_command(text, 2);
			if (args[2].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}
			username = args[1].c_str();
			tag = args[2].c_str(); // clienttag

			account = accountlist_find_account(username);
			if (!account) {
				message_send_text(c, message_type_error, c, localize(c, "Invalid user."));
				return -1;
			}

			if (strcasecmp(tag, "all") != 0)
			{
				ctag = tag_validate_client(tag);
				all = 0;
			}
			else all = 1;

			if (all || ctag == CLIENTTAG_DIABLORTL_UINT)
				_reset_d1_stats(account, CLIENTTAG_DIABLORTL_UINT, c);

			if (all || ctag == CLIENTTAG_DIABLOSHR_UINT)
				_reset_d1_stats(account, CLIENTTAG_DIABLOSHR_UINT, c);

			if (all || ctag == CLIENTTAG_WARCIIBNE_UINT)
				_reset_scw2_stats(account, CLIENTTAG_WARCIIBNE_UINT, c);

			if (all || ctag == CLIENTTAG_STARCRAFT_UINT)
				_reset_scw2_stats(account, CLIENTTAG_STARCRAFT_UINT, c);

			if (all || ctag == CLIENTTAG_BROODWARS_UINT)
				_reset_scw2_stats(account, CLIENTTAG_BROODWARS_UINT, c);

			if (all || ctag == CLIENTTAG_SHAREWARE_UINT)
				_reset_scw2_stats(account, CLIENTTAG_SHAREWARE_UINT, c);

			if (all || ctag == CLIENTTAG_WARCRAFT3_UINT)
				_reset_w3_stats(account, CLIENTTAG_WARCRAFT3_UINT, c);

			if (all || ctag == CLIENTTAG_WAR3XP_UINT)
				_reset_w3_stats(account, CLIENTTAG_WAR3XP_UINT, c);

			ladders.update();

			return 0;
		}

		/* Send message to all clients (similar to announce, but in messagebox) */
		static int _handle_alert_command(t_connection* c, char const* text)
		{
			t_clienttag  clienttag;
			t_clienttag  clienttag_dest;

			clienttag = conn_get_clienttag(c);
			// disallow clients that doesn't support SID_MESSAGEBOX
			if (clienttag != CLIENTTAG_STARCRAFT_UINT && clienttag != CLIENTTAG_BROODWARS_UINT && clienttag != CLIENTTAG_STARJAPAN_UINT && clienttag != CLIENTTAG_SHAREWARE_UINT &&
				clienttag != CLIENTTAG_DIABLORTL_UINT && clienttag != CLIENTTAG_DIABLOSHR_UINT &&
				clienttag != CLIENTTAG_WARCIIBNE_UINT && clienttag != CLIENTTAG_BNCHATBOT_UINT)
			{
				message_send_text(c, message_type_error, c, localize(c, "Your game client doesn't support MessageBox."));
				return -1;
			}

			std::vector<std::string> args = split_command(text, 1);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			// redundant line - it adds a caption to message box
			std::string goodtext = args[1] + localize(c, "\n\n***************************\nBy {}", conn_get_username(c));

			// caption
			msgtemp = localize(c, "Information from {}", prefs_get_servername());
			msgtemp = localize(c, " for {}", prefs_get_servername());

			t_connection* conn;
			t_elem const* curr;
			// send to online users
			LIST_TRAVERSE_CONST(connlist(), curr)
			{
				if (conn = (t_connection*)elem_get_data(curr))
				{
					clienttag_dest = conn_get_clienttag(conn);

					if (clienttag_dest != CLIENTTAG_STARCRAFT_UINT && clienttag_dest != CLIENTTAG_BROODWARS_UINT && clienttag_dest != CLIENTTAG_STARJAPAN_UINT && clienttag_dest != CLIENTTAG_SHAREWARE_UINT &&
						clienttag_dest != CLIENTTAG_DIABLORTL_UINT && clienttag_dest != CLIENTTAG_DIABLOSHR_UINT &&
						clienttag_dest != CLIENTTAG_WARCIIBNE_UINT && clienttag_dest != CLIENTTAG_BNCHATBOT_UINT) {
						continue;
					}
					messagebox_show(conn, goodtext.c_str(), msgtemp.c_str());
				}
			}

			return 0;
		}

		static int _handle_email_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			if (args[1].empty())
			{
				describe_command(c, args[0].c_str());
				return -1;
			}

			t_account* account = conn_get_account(c);

			if (args[1] == "get")
			{
				const char* email = account_get_email(account);
				if (email == nullptr)
				{
					message_send_text(c, message_type_error, c, localize(c, "You have not set an email address yet."));
					return -1;
				}

				message_send_text(c, message_type_info, c, localize(c, "Your email address is: {}", email));
			}
			else if (args[1] == "set")
			{
				if (args[2].empty())
				{
					describe_command(c, args[0].c_str());
					return -1;
				}

				// FIXME: check format of email address

				const char* current_email = account_get_email(account);
				if (current_email != nullptr && args[2].compare(current_email) == 0)
				{
					message_send_text(c, message_type_error, c, localize(c, "Your email address is already set to {}.", args[2]));
					return 0;
				}

				if ((args[2].length() + 1) > MAX_EMAIL_STR)
				{
					message_send_text(c, message_type_error, c, localize(c, "The email address is too long, please use another one.", args[2]));
					return -1;
				}

				int set_email_result = account_set_email(account, args[2]);
				if (set_email_result != 0)
				{
					message_send_text(c, message_type_error, c, localize(c, "An error has occurred."));
					return -1;
				}

				account_set_email_verified(account, false);
				
				message_send_text(c, message_type_info, c, localize(c, "Email address successfully set to {}.", args[2]));

				if (prefs_get_verify_account_email() == 1)
				{
					bool send_verification_code_successful = account_generate_email_verification_code(account);
					if (send_verification_code_successful)
					{
						message_send_text(c, message_type_info, c, localize(c, "An email has been sent, please check your inbox for the verification code."));
					}
					else
					{
						message_send_text(c, message_type_error, c, localize(c, "An error has occurred, could not send a verification email."));
					}
				}
			}
			else if (args[1] == "verify")
			{
				if (prefs_get_verify_account_email() == 0)
				{
					message_send_text(c, message_type_info, c, localize(c, "Email address verification is disabled."));
					return -1;
				}

				if (args[2].empty())
				{
					describe_command(c, args[0].c_str());
					return -1;
				}

				int is_verified = account_get_email_verified(account);
				if (is_verified == 1)
				{
					message_send_text(c, message_type_info, c, localize(c, "Your email address has already been verified."));
					return -1;
				}

				AccountVerifyEmailStatus verify_email_status = account_verify_email(account, args[2]);
				switch (verify_email_status)
				{
				case AccountVerifyEmailStatus::Success:
					message_send_text(c, message_type_info, c, localize(c, "Successfully verified email address."));
					break;
				case AccountVerifyEmailStatus::FailureCodeExpired:
					message_send_text(c, message_type_error, c, localize(c, "The code has already expired."));
					return -1;
				case AccountVerifyEmailStatus::FailureCodeIncorrect:
					message_send_text(c, message_type_error, c, localize(c, "The code is incorrect."));
					return -1;
				case AccountVerifyEmailStatus::FailureOther:
				default:
					message_send_text(c, message_type_error, c, localize(c, "An error has occurred."));
					return -1;
				}
			}
			else if (args[1] == "resendverification")
			{
				if (prefs_get_verify_account_email() == 0)
				{
					message_send_text(c, message_type_info, c, localize(c, "Email address verification is disabled."));
					return -1;
				}

				int is_verified = account_get_email_verified(account);
				if (is_verified == 1)
				{
					message_send_text(c, message_type_info, c, localize(c, "Your email address has already been verified."));
					return -1;
				}

				bool resend_verification_code_successful = account_generate_email_verification_code(account);
				if (resend_verification_code_successful)
				{
					message_send_text(c, message_type_info, c, localize(c, "Regenerated verification code. Check your email."));
				}
				else
				{
					message_send_text(c, message_type_error, c, localize(c, "An error has occurred."));
					return -1;
				}
				
			}
			else
			{
				describe_command(c, args[0].c_str());
				return -1;
			}


			return 0;
		}

		//here

		static int _handle_setahversion_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			const char* _newah_text = args[1].c_str();

			if (_newah_text[0] != '\0')
			{
				int newahversion = str_to_int(_newah_text);
				ANTIHACK_VERSION = newahversion;
				msgtemp = localize(c, "New AH Version: {}", ANTIHACK_VERSION);
				message_send_text(c, message_type_error, c, msgtemp);
			}
			else
			{
				msgtemp = localize(c, "Current AH Version: {}", ANTIHACK_VERSION);
				message_send_text(c, message_type_error, c, msgtemp);
			}
			return 0;
		}

		static int _handle_setlauncherversion_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			const char* _newlauncher_text = args[1].c_str();

			if (_newlauncher_text[0] != '\0')
			{
				int newlauncherversion = str_to_int(_newlauncher_text);
				LAUNCHER_VERSION = newlauncherversion;
				msgtemp = localize(c, "New LAUNCHER Version: {}", LAUNCHER_VERSION);
				message_send_text(c, message_type_error, c, msgtemp);
			}
			else
			{
				msgtemp = localize(c, "Current LAUNCHER Version: {}", LAUNCHER_VERSION);
				message_send_text(c, message_type_error, c, msgtemp);
			}
			return 0;
		}

		static int _handle_usercolor_command(t_connection* c, char const* text)
		{
			t_account* acc = conn_get_account(c);
			if (!c)
				return 0;
			std::vector<std::string> args = split_command(text, 1);
			unsigned int newcolor = atol(args[1].c_str());

			account_set_numattr(acc, "BNET\\acct\\channelcolor", newcolor);


			return 0;
		}

		static int _handle_updatewhitelistfile_command(t_connection* c, char const* text)
		{
			InitHostBotList();
			return 0;
		}

		static int _handle_updatecrcwhitelistfile_command(t_connection* c, char const* text)
		{
			InitCrcWhiteList();
			return 0;
		}

		static int _handle_updateblacklistfile_command(t_connection* c, char const* text)
		{
			InitBlackList();
			return 0;
		}

		static int _handle_setstats_command(t_connection* c, char const* text)
		{
			// setstats dota 
			std::vector<std::string> args = split_command(text, 30);
			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, "No username");
			}
			else if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, "No mapname");
			}
			else if (args[3].empty())
			{
				message_send_text(c, message_type_error, c, "No stats");
			}
			else
			{
				t_account* useracc;
				if (!(useracc = accountlist_find_account(args[1].c_str()))) {
					message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
					return -1;
				}

				std::string lmapname = ToLower(args[2]);
				bool FoundDota = false;
				if (lmapname.find("dota") == 0)
				{
					for (auto& mhost : MapHostStructList)
					{
						if (lmapname == mhost.MapCode)
						{
							FoundDota = true;
						}
					}
				}

				if (FoundDota && !args[12].empty())
				{
					account_map_set_stats(useracc, lmapname, "mmr", str_to_int(args[3]));
					account_map_set_stats(useracc, lmapname, "wins", str_to_int(args[4]));
					account_map_set_stats(useracc, lmapname, "loses", str_to_int(args[5]));
					account_map_set_stats(useracc, lmapname, "leaves", str_to_int(args[6]));
					account_map_set_stats(useracc, lmapname, "streak", str_to_int(args[7]));
					account_map_set_stats(useracc, lmapname, "minstreak", str_to_int(args[8]));
					account_map_set_stats(useracc, lmapname, "maxstreak", str_to_int(args[9]));
					account_map_set_stats(useracc, lmapname, "kills", str_to_int(args[10]));
					account_map_set_stats(useracc, lmapname, "deaths", str_to_int(args[11]));
					account_map_set_stats(useracc, lmapname, "assists", str_to_int(args[12]));

					account_print_map_stats(useracc, lmapname);
				}
				else if (lmapname == "default" && !args[4].empty())
				{
					account_map_set_stats(useracc, lmapname, "games", str_to_int(args[3]));
					account_map_set_stats(useracc, lmapname, "leaves", str_to_int(args[4]));
					account_print_map_stats(useracc, lmapname);
				}
				else
				{
					message_send_text(c, message_type_error, c, localize(c, "unknown mapname"));
				}
			}



			return 0;
		}


		static int _handle_addstats_command(t_connection* c, char const* text)
		{
			// setstats dota 
			std::vector<std::string> args = split_command(text, 30);
			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, "No username");
			}
			else if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, "No mapname");
			}
			else if (args[3].empty())
			{
				message_send_text(c, message_type_error, c, "No stats");
			}
			else
			{
				t_account* useracc;
				if (!(useracc = accountlist_find_account(args[1].c_str()))) {
					message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
					return -1;
				}


				std::string lmapname = ToLower(args[2]);
				bool FoundDota = false;
				if (lmapname.find("dota") == 0)
				{
					for (auto& mhost : MapHostStructList)
					{
						if (lmapname == mhost.MapCode)
						{
							FoundDota = true;
						}
					}
				}

				if (FoundDota && !args[12].empty())
				{
					account_map_set_stats(useracc, lmapname, "mmr", str_to_int(args[3])
						+ account_map_get_stats(useracc, lmapname, "mmr"));
					account_map_set_stats(useracc, lmapname, "wins", str_to_int(args[4])
						+ account_map_get_stats(useracc, lmapname, "wins"));
					account_map_set_stats(useracc, lmapname, "loses", str_to_int(args[5])
						+ account_map_get_stats(useracc, lmapname, "loses"));
					account_map_set_stats(useracc, lmapname, "leaves", str_to_int(args[6])
						+ account_map_get_stats(useracc, lmapname, "leaves"));
					account_map_set_stats(useracc, lmapname, "streak", str_to_int(args[7])
						+ account_map_get_stats(useracc, lmapname, "streak"));
					account_map_set_stats(useracc, lmapname, "minstreak", str_to_int(args[8])
						+ account_map_get_stats(useracc, lmapname, "minstreak"));
					account_map_set_stats(useracc, lmapname, "maxstreak", str_to_int(args[9])
						+ account_map_get_stats(useracc, lmapname, "maxstreak"));

					account_map_set_stats(useracc, lmapname, "kills", str_to_int(args[10])
						+ account_map_get_stats(useracc, lmapname, "kills"));
					account_map_set_stats(useracc, lmapname, "deaths", str_to_int(args[11])
						+ account_map_get_stats(useracc, lmapname, "deaths"));
					account_map_set_stats(useracc, lmapname, "assists", str_to_int(args[12])
						+ account_map_get_stats(useracc, lmapname, "assists"));

					account_print_map_stats(useracc, lmapname);
				}
				else if (lmapname == "default" && !args[4].empty())
				{
					account_map_set_stats(useracc, lmapname, "games", str_to_int(args[3])
						+ account_map_get_stats(useracc, lmapname, "games"));
					account_map_set_stats(useracc, lmapname, "leaves", str_to_int(args[4])
						+ account_map_get_stats(useracc, lmapname, "leaves"));
					account_print_map_stats(useracc, lmapname);
				}
				else
				{
					message_send_text(c, message_type_error, c, localize(c, "unknown statstype"));
				}

			}



			return 0;
		}

		static int _handle_cstats_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			t_account* useracc = conn_get_account(c);

			if (useracc && !args[1].empty())
			{
				if (IsCodeInMapList(args[1]))
					account_print_map_stats(useracc, args[1]);
				else
					message_send_text(c, message_type_error, c, "unknown map code");
			}
			else if (useracc && !args[2].empty())
			{
				useracc = accountlist_find_account(args[1].c_str());
				if (useracc)
				{
					if (IsCodeInMapList(args[1]))
						account_print_map_stats(useracc, args[1]);
					else
						message_send_text(c, message_type_error, c, "unknown map code");
				}
				else
					message_send_text(c, message_type_error, c, "unknown user");
			}
			else
			{
				message_send_text(c, message_type_error, c, "unknown map code");
			}
			return 0;
		}


		static int _handle_setstatstype_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			t_game* usergame = conn_get_game(c);
			if (!usergame)
			{
				message_send_text(c, message_type_error, c, "need enter game");
				return 0;
			}

			if (usergame && !args[1].empty())
			{
				//game_set_statstype( usergame, ToLower( args[ 1 ] ) );
				message_send_text(c, message_type_error, c, "command disabled in code!");
			}
			else
			{
				message_send_text(c, message_type_error, c, "need enter statstype");
			}
			return 0;
		}

		static int _handle_setgametop_command(t_connection* c, char const* text)
		{
			t_game* usergame = conn_get_game(c);
			std::vector<std::string> args = split_command(text, 1);
			if (usergame)
			{
				if (!args[1].empty())
					game_set_top(usergame, ToLower(args[1]) == "true" ? 2 : 0);
				else
					game_set_top(usergame, 2);
			}
			else
			{
				message_send_text(c, message_type_error, c, "not in game");
			}
			return 0;
		}

		static int _handle_setptslimit_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, "bad setptslimit command! need /setptslimit MIN MAX");
			}
			else
			{
				t_game* usergame = conn_get_game(c);
				if (usergame)
				{
					game_set_dotaptslimit(usergame, str_to_int(args[1]), str_to_int(args[2]));
				}
				else
				{
					message_send_text(c, message_type_error, c, "not in game");
				}

			}
			return 0;
		}

		static int _handle_hidegame_command(t_connection* c, char const* text)
		{
			t_game* usergame = conn_get_game(c);
			if (usergame)
			{
				game_set_top(usergame, -1);
			}
			return 0;
		}


		static int _handle_follow_command(t_connection* c, char const* text)
		{
			if (!c)
				return -1;


			t_account* targetacc;
			t_account* srcacc = conn_get_account(c);

			if (!srcacc)
				return -1;

			t_connection* connection;
			//t_elem  * curr;
			t_friend* fr;
			t_list* flist;
			std::vector<std::string> args = split_command(text, 1);

			conn_del_watch(c, NULL, 0);
			conn_set_follow_account(c, NULL);

			if (args[1].empty())
			{
				message_send_text(c, message_type_info, c, localize(c, "Follow end!"));
				return 0;
			}
			text = args[1].c_str(); // username

			if (!(targetacc = accountlist_find_account(text)))
			{
				message_send_text(c, message_type_info, c, localize(c, "That user does not exist."));
				return 0;
			}

			if (!(connection = account_get_conn(targetacc)))
			{
				message_send_text(c, message_type_info, c, localize(c, "User logged off."));
				return 0;
			}


			flist = account_get_friends(srcacc);
			if (flist == NULL)
			{
				message_send_text(c, message_type_info, c, localize(c, "You is alone... :("));
				return 0;
			}

			fr = friendlist_find_account(flist, targetacc);


			flist = account_get_friends(targetacc);
			if (flist == NULL)
				return 0;

			if (fr && friendlist_find_account(flist, srcacc))
			{
				if (conn_add_follow(c, targetacc, 0) < 0) /* FIXME: adds all events for now */
				{
					message_send_text(c, message_type_error, c, localize(c, "Add to follow failed."));
					return 0;
				}
				else
				{
					message_send_text(c, message_type_info, c, localize(c, "User {} added to follow.", text));
					conn_set_follow_account(c, targetacc);

					t_game* srcgame = conn_get_game(c);
					t_game* targetgame = conn_get_game(connection);

					if (srcgame != targetgame)
					{
						conn_send_ah_followstate(c, srcgame ? 2 : Watch::ET_joingame, targetgame ? game_get_name(targetgame) : "___");
					}
					else
					{
						t_channel* srcchannel = conn_get_channel(c);
						t_channel* targetchannel = conn_get_channel(connection);

						if (srcchannel != targetchannel && targetchannel)
						{
							if (!srcgame)
								conn_set_channel(c, channel_get_name(targetchannel));
						}

					}

				}
			}
			else
			{
				msgtemp = localize(c, "User {} is not your friend!", text);
				message_send_text(c, message_type_error, c, msgtemp);
			}


			return 0;
		}



		static int _handle_addcomment_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			t_account* acc;
			//arg 1 = username, arg 2 = comment
			if (args[1].empty())
			{
				msgtemp = localize(c, "Need enter username and comment!");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}
			else if (args[2].empty())
			{
				msgtemp = localize(c, "Need enter comment!");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}
			else if (args[2].length() < 5 || args[2].length() > 120)
			{
				msgtemp = localize(c, "Error. Limit for comment length: 5-100 chars!");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}


			if (!(acc = accountlist_find_account(args[1].c_str())))
			{
				msgtemp = localize(c, "User {} not found!", args[1].c_str());
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			account_add_comment(c, acc, args[1], args[2]);

			return 0;
		}


		static int _handle_delcomment_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			//arg 1 = username, arg 2 = commentid
			t_account* acc;



			if (args[1].empty())
			{
				msgtemp = localize(c, "Need enter username and comment id!");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}
			else if (args[2].empty())
			{
				msgtemp = localize(c, "Need enter comment id!(enter /showcomments for see id)");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			if (!(acc = accountlist_find_account(args[1].c_str())))
			{
				msgtemp = localize(c, "User {} not found!", args[1].c_str());
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			account_del_comment(c, acc, str_to_int(args[2]/*.c_str( )*/));

			return 0;
		}


		static int _handle_showcomment_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);
			std::vector<std::string> ListOfUsernames, ListOfComments;
			int commentscount = 0;
			//arg 1 = username
			t_account* acc;

			if (args[1].empty())
			{
				msgtemp = localize(c, "Need enter username!");
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			if (!(acc = accountlist_find_account(args[1].c_str())))
			{
				msgtemp = localize(c, "User {} not found!", args[1].c_str());
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			if (!(commentscount = account_get_comments(c, acc, ListOfUsernames, ListOfComments)))
			{
				msgtemp = localize(c, "No comments for user {}.", args[1].c_str());
				message_send_text(c, message_type_error, c, msgtemp);
				return -1;
			}

			msgtemp = localize(c, "List comments for user {}.", args[1].c_str());
			message_send_text(c, message_type_info, c, msgtemp);

			for (int i = 0; i < commentscount; i++)
			{
				msgtemp = localize(c, "#{}.{}:{}", i + 1, ListOfUsernames[i].c_str(), ListOfComments[i].c_str());
				message_send_text(c, message_type_info, c, msgtemp);
			}


			return 0;
		}

		static int _handle_host_command(t_connection* c, char const* text)
		{

			if (conn_get_game(c) && !conn_get_channel(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command not works in game!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);



			if (oldbotconnection && conn_get_hosted_game(c) && (
				game_get_status(conn_get_hosted_game(c)) == game_status_open
				||
				game_get_status(conn_get_hosted_game(c)) == game_status_full))
			{
				message_send_text(c, message_type_error, c, localize(c, "You have hosted game! Please use /unhost before start new!"));
				return 0;
			}

			if (conn_get_host_timeout(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "You can not create new game while wait own game!"));
				return 0;
			}


			// /host MAPCODE PLAYERS MAPMODE MAPNAME
			std::vector<std::string> args = split_command(text, 4);


			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}

			if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}



			MapHostStruct tmpHostStr;
			if (GetMapHostStructByCode(args[1], &tmpHostStr))
			{
				bool UsingStats = tmpHostStr.stats;
				message_send_text(c, message_type_error, c, localize(c, "Found map:{}", tmpHostStr.MapName));
				// PLAYESR args[2] - ��� ����, ���� ������ �� ������ ������� �� ���������
				t_connection* botconn = conn_search_availablebot(c);
				if (!botconn)
				{
					message_send_text(c, message_type_error, c, localize(c, "No availabled bot found!"));
					return 0;
				}

				std::string playercount = tmpHostStr.DefaultPlayers;

				int GameNameStartIndex = 4;
				int GameModeStartIndex = 3;
				int GamePlayersStartIndex = 2;
				int TeamBalanceType = 0;

				if (args[GameNameStartIndex].empty())
				{
					GameNameStartIndex--;
					GameModeStartIndex = 2;
					if (args[GameNameStartIndex].empty())
					{
						GameModeStartIndex = 0;
						GamePlayersStartIndex = 0;
						GameNameStartIndex--;
					}
				}

				if (GamePlayersStartIndex)
				{
					bool FoundPlayerCount = false;
					for (auto s : tmpHostStr.PlayersSettings)
					{
						if (ToLower(args[GamePlayersStartIndex]) == ToLower(s.Players))
						{
							FoundPlayerCount = true;
							playercount = s.Players;
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;

							break;
						}
					}

					if (!FoundPlayerCount)
					{
						GamePlayersStartIndex = 0;
						GameModeStartIndex = 2;
					}
					else if (GameModeStartIndex == 2)
					{
						GameModeStartIndex = 0;
					}
				}

				if (GameModeStartIndex > 1)
				{
					bool FoundMode = false;
					for (auto s : tmpHostStr.HostModes)
					{
						if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
						{
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;


							if (s.Players.size())
							{
								bool FoundInPlayerList = false;
								for (auto str : s.Players)
								{
									if (ToLower(str) == ToLower(playercount))
									{
										FoundInPlayerList = true;
									}
								}
								if (!FoundInPlayerList)
								{
									message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
									return 0;
								}
							}

							FoundMode = true;
							break;
						}
					}

					if (!FoundMode)
					{
						size_t modestrlen = args[GameModeStartIndex].length();
						if (modestrlen > 1)
						{
							size_t tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "tb");
							if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
							{
								TeamBalanceType = 1;
								if (modestrlen - 2 <= 0)
								{
									GameModeStartIndex = 0;
								}
								else
									args[GameModeStartIndex] = std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
							}
							else
							{
								tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "sp");
								if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
								{
									TeamBalanceType = 2;
									if (modestrlen - 2 <= 0)
									{
										GameModeStartIndex = 0;
									}
									else
										args[GameModeStartIndex] =
										std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
								}
							}
						}

					}

					if (GameModeStartIndex > 1 && !FoundMode)
					{

						for (auto s : tmpHostStr.HostModes)
						{
							if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
							{
								if (UsingStats && !s.ForStats)
									UsingStats = s.ForStats;


								if (s.Players.size())
								{
									bool FoundInPlayerList = false;
									for (auto str : s.Players)
									{
										if (ToLower(str) == ToLower(playercount))
										{
											FoundInPlayerList = true;
										}

									}
									if (!FoundInPlayerList)
									{
										message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
										return 0;
									}
								}

								FoundMode = true;
								break;
							}
						}

					}

					if (!FoundMode)
					{
						GameModeStartIndex = 0;
					}
				}




				if (GameModeStartIndex == 0 && GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 2;
				}
				else if (GameModeStartIndex == 0 || GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 3;
				}

				std::string gamename = "";
				for (int i = GameNameStartIndex; i < 5; i++)
					gamename += " " + args[i];
				if (gamename.length() > 24)
				{
					gamename = gamename.substr(0, 24);
				}
				/*std::string msgforplayer = "Map:" + tmpHostStr.MapName;
				msgforplayer += ". Players:";
				msgforplayer += GamePlayersStartIndex ? args[ GamePlayersStartIndex] : "default";
				msgforplayer += ". Mode:";
				msgforplayer += GameModeStartIndex ? args[ GameModeStartIndex ] : "default";
				msgforplayer += ". Game name:" + gamename;
				*/
				message_send_text(c, message_type_info, c,
					localize(c, "Map:{}. Players:{}. Mode:{}. Name:{}.", tmpHostStr.MapName
						, GamePlayersStartIndex ? args[GamePlayersStartIndex] : "default",
						GameModeStartIndex ? args[GameModeStartIndex] : "default", gamename));


				//std::string hostPlayers = GamePlayersStartIndex ? args[ GamePlayersStartIndex ] : tmpHostStr.DefaultPlayers;
				uint32_t hostPlayerCount = get_sum_from_string(playercount);

				if (hostPlayerCount < 2 && tmpHostStr.stats)
				{
					message_send_text(c, message_type_error, c, localize(c, "Bad player count for host!"));
					return 0;
				}

				conn_set_host_players(c, hostPlayerCount);

				if (!tmpHostStr.stats)
				{
					std::string LoadMapCmd = fmt::format("!map {}", tmpHostStr.MapLocalPath);
					message_send_text(botconn, message_type_whisper, botconn, LoadMapCmd);
				}

				std::string HostCmd = fmt::format("!host {} {} {} {} {} {} {}", tmpHostStr.stats ? "stats" : "custom",
					args[1],
					playercount.empty() ? "default" : playercount,
					TeamBalanceType ? std::to_string(TeamBalanceType) : "default",
					GameModeStartIndex ? args[GameModeStartIndex] : "default", account_get_name(conn_get_account(c)), gamename);

				message_send_text(c, message_type_info, c, HostCmd);
				message_send_text(botconn, message_type_whisper, botconn, HostCmd);


				// ���� ��� args ��������� ������ � 
				// args[4] ���
				// args[3] ���
				// args[2] ���������� �������
				// args[1] ��� �����  
				// �������� ���� � args[1] ����, �� ��� �������� ������� !load dota
				// 
				//------
				// ���� � args[3] �� ���, ������ ��� � args[2]
				// 
				// 
				//------
				// ���� args[4] ����, ������ ��� � args[3]
				//------
				// ���� � args[2] �� ���������� �������
				// �� � args[2] ���
				// ���� � args[2] �� ��� �� ������ ���, args[2] + " " + args[3]
				//------
				// ���� args[3] ����, ������ ��� � args[2]
				//------
				// 
				//------


				conn_set_botconnection(c, botconn);
				set_bot_availabled(botconn, false);
				conn_set_host_timeout(c, 15);
				conn_set_host_statstype(c, tmpHostStr.MapCode.c_str());
				if (tmpHostStr.stats && UsingStats)
				{
					conn_set_host_withstats(c, true);
				}
				else
				{
					message_send_text(c, message_type_error, c, localize(c, "Selected map hosted without stats!"));
					conn_set_host_withstats(c, false);
				}
			}
			else
			{
				message_send_text(c, message_type_error, c, localize(c, "Code {} not found in map list!", args[1]));
			}



			return 0;
		}



		static int _handle_phost_command(t_connection* c, char const* text)
		{
			if (conn_get_game(c) && !conn_get_channel(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command not works in game!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);



			if (oldbotconnection && conn_get_hosted_game(c) && (
				game_get_status(conn_get_hosted_game(c)) == game_status_open
				||
				game_get_status(conn_get_hosted_game(c)) == game_status_full))
			{
				message_send_text(c, message_type_error, c, localize(c, "You have hosted game! Please use /unhost before start new!"));
				return 0;
			}

			if (conn_get_host_timeout(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "You can not create new game while wait own game!"));
				return 0;
			}


			// /host MAPCODE PLAYERS MAPMODE MAPNAME
			std::vector<std::string> args = split_command(text, 4);


			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}

			if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}



			MapHostStruct tmpHostStr;
			if (GetMapHostStructByCode(args[1], &tmpHostStr))
			{
				bool UsingStats = tmpHostStr.stats;
				message_send_text(c, message_type_error, c, localize(c, "Found map:{}", tmpHostStr.MapName));
				// PLAYESR args[2] - ��� ����, ���� ������ �� ������ ������� �� ���������
				t_connection* botconn = conn_search_availablebot(c);
				if (!botconn)
				{
					message_send_text(c, message_type_error, c, localize(c, "No availabled bot found!"));
					return 0;
				}

				std::string playercount = tmpHostStr.DefaultPlayers;

				int GameNameStartIndex = 4;
				int GameModeStartIndex = 3;
				int GamePlayersStartIndex = 2;
				int TeamBalanceType = 0;

				if (args[GameNameStartIndex].empty())
				{
					GameNameStartIndex--;
					GameModeStartIndex = 2;
					if (args[GameNameStartIndex].empty())
					{
						GameModeStartIndex = 0;
						GamePlayersStartIndex = 0;
						GameNameStartIndex--;
					}
				}

				if (GamePlayersStartIndex)
				{
					bool FoundPlayerCount = false;
					for (auto s : tmpHostStr.PlayersSettings)
					{
						if (ToLower(args[GamePlayersStartIndex]) == ToLower(s.Players))
						{
							FoundPlayerCount = true;
							playercount = s.Players;
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;

							break;
						}
					}

					if (!FoundPlayerCount)
					{
						GamePlayersStartIndex = 0;
						GameModeStartIndex = 2;
					}
					else if (GameModeStartIndex == 2)
					{
						GameModeStartIndex = 0;
					}
				}

				if (GameModeStartIndex > 1)
				{
					bool FoundMode = false;
					for (auto s : tmpHostStr.HostModes)
					{
						if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
						{
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;


							if (s.Players.size())
							{
								bool FoundInPlayerList = false;
								for (auto str : s.Players)
								{
									if (ToLower(str) == ToLower(playercount))
									{
										FoundInPlayerList = true;
									}

								}
								if (!FoundInPlayerList)
								{
									message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
									return 0;
								}
							}

							FoundMode = true;
							break;
						}
					}

					if (!FoundMode)
					{
						size_t modestrlen = args[GameModeStartIndex].length();
						if (modestrlen > 1)
						{
							size_t tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "tb");
							if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
							{
								TeamBalanceType = 1;
								if (modestrlen - 2 <= 0)
								{
									GameModeStartIndex = 0;
								}
								else
									args[GameModeStartIndex] = std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
							}
							else
							{
								tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "sp");
								if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
								{
									TeamBalanceType = 2;
									if (modestrlen - 2 <= 0)
									{
										GameModeStartIndex = 0;
									}
									else
										args[GameModeStartIndex] =
										std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
								}
							}
						}

					}

					if (GameModeStartIndex > 1 && !FoundMode)
					{

						for (auto s : tmpHostStr.HostModes)
						{
							if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
							{
								if (UsingStats && !s.ForStats)
									UsingStats = s.ForStats;


								if (s.Players.size())
								{
									bool FoundInPlayerList = false;
									for (auto str : s.Players)
									{
										if (ToLower(str) == ToLower(playercount))
										{
											FoundInPlayerList = true;
										}

									}
									if (!FoundInPlayerList)
									{
										message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
										return 0;
									}
								}

								FoundMode = true;
								break;
							}
						}

					}

					if (!FoundMode)
					{
						GameModeStartIndex = 0;
					}
				}




				if (GameModeStartIndex == 0 && GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 2;
				}
				else if (GameModeStartIndex == 0 || GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 3;
				}

				std::string gamename = "";
				for (int i = GameNameStartIndex; i < 5; i++)
					gamename += " " + args[i];
				if (gamename.length() > 24)
				{
					gamename = gamename.substr(0, 24);
				}
				/*std::string msgforplayer = "Map:" + tmpHostStr.MapName;
				msgforplayer += ". Players:";
				msgforplayer += GamePlayersStartIndex ? args[ GamePlayersStartIndex] : "default";
				msgforplayer += ". Mode:";
				msgforplayer += GameModeStartIndex ? args[ GameModeStartIndex ] : "default";
				msgforplayer += ". Game name:" + gamename;
				*/
				message_send_text(c, message_type_info, c,
					localize(c, "Map:{}. Players:{}. Mode:{}. Name:{}.", tmpHostStr.MapName
						, GamePlayersStartIndex ? args[GamePlayersStartIndex] : "default",
						GameModeStartIndex ? args[GameModeStartIndex] : "default", gamename));


				//std::string hostPlayers = GamePlayersStartIndex ? args[ GamePlayersStartIndex ] : tmpHostStr.DefaultPlayers;
				uint32_t hostPlayerCount = get_sum_from_string(playercount);

				if (hostPlayerCount < 2 && tmpHostStr.stats)
				{
					message_send_text(c, message_type_error, c, localize(c, "Bad player count for host!"));
					return 0;
				}

				conn_set_host_players(c, hostPlayerCount);

				if (!tmpHostStr.stats)
				{
					std::string LoadMapCmd = fmt::format("!map {}", tmpHostStr.MapLocalPath);
					message_send_text(botconn, message_type_whisper, botconn, LoadMapCmd);
				}

				std::string HostCmd = fmt::format("!host {} {} {} {} {} {} {}", "private",
					args[1],
					playercount.empty() ? "default" : playercount,
					TeamBalanceType ? std::to_string(TeamBalanceType) : "default",
					GameModeStartIndex ? args[GameModeStartIndex] : "default", account_get_name(conn_get_account(c)), gamename);

				message_send_text(c, message_type_info, c, HostCmd);
				message_send_text(botconn, message_type_whisper, botconn, HostCmd);


				// ���� ��� args ��������� ������ � 
				// args[4] ���
				// args[3] ���
				// args[2] ���������� �������
				// args[1] ��� �����  
				// �������� ���� � args[1] ����, �� ��� �������� ������� !load dota
				// 
				//------
				// ���� � args[3] �� ���, ������ ��� � args[2]
				// 
				// 
				//------
				// ���� args[4] ����, ������ ��� � args[3]
				//------
				// ���� � args[2] �� ���������� �������
				// �� � args[2] ���
				// ���� � args[2] �� ��� �� ������ ���, args[2] + " " + args[3]
				//------
				// ���� args[3] ����, ������ ��� � args[2]
				//------
				// 
				//------


				conn_set_botconnection(c, botconn);
				set_bot_availabled(botconn, false);
				conn_set_host_timeout(c, 15);
				conn_set_host_statstype(c, tmpHostStr.MapCode.c_str());

				conn_set_host_withstats(c, false);
			}
			else
			{
				message_send_text(c, message_type_error, c, localize(c, "Code {} not found in map list!", args[1]));
			}


			return 0;
		}

		static int _handle_chost_command(t_connection* c, char const* text)
		{
			if (conn_get_game(c) && !conn_get_channel(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command not works in game!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);



			if (oldbotconnection && conn_get_hosted_game(c) && (
				game_get_status(conn_get_hosted_game(c)) == game_status_open
				||
				game_get_status(conn_get_hosted_game(c)) == game_status_full))
			{
				message_send_text(c, message_type_error, c, localize(c, "You have hosted game! Please use /unhost before start new!"));
				return 0;
			}

			if (conn_get_host_timeout(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "You can not create new game while wait own game!"));
				return 0;
			}


			// /host MAPCODE PLAYERS MAPMODE MAPNAME
			std::vector<std::string> args = split_command(text, 4);


			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}

			if (args[2].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /host mapcode players and/or mode and/or name!"));
				return 0;
			}



			MapHostStruct tmpHostStr;
			if (GetMapHostStructByCode(args[1], &tmpHostStr))
			{
				bool UsingStats = tmpHostStr.stats;
				message_send_text(c, message_type_error, c, localize(c, "Found map:{}", tmpHostStr.MapName));
				// PLAYESR args[2] - ��� ����, ���� ������ �� ������ ������� �� ���������
				t_connection* botconn = conn_search_availablebot(c);
				if (!botconn)
				{
					message_send_text(c, message_type_error, c, localize(c, "No availabled bot found!"));
					return 0;
				}

				std::string playercount = tmpHostStr.DefaultPlayers;

				int GameNameStartIndex = 4;
				int GameModeStartIndex = 3;
				int GamePlayersStartIndex = 2;
				int TeamBalanceType = 0;

				if (args[GameNameStartIndex].empty())
				{
					GameNameStartIndex--;
					GameModeStartIndex = 2;
					if (args[GameNameStartIndex].empty())
					{
						GameModeStartIndex = 0;
						GamePlayersStartIndex = 0;
						GameNameStartIndex--;
					}
				}

				if (GamePlayersStartIndex)
				{
					bool FoundPlayerCount = false;
					for (auto s : tmpHostStr.PlayersSettings)
					{
						if (ToLower(args[GamePlayersStartIndex]) == ToLower(s.Players))
						{
							FoundPlayerCount = true;
							playercount = s.Players;
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;

							break;
						}
					}

					if (!FoundPlayerCount)
					{
						GamePlayersStartIndex = 0;
						GameModeStartIndex = 2;
					}
					else if (GameModeStartIndex == 2)
					{
						GameModeStartIndex = 0;
					}
				}

				if (GameModeStartIndex > 1)
				{
					bool FoundMode = false;
					for (auto s : tmpHostStr.HostModes)
					{
						if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
						{
							if (UsingStats && !s.ForStats)
								UsingStats = s.ForStats;


							if (s.Players.size())
							{
								bool FoundInPlayerList = false;
								for (auto str : s.Players)
								{
									if (ToLower(str) == ToLower(playercount))
									{
										FoundInPlayerList = true;
									}

								}
								if (!FoundInPlayerList)
								{
									message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
									return 0;
								}
							}

							FoundMode = true;
							break;
						}
					}

					if (!FoundMode)
					{
						size_t modestrlen = args[GameModeStartIndex].length();
						if (modestrlen > 1)
						{
							size_t tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "tb");
							if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
							{
								TeamBalanceType = 1;
								if (modestrlen - 2 <= 0)
								{
									GameModeStartIndex = 0;
								}
								else
									args[GameModeStartIndex] = std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
							}
							else
							{
								tb_mode = findLastOf(ToLower(args[GameModeStartIndex]), "sp");
								if (tb_mode != std::string::npos && tb_mode == modestrlen - 2)
								{
									TeamBalanceType = 2;
									if (modestrlen - 2 <= 0)
									{
										GameModeStartIndex = 0;
									}
									else
										args[GameModeStartIndex] =
										std::string(args[GameModeStartIndex].begin(), args[GameModeStartIndex].end() - 2);
								}
							}
						}

					}

					if (GameModeStartIndex > 1 && !FoundMode)
					{

						for (auto s : tmpHostStr.HostModes)
						{
							if (ToLower(args[GameModeStartIndex]) == ToLower(s.Mode))
							{
								if (UsingStats && !s.ForStats)
									UsingStats = s.ForStats;


								if (s.Players.size())
								{
									bool FoundInPlayerList = false;
									for (auto str : s.Players)
									{
										if (ToLower(str) == ToLower(playercount))
										{
											FoundInPlayerList = true;
										}

									}
									if (!FoundInPlayerList)
									{
										message_send_text(c, message_type_error, c, localize(c, "This mode {} can not be used with players:{}", s.Mode, playercount));
										return 0;
									}
								}

								FoundMode = true;
								break;
							}
						}

					}

					if (!FoundMode)
					{
						GameModeStartIndex = 0;
					}
				}




				if (GameModeStartIndex == 0 && GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 2;
				}
				else if (GameModeStartIndex == 0 || GamePlayersStartIndex == 0)
				{
					GameNameStartIndex = 3;
				}

				std::string gamename = "";
				for (int i = GameNameStartIndex; i < 5; i++)
					gamename += " " + args[i];
				if (gamename.length() > 24)
				{
					gamename = gamename.substr(0, 24);
				}
				/*std::string msgforplayer = "Map:" + tmpHostStr.MapName;
				msgforplayer += ". Players:";
				msgforplayer += GamePlayersStartIndex ? args[ GamePlayersStartIndex] : "default";
				msgforplayer += ". Mode:";
				msgforplayer += GameModeStartIndex ? args[ GameModeStartIndex ] : "default";
				msgforplayer += ". Game name:" + gamename;
				*/
				message_send_text(c, message_type_info, c,
					localize(c, "Map:{}. Players:{}. Mode:{}. Name:{}.", tmpHostStr.MapName
						, GamePlayersStartIndex ? args[GamePlayersStartIndex] : "default",
						GameModeStartIndex ? args[GameModeStartIndex] : "default", gamename));


				//std::string hostPlayers = GamePlayersStartIndex ? args[ GamePlayersStartIndex ] : tmpHostStr.DefaultPlayers;
				uint32_t hostPlayerCount = get_sum_from_string(playercount);

				if (hostPlayerCount < 2 && tmpHostStr.stats)
				{
					message_send_text(c, message_type_error, c, localize(c, "Bad player count for host!"));
					return 0;
				}

				conn_set_host_players(c, hostPlayerCount);

				if (!tmpHostStr.stats)
				{
					std::string LoadMapCmd = fmt::format("!map {}", tmpHostStr.MapLocalPath);
					message_send_text(botconn, message_type_whisper, botconn, LoadMapCmd);
				}

				std::string HostCmd = fmt::format("!host {} {} {} {} {} {} {}", "custom",
					args[1],
					playercount.empty() ? "default" : playercount,
					TeamBalanceType ? std::to_string(TeamBalanceType) : "default",
					GameModeStartIndex ? args[GameModeStartIndex] : "default", account_get_name(conn_get_account(c)), gamename);

				message_send_text(c, message_type_info, c, HostCmd);
				message_send_text(botconn, message_type_whisper, botconn, HostCmd);


				// ���� ��� args ��������� ������ � 
				// args[4] ���
				// args[3] ���
				// args[2] ���������� �������
				// args[1] ��� �����  
				// �������� ���� � args[1] ����, �� ��� �������� ������� !load dota
				// 
				//------
				// ���� � args[3] �� ���, ������ ��� � args[2]
				// 
				// 
				//------
				// ���� args[4] ����, ������ ��� � args[3]
				//------
				// ���� � args[2] �� ���������� �������
				// �� � args[2] ���
				// ���� � args[2] �� ��� �� ������ ���, args[2] + " " + args[3]
				//------
				// ���� args[3] ����, ������ ��� � args[2]
				//------
				// 
				//------


				conn_set_botconnection(c, botconn);
				set_bot_availabled(botconn, false);
				conn_set_host_timeout(c, 15);
				conn_set_host_statstype(c, tmpHostStr.MapCode.c_str());
				conn_set_host_withstats(c, false);
			}
			else
			{
				message_send_text(c, message_type_error, c, localize(c, "Code {} not found in map list!", args[1]));
			}



			return 0;
		}


		static int _handle_unhost_command(t_connection* c, char const* text)
		{
			t_connection* oldbotconnection = conn_get_botconnection(c);
			if (!oldbotconnection || !conn_get_hosted_game(c) || conn_get_game(oldbotconnection) != conn_get_hosted_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}
			message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!unhost");
			return 0;
		}

		static int _handle_swap_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);

			if (args[2].empty() || args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /swap # #"));
				return 0;
			}
			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}
			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if (conn_get_game(oldbotconnection) != currentgame)
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!swap " + args[1] + " " + args[2]);

			return 0;
		}


		static int _handle_open_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /open #"));
				return 0;
			}

			if (!conn_get_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if (conn_get_game(oldbotconnection) != conn_get_hosted_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}
			message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!open " + args[1]);

			return 0;
		}

		static int _handle_hcl_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Example usage /hcl ar"));
				return 0;
			}

			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if (conn_get_game(oldbotconnection) != conn_get_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!hcl " + args[1]);

			return 0;
		}

		static int _handle_holdslots_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "error"));
				return 0;
			}

			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if ((currentgame != conn_get_game(oldbotconnection)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
			{
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!holdslot " + args[1]);
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!hostfornewclient");
			}

			return 0;
		}

		static int _handle_hold_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /hold [names]"));
				return 0;
			}

			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if ((currentgame != conn_get_game(oldbotconnection)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!hold " + args[1]);

			return 0;
		}

		static int _handle_corace_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /comp [slot, level]"));
				return 0;
			}

			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if ((currentgame != conn_get_game(oldbotconnection)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!corace " + args[1]);

			return 0;
		}

		static int _handle_comp_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /comp [slot, level]"));
				return 0;
			}

			t_game* currentgame = NULL;

			if (!(currentgame = conn_get_hosted_game(c)))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}

			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if ((currentgame != conn_get_game(oldbotconnection)))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(currentgame))
			{
				message_send_text(c, message_type_error, c, localize(c, "Command not allowed in this game!"));
			}
			else
				message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!comp " + args[1]);

			return 0;
		}

		static int _handle_start_command(t_connection* c, char const* text)
		{
			//	std::vector<std::string> args = split_command( text, 1 );


			t_game* hostedgame = conn_get_hosted_game(c);
			if (!hostedgame || !conn_get_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}
			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if (conn_get_game(oldbotconnection) != hostedgame)
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}

			if (game_get_forstats(hostedgame) && game_get_statstype(hostedgame) && game_get_statstype(hostedgame)[0] != '\0')
			{
				if (game_get_ref(hostedgame) - 1 != conn_get_host_players(c))
				{
					message_send_text(c, message_type_error, c, localize(c, "Please wait while {} players enter game!", ((int)conn_get_host_players(c) - (int)game_get_ref(hostedgame) + 1)));
					return 0;
				}
			}

			//	if ( !args[ 1 ].empty( ) )
				//	message_send_text( oldbotconnection, message_type_whisper, oldbotconnection, "!start force" );
			//	else
			message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!start");

			return 0;
		}


		static int _handle_abort_command(t_connection* c, char const* text)
		{
			if (!conn_get_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "This command works only ingame!"));
				return 0;
			}
			t_connection* oldbotconnection = conn_get_botconnection(c);

			if (!oldbotconnection)
			{
				message_send_text(c, message_type_error, c, localize(c, "Need create game before using this commands!"));
				return 0;
			}

			if (conn_get_game(oldbotconnection) != conn_get_hosted_game(c))
			{
				message_send_text(c, message_type_error, c, localize(c, "Access denied for this game!"));
				return 0;
			}
			message_send_text(oldbotconnection, message_type_whisper, oldbotconnection, "!abort");
			return 0;
		}


		static int _handle_reloadmaplist_command(t_connection* c, char const* text)
		{

			std::vector<std::string> args = split_command(text, 5);
			if (args[5].empty() && c)
			{
				message_send_text(c, message_type_error, c, localize(c, "Usage /reloadmaplist (SECRET ARGUMENTS)"));
				return 0;
			}
			else if (args[5].empty())
			{
				return 0;
			}
			std::string FtpIp = args[1];
			int FtpPort = std::atoi(args[2].c_str());
			std::string FtpUser = args[3];
			std::string FtpPassword = args[4];
			std::string FtpStartDir = args[5];

			fs::path curpath = fs::current_path();
			auto CustomMapDir = curpath / "mapcodescustom";
			auto HostMapDir = curpath / "maphostcodes";
			eventlog(eventlog_level_error, __FUNCTION__, "Start connect to ftp and update map configs. ({} {} {})", FtpIp, FtpPort, FtpUser);

			conn_send_bot_command("!UpdateMapList " + args[1] + " " + args[2] + " "
				+ args[3] + " " + args[4] + " " + args[5]);

			eventlog(eventlog_level_error, __FUNCTION__, "Bot command send!");

			try
			{
				std::thread{ UpadeMapList, FtpIp, FtpPort, FtpUser,FtpPassword ,FtpStartDir,std::string("maphostcodes/"),std::string("mapcodescustom/") }.detach();
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "Error while map configs update");
			}
			eventlog(eventlog_level_error, __FUNCTION__, "Start reload maplist.");


			return 0;
		}


		static int _handle_maplist_command(t_connection* c, char const* text)
		{
			PrintMapList(c);
			return 0;
		}

		static int _handle_banrequest_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);
			t_account* acc = conn_get_account(c);
			if (acc && !args[1].empty() && !args[2].empty())
			{
				t_account* targetacc = accountlist_find_account(args[1].c_str());

				if (IsHostBot(account_get_name(acc)))
				{
					if (targetacc)
					{


						eventlog(eventlog_level_error, __FUNCTION__, "ban request  by {}.  {}. {}. {}. {}.!", account_get_name(acc), args[1], args[2], targetacc ? "Account found!" : "Account not found!", account_get_auth_lock(targetacc) ? "Account locked!" : "Account not locked!");
						/*	account_set_auth_lock( targetacc, 1 );
							account_set_auth_lockreason( targetacc, ( std::string( "Ban by bot! Reason:" ) + args[ 2 ] ).c_str( ) );
									*/
						if (account_get_conn(targetacc))
						{
							account_add_newevent(targetacc, 8);
							conn_send_ah_gamemessagebox(account_get_conn(targetacc), 1, ("|c0000FF40Test Protection:|r|n |c00FF8000Ban with reason:|r |c00FF0000Ban by bot! Reason: " + args[2]).c_str());
							msgtemp = localize(c, "Your account has just been locked");
							msgtemp += account_get_locktext(c, targetacc, true);
							message_send_text(account_get_conn(targetacc), message_type_error, account_get_conn(targetacc), msgtemp);
							conn_set_state(account_get_conn(targetacc), conn_state_destroy);
						}

					}
				}

			}
			eventlog(eventlog_level_error, __FUNCTION__, "ban request end!");
			message_send_text(c, message_type_error, c, "ban success!");
			return 0;
		}

		static int _handle_gamemessage_command(t_connection* c, char const* text)
		{

			std::smatch matchGameNameAndMessage;
			std::string GameName = "";
			std::string GameMessage = "";
			std::string Text_str = text;
			t_game* targetgame = NULL;

			if (std::regex_match(Text_str, matchGameNameAndMessage, GetGameNameAndMessage))
			{
				GameName = matchGameNameAndMessage[1].str();
				GameMessage = matchGameNameAndMessage[2].str();
			}
			eventlog(eventlog_level_error, __FUNCTION__, "command {} game {} message {}!", text, GameName.empty() ? "UNKNOWN NAME" : GameName, GameMessage.empty() ? "NO MESSAGE" : GameMessage);

			if (!GameMessage.empty() && (targetgame = gamelist_find_game(GameName.c_str(), conn_get_clienttag(c), t_game_type::game_type_all)))
			{
				game_send_message(targetgame, GameMessage);
			}
			eventlog(eventlog_level_error, __FUNCTION__, "game message end!");
			message_send_text(c, message_type_error, c, "gamemessage success!");
			return 0;
		}


		static int _handle_cclearstats_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 1);

			if (args[1].empty())
			{
				message_send_text(c, message_type_error, c, "bad map code!");
			}
			else
			{
				t_account* useracc = conn_get_account(c);
				if (useracc)
				{
					bool FoundDota = false;
					if (args[1].find("dota") == 0)
					{
						for (auto& mhost : MapHostStructList)
						{
							if (args[1] == mhost.MapCode)
							{
								FoundDota = true;
							}
						}
					}

					if (FoundDota)
					{
						account_map_set_stats(useracc, args[1], "mmr", 1000);
						account_map_set_stats(useracc, args[1], "wins", 0);
						account_map_set_stats(useracc, args[1], "loses", 0);
						account_map_set_stats(useracc, args[1], "leaves", 0);
						account_map_set_stats(useracc, args[1], "streak", 0);
						account_map_set_stats(useracc, args[1], "minstreak", 0);
						account_map_set_stats(useracc, args[1], "maxstreak", 0);
						account_map_set_stats(useracc, args[1], "kills", 0);
						account_map_set_stats(useracc, args[1], "deaths", 0);
						account_map_set_stats(useracc, args[1], "assists", 0);

						account_print_map_stats(useracc, args[1]);
					}
					else if (args[1] == "default")
					{
						account_map_set_stats(useracc, args[1], "games", 0);
						account_map_set_stats(useracc, args[1], "leaves", 0);
						account_print_map_stats(useracc, args[1]);
					}
					else
					{
						message_send_text(c, message_type_error, c, localize(c, "unknown mapname"));
					}
				}

			}

			return 0;
		}

		static int _handle_setahcrcvalue_command(t_connection* c, char const* text)
		{
			std::vector<std::string> args = split_command(text, 2);

			message_send_text(c, message_type_error, c, "bad amh hash!");

			if (args[1].empty())
			{
				return 0;
			}
			message_send_text(c, message_type_error, c, "ok amh hash!");

			if (!args[2].empty())
			{
				message_send_text(c, message_type_error, c, "success!");
				return 0;
			}

			message_send_text(c, message_type_error, c, "very ok amh hash!");

			std::istringstream reader(args[1]);
			unsigned int val;
			reader >> val;

			AddAMHHash(val);

			return 0;
		}

		static int _handle_testmessagebox_command(t_connection* c, char const* text)
		{
			conn_send_ah_gamemessagebox(c, 2, "Hello from messagebox!");
			return 0;
		}
	}
}
