/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999,2000  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2003 Dizzy
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

#include "common/setup_before.h"
#include "handle_bnet.h"
#include <string>
#include <vector>

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <tuple>

#include <nonstd/optional.hpp>

#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "common/packet.h"
#include "common/eventlog.h"
#include "common/tag.h"
#include "common/bn_type.h"
#include "common/addr.h"
#include "common/bnettime.h"
#include "common/trans.h"
#include "common/list.h"
#include "common/bnethash.h"
#include "common/bnethashconv.h"
#include "common/proginfo.h"
#include "common/util.h"
#include "common/bnetsrp3.h"
#include "common/xstring.h"

#include "account_email_verification.h"
#include "handlers.h"
#include "connection.h"
#include "prefs.h"
#include "versioncheck.h"
#include "handle_anongame.h"
#include "account.h"
#include "account_wrap.h"
#include "clan.h"
#include "channel.h"
#include "game.h"
#include "game_conv.h"
#include "ladder.h"
#include "watch.h"
#include "realm.h"
#include "adbanner.h"
#include "character.h"
#include "command.h"
#include "tick.h"
#include "message.h"
#include "file.h"
#include "news.h"
#include "team.h"
#include "server.h"
#include "friends.h"
#include "autoupdate.h"
#include "anongame.h"
#include "i18n.h"

#include "bcrypt.h"

#ifdef WIN32_GUI
#include <win32/winmain.h>
#endif
#include "common/setup_after.h"
#ifdef WITH_LUA
#include "luainterface.h"
#endif
 //here
int ANTIHACK_VERSION = 0x218;
int LAUNCHER_VERSION = 9;

std::vector<std::string> HostBotUsers;
std::vector<std::string> CrcWhiteList;
std::vector<std::string> BlackListUsers;

std::vector<unsigned int> AMH_ACCESS_HASHES;


namespace pvpgn
{

	namespace bnetd
	{

		extern void AddAMHHash(unsigned int hash)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "[AMH HASH] added {}", hash);
			AMH_ACCESS_HASHES.push_back(hash);
			if (AMH_ACCESS_HASHES.size() > 15)
			{
				AMH_ACCESS_HASHES.erase(AMH_ACCESS_HASHES.begin());
			}

		}

		extern bool IsAMHHashOkay(unsigned int hash)
		{
			if (hash == 'test')
				return true;

			eventlog(eventlog_level_trace, __FUNCTION__, "[AMH HASH] check {}", hash);
			for (auto& s : AMH_ACCESS_HASHES)
			{
				if (s == hash)
					return true;
			}

			return false;
		}

		extern void InitHostBotList()
		{
			if (!HostBotUsers.empty())
				HostBotUsers.clear();

			const char* whitelistfilename = prefs_get_hostbotlistfile();
			std::ifstream file(whitelistfilename);
			if (file.is_open())
			{
				std::string line;
				while (std::getline(file, line))
				{
					if (!line.empty())
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[WHITELIST] added {}", line.c_str());
						HostBotUsers.push_back(line);
					}
				}
			}
		}

		extern bool IsHostBot(const char* name)
		{
			if (name && name[0] != '\0')
			{
				for (std::string s : HostBotUsers)
				{
					if (ToLower(s) == ToLower(name))
						return true;
				}
			}
			return false;
		}

		extern void InitCrcWhiteList()
		{
			if (!CrcWhiteList.empty())
				CrcWhiteList.clear();

			const char* crcwhitelistfilename = prefs_get_crcwhitelistfile();
			std::ifstream file(crcwhitelistfilename);
			if (file.is_open())
			{
				std::string line;
				while (std::getline(file, line)) {
					if (!line.empty())
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[CRCWHITELIST] added {}", line.c_str());
						CrcWhiteList.push_back(line);
					}
				}
			}
		}

		extern bool IsInCrcWhiteList(const char* crc)
		{
			if (crc && crc[0] != '\0')
			{
				for (std::string s : CrcWhiteList)
				{
					if (ToLower(s) == ToLower(crc))
						return true;
				}
			}
			return false;
		}


		extern void InitBlackList()
		{
			if (!BlackListUsers.empty())
				BlackListUsers.clear();

			const char* Blacklistfilename = prefs_get_blacklistfile();
			std::ifstream file(Blacklistfilename);
			if (file.is_open())
			{
				std::string line;
				while (std::getline(file, line)) {
					if (!line.empty())
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[BLACKLIST] added {}", line.c_str());
						BlackListUsers.push_back(line);
					}
				}
			}
		}


		extern bool IsInBlackList(const char* str)
		{
			if (str && str[0] != '\0')
			{
				for (std::string s : BlackListUsers)
				{
					if (ToLower(s) == ToLower(str))
						return true;
				}
			}
			return false;
		}


		// 


		extern int last_news;
		extern int first_news;

		/* handlers prototypes */
		static int _client_unknown_1b(t_connection* c, t_packet const* const packet);
		static int _client_compinfo1(t_connection* c, t_packet const* const packet);
		static int _client_compinfo2(t_connection* c, t_packet const* const packet);
		static int _client_countryinfo1(t_connection* c, t_packet const* const packet);
		static int _client_auth_info(t_connection* c, t_packet const* const packet);
		static int _client_unknown2b(t_connection* c, t_packet const* const packet);
		static int _client_progident(t_connection* c, t_packet const* const packet);
		static int _client_createaccountw3(t_connection* c, t_packet const* const packet);
		static int _client_createacctreq1(t_connection* c, t_packet const* const packet);
		static int _client_createacctreq2(t_connection* c, t_packet const* const packet);
		static int _client_changepassreq(t_connection* c, t_packet const* const packet);
		static int _client_echoreply(t_connection* c, t_packet const* const packet);
		static int _client_authreq1(t_connection* c, t_packet const* const packet);
		static int _client_authreq109(t_connection* c, t_packet const* const packet);
		static int _client_regsnoopreply(t_connection* c, t_packet const* const packet);
		static int _client_iconreq(t_connection* c, t_packet const* const packet);
		static int _client_cdkey(t_connection* c, t_packet const* const packet);
		static int _client_cdkey2(t_connection* c, t_packet const* const packet);
		static int _client_cdkey3(t_connection* c, t_packet const* const packet);
		static int _client_udpok(t_connection* c, t_packet const* const packet);
		static int _client_fileinforeq(t_connection* c, t_packet const* const packet);
		static int _client_statsreq(t_connection* c, t_packet const* const packet);
		static int _client_loginreq1(t_connection* c, t_packet const* const packet);
		static int _client_loginreq2(t_connection* c, t_packet const* const packet);
		static int _client_loginreqw3(t_connection* c, t_packet const* const packet);
		static int _client_passchangereq(t_connection* c, t_packet const* const packet);
		static int _client_passchangeproofreq(t_connection* c, t_packet const* const packet);
		static int _client_pingreq(t_connection* c, t_packet const* const packet);
		static int _client_logonproofreq(t_connection* c, t_packet const* const packet);
		static int _client_changegameport(t_connection* c, t_packet const* const packet);
		static int _client_friendslistreq(t_connection* c, t_packet const* const packet);
		static int _client_friendinforeq(t_connection* c, t_packet const* const packet);
		static int _client_atfriendscreen(t_connection* c, t_packet const* const packet);
		static int _client_atinvitefriend(t_connection* c, t_packet const* const packet);
		static int _client_atacceptinvite(t_connection* c, t_packet const* const packet);
		static int _client_atacceptdeclineinvite(t_connection* c, t_packet const* const packet);
		static int _client_motdw3(t_connection* c, t_packet const* const packet);
		static int _client_realmlistreq(t_connection* c, t_packet const* const packet);
		static int _client_realmlistreq110(t_connection* c, t_packet const* const packet);
		static int _client_profilereq(t_connection* c, t_packet const* const packet);
		static int _client_realmjoinreq109(t_connection* c, t_packet const* const packet);
		static int _client_unknown39(t_connection* c, t_packet const* const packet);
		static int _client_charlistreq(t_connection* c, t_packet const* const packet);
		static int _client_adreq(t_connection* c, t_packet const* const packet);
		static int _client_adack(t_connection* c, t_packet const* const packet);
		static int _client_adclick(t_connection* c, t_packet const* const packet);
		static int _client_adclick2(t_connection* c, t_packet const* const packet);
		static int _client_readmemory(t_connection* c, t_packet const* const packet);
		static int _client_statsupdate(t_connection* c, t_packet const* const packet);
		static int _client_playerinforeq(t_connection* c, t_packet const* const packet);
		static int _client_progident2(t_connection* c, t_packet const* const packet);
		static int _client_joinchannel(t_connection* c, t_packet const* const packet);
		static int _client_message(t_connection* c, t_packet const* const packet);
		static int _client_gamelistreq(t_connection* c, t_packet const* const packet);
		static int _client_joingame(t_connection* c, t_packet const* const packet);
		static int _client_startgame1(t_connection* c, t_packet const* const packet);
		static int _client_startgame3(t_connection* c, t_packet const* const packet);
		static int _client_startgame4(t_connection* c, t_packet const* const packet);
		static int _client_closegame(t_connection* c, t_packet const* const packet);
		static int _client_gamereport(t_connection* c, t_packet const* const packet);
		static int _client_leavechannel(t_connection* c, t_packet const* const packet);
		static int _client_ladderreq(t_connection* c, t_packet const* const packet);
		static int _client_laddersearchreq(t_connection* c, t_packet const* const packet);
		static int _client_mapauthreq1(t_connection* c, t_packet const* const packet);
		static int _client_mapauthreq2(t_connection* c, t_packet const* const packet);
		static int _client_changeclient(t_connection* c, t_packet const* const packet);
		static int _client_clanmemberlistreq(t_connection* c, t_packet const* const packet);
		static int _client_clan_motdreq(t_connection* c, t_packet const* const packet);
		static int _client_clan_motdchg(t_connection* c, t_packet const* const packet);
		static int _client_clan_createreq(t_connection* c, t_packet const* const packet);
		static int _client_clan_createinvitereq(t_connection* c, t_packet const* const packet);
		static int _client_clan_createinvitereply(t_connection* c, t_packet const* const packet);
		static int _client_clan_disbandreq(t_connection* c, t_packet const* const packet);
		static int _client_clanmember_rankupdatereq(t_connection* c, t_packet const* const packet);
		static int _client_clanmember_removereq(t_connection* c, t_packet const* const packet);
		static int _client_clan_membernewchiefreq(t_connection* c, t_packet const* const packet);
		static int _client_clan_invitereq(t_connection* c, t_packet const* const packet);
		static int _client_clan_invitereply(t_connection* c, t_packet const* const packet);
		static int _client_crashdump(t_connection* c, t_packet const* const packet);
		static int _client_setemailreply(t_connection* c, t_packet const* const packet);
		static int _client_changeemailreq(t_connection* c, t_packet const* const packet);
		static int _client_getpasswordreq(t_connection* c, t_packet const* const packet);
		static int _client_claninforeq(t_connection* c, t_packet const* const packet);
		static int _client_extrawork(t_connection* c, t_packet const* const packet);
		static int _client_ah_scan_response(t_connection* c, t_packet const* const packet);

		/* connection state connected handler table */
		static const t_htable_row bnet_htable_con[] = {
			{ CLIENT_UNKNOWN_1B, _client_unknown_1b },
			{ CLIENT_COMPINFO1, _client_compinfo1 },
			{ CLIENT_COMPINFO2, _client_compinfo2 },
			{ CLIENT_COUNTRYINFO1, _client_countryinfo1 },
			{ CLIENT_AUTH_INFO, _client_auth_info },
			{ CLIENT_UNKNOWN_2B, _client_unknown2b },
			{ CLIENT_PROGIDENT, _client_progident },
			{ CLIENT_CLOSEGAME, NULL },
			{ CLIENT_CREATEACCOUNT_W3, _client_createaccountw3 },
			{ CLIENT_CREATEACCTREQ1, _client_createacctreq1 },
			{ CLIENT_CREATEACCTREQ2, _client_createacctreq2 },
			{ CLIENT_CHANGEPASSREQ, _client_changepassreq },
			{ CLIENT_ECHOREPLY, _client_echoreply },
			{ CLIENT_AUTHREQ1, _client_authreq1 },
			{ CLIENT_AUTHREQ_109, _client_authreq109 },
			{ CLIENT_REGSNOOPREPLY, _client_regsnoopreply },
			{ CLIENT_ICONREQ, _client_iconreq },
			{ CLIENT_CDKEY, _client_cdkey },
			{ CLIENT_CDKEY2, _client_cdkey2 },
			{ CLIENT_CDKEY3, _client_cdkey3 },
			{ CLIENT_UDPOK, _client_udpok },
			{ CLIENT_FILEINFOREQ, _client_fileinforeq },
			{ CLIENT_STATSREQ, _client_statsreq },
			{ CLIENT_PINGREQ, _client_pingreq },
			{ CLIENT_LOGINREQ1, _client_loginreq1 },
			{ CLIENT_LOGINREQ2, _client_loginreq2 },
			{ CLIENT_LOGINREQ_W3, _client_loginreqw3 },
			{ CLIENT_PASSCHANGEREQ, _client_passchangereq },
			{ CLIENT_PASSCHANGEPROOFREQ, _client_passchangeproofreq },
			{ CLIENT_LOGONPROOFREQ, _client_logonproofreq },
			{ CLIENT_CHANGECLIENT, _client_changeclient },
			{ CLIENT_GETPASSWORDREQ, _client_getpasswordreq },
			{ CLIENT_CHANGEEMAILREQ, _client_changeemailreq },
			{ CLIENT_CRASHDUMP, _client_crashdump },
			{ CLIENT_AH_SCAN_RESPONSE, _client_ah_scan_response },
			{ -1, NULL }
		};

		/* connection state loggedin handlers */
		static const t_htable_row bnet_htable_log[] = {
			{ CLIENT_CHANGEGAMEPORT, _client_changegameport },
			{ CLIENT_FRIENDSLISTREQ, _client_friendslistreq },
			{ CLIENT_FRIENDINFOREQ, _client_friendinforeq },
			{ CLIENT_ARRANGEDTEAM_FRIENDSCREEN, _client_atfriendscreen },
			{ CLIENT_ARRANGEDTEAM_INVITE_FRIEND, _client_atinvitefriend },
			{ CLIENT_ARRANGEDTEAM_ACCEPT_INVITE, _client_atacceptinvite },
			{ CLIENT_ARRANGEDTEAM_ACCEPT_DECLINE_INVITE, _client_atacceptdeclineinvite },
			/* anongame packet (44ff) handled in handle_anongame.c */
			{ CLIENT_FINDANONGAME, handle_anongame_packet },
			{ CLIENT_FILEINFOREQ, _client_fileinforeq },
			{ CLIENT_MOTD_W3, _client_motdw3 },
			{ CLIENT_REALMLISTREQ, _client_realmlistreq },
			{ CLIENT_REALMLISTREQ_110, _client_realmlistreq110 },
			{ CLIENT_PROFILEREQ, _client_profilereq },
			{ CLIENT_REALMJOINREQ_109, _client_realmjoinreq109 },
			{ CLIENT_UNKNOWN_37, _client_charlistreq },
			{ CLIENT_UNKNOWN_39, _client_unknown39 },
			{ CLIENT_ECHOREPLY, _client_echoreply },
			{ CLIENT_PINGREQ, _client_pingreq },
			{ CLIENT_ADREQ, _client_adreq },
			{ CLIENT_ADACK, _client_adack },
			{ CLIENT_ADCLICK, _client_adclick },
			{ CLIENT_ADCLICK2, _client_adclick2 },
			{ CLIENT_READMEMORY, _client_readmemory },
			{ CLIENT_STATSREQ, _client_statsreq },
			{ CLIENT_STATSUPDATE, _client_statsupdate },
			{ CLIENT_PLAYERINFOREQ, _client_playerinforeq },
			{ CLIENT_PROGIDENT2, _client_progident2 },
			{ CLIENT_JOINCHANNEL, _client_joinchannel },
			{ CLIENT_MESSAGE, _client_message },
			{ CLIENT_GAMELISTREQ, _client_gamelistreq },
			{ CLIENT_JOIN_GAME, _client_joingame },
			{ CLIENT_STARTGAME1, _client_startgame1 },
			{ CLIENT_STARTGAME3, _client_startgame3 },
			{ CLIENT_STARTGAME4, _client_startgame4 },
			{ CLIENT_CLOSEGAME, _client_closegame },
			{ CLIENT_CLOSEGAME2, _client_closegame },
			{ CLIENT_GAME_REPORT, _client_gamereport },
			{ CLIENT_LEAVECHANNEL, _client_leavechannel },
			{ CLIENT_LADDERREQ, _client_ladderreq },
			{ CLIENT_LADDERSEARCHREQ, _client_laddersearchreq },
			{ CLIENT_MAPAUTHREQ1, _client_mapauthreq1 },
			{ CLIENT_MAPAUTHREQ2, _client_mapauthreq2 },
			{ CLIENT_CLAN_DISBANDREQ, _client_clan_disbandreq },
			{ CLIENT_CLANMEMBERLIST_REQ, _client_clanmemberlistreq },
			{ CLIENT_CLAN_MOTDCHG, _client_clan_motdchg },
			{ CLIENT_CLAN_MOTDREQ, _client_clan_motdreq },
			{ CLIENT_CLAN_CREATEREQ, _client_clan_createreq },
			{ CLIENT_CLAN_CREATEINVITEREQ, _client_clan_createinvitereq },
			{ CLIENT_CLAN_CREATEINVITEREPLY, _client_clan_createinvitereply },
			{ CLIENT_CLANMEMBER_RANKUPDATE_REQ, _client_clanmember_rankupdatereq },
			{ CLIENT_CLANMEMBER_REMOVE_REQ, _client_clanmember_removereq },
			{ CLIENT_CLAN_MEMBERNEWCHIEFREQ, _client_clan_membernewchiefreq },
			{ CLIENT_CLAN_INVITEREQ, _client_clan_invitereq },
			{ CLIENT_CLAN_INVITEREPLY, _client_clan_invitereply },
			{ CLIENT_CRASHDUMP, _client_crashdump },
			{ CLIENT_SETEMAILREPLY, _client_setemailreply },
			{ CLIENT_CLANINFOREQ, _client_claninforeq },
			{ CLIENT_EXTRAWORK, _client_extrawork },
			{ CLIENT_AH_SCAN_RESPONSE, _client_ah_scan_response },
			{ CLIENT_NULL, NULL },
			{ -1, NULL }
		};

		/* main handler function */
		static int handle(const t_htable_row* htable, int type, t_connection* c, t_packet const* const packet);

		extern int handle_bnet_packet(t_connection* c, t_packet const* const packet)
		{
			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL connection", conn_get_socket(c));
				return -1;
			}
			if (!packet) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet", conn_get_socket(c));
				return -1;
			}
			if (packet_get_class(packet) != packet_class_bnet) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad packet (class {})", conn_get_socket(c), (int)packet_get_class(packet));
				return -1;
			}

			switch (conn_get_state(c)) {
			case conn_state_connected:
				switch (handle(bnet_htable_con, packet_get_type(packet), c, packet)) {
				case 1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (connected) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				case -1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] (connected) got error handling packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				};
				break;

			case conn_state_loggedin:
				switch (handle(bnet_htable_log, packet_get_type(packet), c, packet)) {
				case 1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (logged in) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				case -1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] (logged in) got error handling packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				};
				break;

			case conn_state_untrusted:
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (untrusted) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
				break;

			default:
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid login state {}", conn_get_socket(c), conn_get_state(c));
			};

			return 0;
		}

		static int handle(const t_htable_row* htable, int type, t_connection* c, t_packet const* const packet)
		{
			t_htable_row const* p;
			int res = 1;

			for (p = htable; p->type != -1; p++)
				if (p->type == type) {
					res = 0;
					if (p->handler != NULL)
						res = p->handler(c, packet);
					if (res != 2)
						break;		/* return 2 means we want to continue parsing */
				}

			return res;
		}

		/* handlers for bnet packets */
		static int _client_unknown_1b(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_unknown_1b)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_1B packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_1b), packet_get_size(packet));
				return -1;
			}

			{
				unsigned int newip;
				unsigned short newport;

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown1=0x{:04hx}", conn_get_socket(c), bn_short_get(packet->u.client_unknown_1b.unknown1));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown2=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_unknown_1b.unknown2));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown3=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_unknown_1b.unknown3));

				newip = bn_int_nget(packet->u.client_unknown_1b.ip);
				newport = bn_short_nget(packet->u.client_unknown_1b.port);

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] UNKNOWN_1B set new UDP address to {}", conn_get_socket(c), addr_num_to_addr_str(newip, newport));
				conn_set_game_addr(c, newip);
				conn_set_game_port(c, newport);
			}
			return 0;
		}

		static int _client_compinfo1(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_compinfo1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_compinfo1), packet_get_size(packet));
				return -1;
			}

			{
				char const* host;
				char const* user;

				if (!(host = packet_get_str_const(packet, sizeof(t_client_compinfo1), MAX_WINHOST_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (missing or too long host)", conn_get_socket(c));
					return -1;
				}
				if (!(user = packet_get_str_const(packet, sizeof(t_client_compinfo1) + std::strlen(host) + 1, MAX_WINUSER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (missing or too long user)", conn_get_socket(c));
					return -1;
				}

				conn_set_host(c, host);
				conn_set_user(c, user);
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_compreply));
				packet_set_type(rpacket, SERVER_COMPREPLY);
				bn_int_set(&rpacket->u.server_compreply.reg_version, SERVER_COMPREPLY_REG_VERSION);
				bn_int_set(&rpacket->u.server_compreply.reg_auth, SERVER_COMPREPLY_REG_AUTH);
				bn_int_set(&rpacket->u.server_compreply.client_id, SERVER_COMPREPLY_CLIENT_ID);
				bn_int_set(&rpacket->u.server_compreply.client_token, SERVER_COMPREPLY_CLIENT_TOKEN);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_sessionkey1));
				packet_set_type(rpacket, SERVER_SESSIONKEY1);
				bn_int_set(&rpacket->u.server_sessionkey1.sessionkey, conn_get_sessionkey(c));
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			return 0;
		}

		static int _client_compinfo2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_compinfo2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_compinfo2), packet_get_size(packet));
				return -1;
			}

			{
				char const* host;
				char const* user;

				if (!(host = packet_get_str_const(packet, sizeof(t_client_compinfo2), MAX_WINHOST_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (missing or too long host)", conn_get_socket(c));
					return -1;
				}
				if (!(user = packet_get_str_const(packet, sizeof(t_client_compinfo2) + std::strlen(host) + 1, MAX_WINUSER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (missing or too long user)", conn_get_socket(c));
					return -1;
				}

				conn_set_host(c, host);
				conn_set_user(c, user);
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_compreply));
				packet_set_type(rpacket, SERVER_COMPREPLY);
				bn_int_set(&rpacket->u.server_compreply.reg_version, SERVER_COMPREPLY_REG_VERSION);
				bn_int_set(&rpacket->u.server_compreply.reg_auth, SERVER_COMPREPLY_REG_AUTH);
				bn_int_set(&rpacket->u.server_compreply.client_id, SERVER_COMPREPLY_CLIENT_ID);
				bn_int_set(&rpacket->u.server_compreply.client_token, SERVER_COMPREPLY_CLIENT_TOKEN);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_sessionkey2));
				packet_set_type(rpacket, SERVER_SESSIONKEY2);
				bn_int_set(&rpacket->u.server_sessionkey2.sessionnum, conn_get_sessionnum(c));
				bn_int_set(&rpacket->u.server_sessionkey2.sessionkey, conn_get_sessionkey(c));
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_countryinfo1(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_countryinfo1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_countryinfo1), packet_get_size(packet));
				return -1;
			}
			{
				char const* langstr;
				char const* countrycode;
				char const* country;
				unsigned int tzbias;

				if (!(langstr = packet_get_str_const(packet, sizeof(t_client_countryinfo1), MAX_LANG_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long langstr)", conn_get_socket(c));
					return -1;
				}

				if (!(countrycode = packet_get_str_const(packet, sizeof(t_client_countryinfo1) + std::strlen(langstr) + 1, MAX_COUNTRYCODE_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long countrycode)", conn_get_socket(c));
					return -1;
				}

				if (!(country = packet_get_str_const(packet, sizeof(t_client_countryinfo1) + std::strlen(langstr) + 1 + std::strlen(countrycode) + 1, MAX_COUNTRY_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long country)", conn_get_socket(c));
					return -1;
				}

				if (!(packet_get_str_const(packet, sizeof(t_client_countryinfo1) + std::strlen(langstr) + 1 + std::strlen(countrycode) + 1 + std::strlen(country) + 1, MAX_COUNTRYNAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long countryname)", conn_get_socket(c));
					return -1;
				}

				tzbias = bn_int_get(packet->u.client_countryinfo1.bias);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] COUNTRYINFO1 packet from tzbias=0x{:04x} langstr={} countrycode={} country={}", conn_get_socket(c), tzbias, langstr, countrycode, country);
				conn_set_country(c, country);
				conn_set_tzbias(c, uint32_to_int(tzbias));
			}
			return 0;
		}

		static int _client_auth_info(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_auth_info)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_auth_info), packet_get_size(packet));
				return -1;
			}

			{
				char const* langstr;
				char const* countryname;
				unsigned int tzbias;
				char archtag_str[5];
				char clienttag_str[5];
				char gamelang_str[5];

				if (!(langstr = packet_get_str_const(packet, sizeof(t_client_auth_info), MAX_LANG_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (missing or too long langstr)", conn_get_socket(c));
					return -1;
				}

				if (!(countryname = packet_get_str_const(packet, sizeof(t_client_auth_info) + std::strlen(langstr) + 1, MAX_COUNTRYNAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (missing or too long countryname)", conn_get_socket(c));
					return -1;
				}

				/* check if it's an allowed client type */
				if (tag_check_in_list(bn_int_get(packet->u.client_auth_info.clienttag), prefs_get_allowed_clients())) {
					conn_set_state(c, conn_state_destroy);
					return 0;
				}

				tzbias = bn_int_get(packet->u.client_auth_info.bias);

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] AUTH_INFO packet {{ protocol={:#02x}, platform={}, product={}, versionid={:#02x}, language={}, localip={:#04x}, tzbias={:04x}, locale={}, language={}, country={}.{} }}",
					conn_get_socket(c),
					bn_int_get(packet->u.client_auth_info.protocol),
					tag_uint_to_str(archtag_str, bn_int_get(packet->u.client_auth_info.archtag)),
					tag_uint_to_str(clienttag_str, bn_int_get(packet->u.client_auth_info.clienttag)),
					bn_int_get(packet->u.client_auth_info.versionid),
					tag_uint_to_str(gamelang_str, bn_int_get(packet->u.client_auth_info.gamelang)),
					bn_int_get(packet->u.client_auth_info.localip),
					tzbias,
					bn_int_get(packet->u.client_auth_info.lcid),
					bn_int_get(packet->u.client_auth_info.langid),
					langstr,
					countryname);

				conn_set_country(c, langstr);	/* FIXME: This isn't right.  We want USA not ENU (English-US) */
				conn_set_tzbias(c, uint32_to_int(tzbias));
				conn_set_versionid(c, bn_int_get(packet->u.client_auth_info.versionid));
				conn_set_archtag(c, bn_int_get(packet->u.client_auth_info.archtag));
				conn_set_clienttag(c, bn_int_get(packet->u.client_auth_info.clienttag));
				conn_set_gamelang(c, bn_int_get(packet->u.client_auth_info.gamelang));

				/* First, send an ECHO_REQ */

				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_echoreq));
					packet_set_type(rpacket, SERVER_ECHOREQ);
					bn_int_set(&rpacket->u.server_echoreq.ticks, get_ticks());
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}

				if ((rpacket = packet_create(packet_class_bnet)))
				{
					packet_set_size(rpacket, sizeof(t_server_authreq_109));
					packet_set_type(rpacket, SERVER_AUTHREQ_109);

					// Logon type
					if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT))
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE_W3);
					else if ((conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE_W3XP);
					else
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE);


					// Session
					bn_int_set(&rpacket->u.server_authreq_109.sessionkey, conn_get_sessionkey(c));
					bn_int_set(&rpacket->u.server_authreq_109.sessionnum, conn_get_sessionnum(c));


					// CheckRevision
					std::tuple<std::string, std::string> checkrevision = select_checkrevision(bn_int_get(packet->u.client_auth_info.archtag), bn_int_get(packet->u.client_auth_info.clienttag), bn_int_get(packet->u.client_auth_info.versionid));
					file_to_mod_time(c, std::get<0>(checkrevision).c_str(), &rpacket->u.server_authreq_109.timestamp); // Checkrevision file timestamp
					packet_append_string(rpacket, std::get<0>(checkrevision).c_str()); // CheckRevision filename
					packet_append_string(rpacket, std::get<1>(checkrevision).c_str()); // CheckRevision equation
					eventlog(eventlog_level_debug, __FUNCTION__, "[{}] selected \"{}\" \"{}\"", conn_get_socket(c), std::get<0>(checkrevision), std::get<1>(checkrevision));


					// WarCraft 3 Server Signature
					if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT)
						|| (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
					{
						char padding[128];
						std::memset(padding, 0, 128);
						packet_append_data(rpacket, padding, 128);
					}
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
			return 0;
		}

		static int _client_unknown2b(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_unknown_2b)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_2B packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_2b), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		static int _client_progident(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_progident)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROGIDENT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_progident), packet_get_size(packet));
				return -1;
			}

			if (tag_check_in_list(bn_int_get(packet->u.client_progident.clienttag), prefs_get_allowed_clients())) {
				conn_set_state(c, conn_state_destroy);
				return 0;
			}

			conn_set_archtag(c, bn_int_get(packet->u.client_progident.archtag));
			conn_set_clienttag(c, bn_int_get(packet->u.client_progident.clienttag));
			conn_set_versionid(c, bn_int_get(packet->u.client_progident.versionid));

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}]CLIENT_PROGIDENTarchtag=0x{:08x}clienttag=0x{:08x}versionid=0x{:08x}unknown1=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_progident.archtag), bn_int_get(packet->u.client_progident.clienttag), bn_int_get(packet->u.client_progident.versionid), bn_int_get(packet->u.client_progident.unknown1));

			if ((rpacket = packet_create(packet_class_bnet)))
			{
				packet_set_size(rpacket, sizeof(t_server_authreq1));
				packet_set_type(rpacket, SERVER_AUTHREQ1);


				// CheckRevision
				std::tuple<std::string, std::string> checkrevision = select_checkrevision(bn_int_get(packet->u.client_progident.archtag), bn_int_get(packet->u.client_progident.clienttag), bn_int_get(packet->u.client_progident.versionid));
				file_to_mod_time(c, std::get<0>(checkrevision).c_str(), &rpacket->u.server_authreq1.timestamp); // Checkrevision file timestamp
				packet_append_string(rpacket, std::get<0>(checkrevision).c_str()); // CheckRevision filename
				packet_append_string(rpacket, std::get<1>(checkrevision).c_str()); // CheckRevision equation
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] selected \"{}\" \"{}\"", conn_get_socket(c), std::get<0>(checkrevision), std::get<1>(checkrevision));


				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_createaccountw3(t_connection* c, t_packet const* const packet)
		{
			char const* username;
			char const* plainpass;
			//			t_hash sc_hash;
			unsigned int i;
			const char* account_salt;
			const char* account_verifier;
			t_account* account;
			bool presume_plainpass = true;

			if (packet_get_size(packet) < sizeof(t_client_createaccount_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createaccount_w3), packet_get_size(packet));
				return -1;
			}

			username = packet_get_str_const(packet, sizeof(t_client_createaccount_w3), UNCHECKED_NAME_STR);
			if (!username) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			/* PELISH: We are testing if username missing and if packetsize is good
					   so we does not need to test if salt and verifier are present */
			account_salt = (const char*)packet_get_data_const(packet, offsetof(t_client_createaccount_w3, salt), 32);
			account_verifier = (const char*)packet_get_data_const(packet, offsetof(t_client_createaccount_w3, password_verifier), 32);

			for (i = 0; i < 16; i++) {
				int value = (unsigned char)account_verifier[i];
				if (value == 0)
					break;
				presume_plainpass &= (std::isprint(value) > 0);
			}

			for (i = 16; (i < 32) && presume_plainpass; i++) {
				int value = account_verifier[i];
				presume_plainpass &= (value == 0);
			}

			if (presume_plainpass)
				eventlog(eventlog_level_debug, __FUNCTION__, "This looks more like a plaintext password than like a verifier");

			plainpass = packet_get_str_const(packet, offsetof(t_client_createaccount_w3, password_verifier), 256);
			if (presume_plainpass && !plainpass) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCOUNT_W3 (missing password)", conn_get_socket(c));
				return -1;
			}

			t_packet* const rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_createaccount_w3));
			packet_set_type(rpacket, SERVER_CREATEACCOUNT_W3);

			//eventlog( eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\" , pass: {}", conn_get_socket( c ), username, plainpass );
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\" ", conn_get_socket(c), username);
			if (prefs_get_allow_new_accounts() == 0)
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_EXIST);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}

			if (account_check_name(username) < 0)
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (invalid symbols)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_INVALID);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				return 0;
			}

			//char lpass[ 256 ] = {};
			//if ( plainpass )
			//{
			//	/* convert plaintext password to lowercase for sc etc. */
			//	std::snprintf( lpass, sizeof lpass, "%s", plainpass );
			//	//strtolower( lpass );
			//}

			//set password hash for sc etc.
			//bnet_hash( &sc_hash, std::strlen( lpass ), lpass );
			plainpass = conn_get_plainpassword(c);
			char salt[BCRYPT_HASHSIZE];
			char hash[BCRYPT_HASHSIZE];
			bcrypt_gensalt(10, salt);
			bcrypt_hashpw(plainpass, salt, hash);

			if (!(account = accountlist_create_account(username, hash)))
			{
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_EXIST);
			}
			else
			{
				if (presume_plainpass)
				{
					BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
					BnetSRP3 srp3 = BnetSRP3(username, plainpass);
					srp3.setSalt(salt);
					BigInt verifier = srp3.getVerifier();
					account_verifier = (const char*)verifier.getData(32, 4, false);
				}
				else {
					//NEED TO TAG ACCOUNT AS "SC PASS BROKEN"
				}
				account_set_salt(account, account_salt);
				account_set_verifier(account, account_verifier);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCOUNT_W3_RESULT_OK);
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_createacctreq1(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			char const* username;
			t_hash newpasshash1;

			if (packet_get_size(packet) < sizeof(t_client_createacctreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createacctreq1), packet_get_size(packet));
				return -1;
			}

			if (!(username = packet_get_str_const(packet, sizeof(t_client_createacctreq1), UNCHECKED_NAME_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ1 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\"", conn_get_socket(c), username);

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;
			packet_set_size(rpacket, sizeof(t_server_createacctreply1));
			packet_set_type(rpacket, SERVER_CREATEACCTREPLY1);

			if (prefs_get_allow_new_accounts() == 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_NO);
				goto out;
			}

			bnhash_to_hash(packet->u.client_createacctreq1.password_hash1, &newpasshash1);
			if (!accountlist_create_account(username, hash_get_str(newpasshash1))) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (failed)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_NO);
				goto out;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
			bn_int_set(&rpacket->u.server_createacctreply1.result, SERVER_CREATEACCTREPLY1_RESULT_OK);

		out:
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_createacctreq2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			char const* username;
			t_hash newpasshash1;

			if (packet_get_size(packet) < sizeof(t_client_createacctreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CREATEACCTREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_createacctreq2), packet_get_size(packet));
				return -1;
			}

			username = packet_get_str_const(packet, sizeof(t_client_createacctreq2), UNCHECKED_NAME_STR);
			if (!username) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CREATEACCTREQ2 (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] new account requested for \"{}\"", conn_get_socket(c), username);

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;
			packet_set_size(rpacket, sizeof(t_server_createacctreply2));
			packet_set_type(rpacket, SERVER_CREATEACCTREPLY2);

			if (prefs_get_allow_new_accounts() == 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (disabled)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_EXIST);
				goto out;
			}

			if (account_check_name(username) < 0) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (invalid symbols)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createaccount_w3.result, SERVER_CREATEACCTREPLY2_RESULT_INVALID);
				goto out;
			}

			bnhash_to_hash(packet->u.client_createacctreq2.password_hash1, &newpasshash1);
			if (!accountlist_create_account(username, hash_get_str(newpasshash1))) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account not created (failed)", conn_get_socket(c));
				bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_EXIST);	/* FIXME: return reason for failure */
				goto out;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account created", conn_get_socket(c));
			bn_int_set(&rpacket->u.server_createacctreply2.result, SERVER_CREATEACCTREPLY2_RESULT_OK);

		out:
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_changepassreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_changepassreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CHANGEPASSREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changepassreq), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_changepassreq), UNCHECKED_NAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CHANGEPASSREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change requested for \"{}\"", conn_get_socket(c), username);

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_changepassack));
				packet_set_type(rpacket, SERVER_CHANGEPASSACK);

				/* fail if logged in or no account */
				if (connlist_find_connection_by_accountname(username) || !(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
				}
				else if (account_get_auth_changepass(account) == 0) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (no change access)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
				}
				else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_changepassreq.sessionkey)) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] password change for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_changepassreq.sessionkey));
					bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
				}
				else {
					struct {
						bn_int ticks;
						bn_int sessionkey;
						bn_int passhash1[5];
					} temp;
					char const* oldstrhash1;
					t_hash oldpasshash1;
					t_hash oldpasshash2;
					t_hash trypasshash2;
					t_hash newpasshash1;


					if ((oldstrhash1 = account_get_pass(account))) {
						bn_int_set(&temp.ticks, bn_int_get(packet->u.client_changepassreq.ticks));
						bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_changepassreq.sessionkey));
						if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
							bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
							account_set_pass(account, hash_get_str(newpasshash1));
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (bad previous password)", conn_get_socket(c), account_get_name(account));
							bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
						}
						else {
							hash_to_bnhash((t_hash const*)&oldpasshash1, temp.passhash1);	/* avoid warning */
							bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
							bnhash_to_hash(packet->u.client_changepassreq.oldpassword_hash2, &trypasshash2);

							if (hash_eq(trypasshash2, oldpasshash2) == 1) {
								bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
								account_set_pass(account, hash_get_str(newpasshash1));
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (previous password)", conn_get_socket(c), account_get_name(account));
								bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
							}
							else {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" refused (wrong password)", conn_get_socket(c), account_get_name(account));
								conn_increment_passfail_count(c);
								bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_FAIL);
							}
						}
					}
					else {
						bnhash_to_hash(packet->u.client_changepassreq.newpassword_hash1, &newpasshash1);
						account_set_pass(account, hash_get_str(newpasshash1));
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] password change for \"{}\" successful (no previous password)", conn_get_socket(c), account_get_name(account));
						bn_int_set(&rpacket->u.server_changepassack.message, SERVER_CHANGEPASSACK_MESSAGE_SUCCESS);
					}
				}

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		static int _client_echoreply(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_echoreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ECHOREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_echoreply), packet_get_size(packet));
				return -1;
			}

			{
				unsigned int now;
				unsigned int then;

				now = get_ticks();
				then = bn_int_get(packet->u.client_echoreply.ticks);
				if (!now || !then || now < then)
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] bad timing in echo reply: now={} then={}", conn_get_socket(c), now, then);
				else
					conn_set_latency(c, now - then);
			}

			return 0;
		}

		static int _client_authreq1(t_connection* c, t_packet const* const packet)
		{
			eventlog(eventlog_level_trace, __FUNCTION__, "[{}] received AUTHREQ1(0x07) packet", conn_get_socket(c));

			if (packet_get_size(packet) < sizeof(t_client_authreq1))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq1), packet_get_size(packet));
				return -1;
			}

			auto send_failed_packet = [](t_connection* c)
				{
					t_packet* rpacket = packet_create(packet_class_bnet);
					if (rpacket)
					{
						packet_set_size(rpacket, sizeof(t_server_authreply1));
						packet_set_type(rpacket, SERVER_AUTHREPLY1);
						conn_set_state(c, conn_state_untrusted);

						bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_BADVERSION);
						packet_append_string(rpacket, "");
						packet_append_string(rpacket, ""); // undocumented extra null terminator

						conn_push_outqueue(c, rpacket);

						packet_del_ref(rpacket);
					}
				};

			// The following if statements are sanity checks
			// The client should have already sent this information in a previous packet and is resending it again in this packet
			if (bn_int_get(packet->u.client_authreq1.archtag) != conn_get_archtag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch architecture)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.clienttag) != conn_get_clienttag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch client tag)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.versionid) != conn_get_versionid(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch version ID)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}


			const char* exeinfo = packet_get_str_const(packet, sizeof(t_client_authreq1), MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			std::string version_string = vernum_to_verstr(bn_int_get(packet->u.client_authreq1.gameversion));
			conn_set_clientver(c, version_string.c_str());

			conn_set_versionid(c, bn_int_get(packet->u.client_authreq1.versionid));
			conn_set_checksum(c, bn_int_get(packet->u.client_authreq1.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq1.gameversion));

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ1 archtag=0x{:08x} clienttag=0x{:08x} verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq1.archtag), bn_int_get(packet->u.client_authreq1.clienttag), version_string, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
			if (vc)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), vc->get_version_tag());
			}
			else
			{
				if (prefs_get_allow_unknown_version())
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
					send_failed_packet(c);
					return 0;
				}
			}


			t_packet* rpacket = packet_create(packet_class_bnet);
			if (rpacket)
			{
				packet_set_size(rpacket, sizeof(t_server_authreply1));
				packet_set_type(rpacket, SERVER_AUTHREPLY1);

				char* mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), vc->get_version_tag().c_str(), nullptr);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for version {} is available \"{}\"", conn_get_socket(c), vc->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);

					xfree(static_cast<void*>(mpqfilename));
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_OK);
				packet_append_string(rpacket, "");
				packet_append_string(rpacket, ""); // FIXME: what's the second string for?

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_authreq109(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_authreq_109))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq_109), packet_get_size(packet));
				return 0;
			}

			auto send_failed_packet = [](t_connection* c)
				{
					t_packet* rpacket = packet_create(packet_class_bnet);
					if (rpacket)
					{
						packet_set_size(rpacket, sizeof(t_server_authreply_109));
						packet_set_type(rpacket, SERVER_AUTHREPLY_109);
						conn_set_state(c, conn_state_untrusted);

						bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_BADVERSION);
						packet_append_string(rpacket, "");

						conn_push_outqueue(c, rpacket);

						packet_del_ref(rpacket);
					}
				};


			std::uint32_t count = bn_int_get(packet->u.client_authreq_109.cdkey_number);
			std::size_t position = sizeof(t_client_authreq_109) + (count * sizeof(t_cdkey_info));

			const char* const exeinfo = packet_get_str_const(packet, position, MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			position += std::strlen(exeinfo) + 1;

			const char* const owner = packet_get_str_const(packet, position, MAX_OWNER_STR);
			if (owner)
			{
				conn_set_owner(c, owner);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long owner)", conn_get_socket(c));
				conn_set_owner(c, "");
			}

			conn_set_checksum(c, bn_int_get(packet->u.client_authreq_109.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq_109.gameversion));
			std::string version = vernum_to_verstr(bn_int_get(packet->u.client_authreq_109.gameversion));
			conn_set_clientver(c, version.c_str());

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ_109 ticks=0x{:08x}, verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq_109.ticks), version, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			t_packet* rpacket;
			if ((rpacket = packet_create(packet_class_bnet)))
			{
				packet_set_size(rpacket, sizeof(t_server_authreply_109));
				packet_set_type(rpacket, SERVER_AUTHREPLY_109);


				const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
				if (vc)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), vc->get_version_tag());
				}
				else
				{
					if (prefs_get_allow_unknown_version())
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
					}
					else
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
						packet_del_ref(rpacket);
						send_failed_packet(c);
						return 0;
					}
				}


				char* mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), vc->get_version_tag().c_str(), NULL);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for {} is available \"{}\"", conn_get_socket(c), vc->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);

					xfree(static_cast<void*>(mpqfilename));
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_OK);
				packet_append_string(rpacket, "");

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);


				if (!conn_is_infos_okay(c))
				{
					conn_send_all_infos(c);
				}
				else
				{

				}
			}

			return 0;
		}

		static int _client_regsnoopreply(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_regsnoopreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REGSNOOPREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_regsnoopreply), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		static int _client_iconreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_iconreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ICONREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_iconreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_iconreply));
				packet_set_type(rpacket, SERVER_ICONREPLY);
				file_to_mod_time(c, prefs_get_iconfile(), &rpacket->u.server_iconreply.timestamp);

				/* battle.net sends different file on iconreq for WAR3 and W3XP [Omega] */
				if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT) || (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
					packet_append_string(rpacket, prefs_get_war3_iconfile());
				/* battle.net still sends "icons.bni" to sc/bw clients
				 * clients request icons_STAR.bni separately */
				 /*	else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_STARCRAFT)==0)
						 packet_append_string(rpacket,prefs_get_star_iconfile());
						 else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_BROODWARS)==0)
						 packet_append_string(rpacket,prefs_get_star_iconfile());
						 */
				else
					packet_append_string(rpacket, prefs_get_iconfile());

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_cdkey(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey), packet_get_size(packet));
				return -1;
			}

			{
				char const* cdkey;
				char const* owner;

				if (!(cdkey = packet_get_str_const(packet, sizeof(t_client_cdkey), MAX_CDKEY_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long cdkey)", conn_get_socket(c));
					return -1;
				}
				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey) + std::strlen(cdkey) + 1, MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_cdkey(c, cdkey);
				conn_set_owner(c, owner);

				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply));
					packet_set_type(rpacket, SERVER_CDKEYREPLY);
					bn_int_set(&rpacket->u.server_cdkeyreply.message, SERVER_CDKEYREPLY_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
#if 0				/* Blizzard used this to track down pirates, should only be accepted by old clients */
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_regsnoopreq));
				packet_set_type(rpacket, SERVER_REGSNOOPREQ);
				bn_int_set(&rpacket->u.server_regsnoopreq.unknown1, SERVER_REGSNOOPREQ_UNKNOWN1);	/* sequence num */
				bn_int_set(&rpacket->u.server_regsnoopreq.hkey, SERVER_REGSNOOPREQ_HKEY_CURRENT_USER);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGKEY);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGVALNAME);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
#endif
			return 0;
		}

		static int _client_cdkey2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const* owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey2), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply2));
					packet_set_type(rpacket, SERVER_CDKEYREPLY2);
					bn_int_set(&rpacket->u.server_cdkeyreply2.message, SERVER_CDKEYREPLY2_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_cdkey3(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const* owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey3), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply3));
					packet_set_type(rpacket, SERVER_CDKEYREPLY3);
					bn_int_set(&rpacket->u.server_cdkeyreply3.message, SERVER_CDKEYREPLY3_MESSAGE_OK);
					packet_append_string(rpacket, "");	/* FIXME: owner, message, ??? */
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_udpok(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_udpok)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UDPOK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_udpok), packet_get_size(packet));
				return -1;
			}
			/* we could check the contents but there really isn't any point */
			conn_set_udpok(c);

			return 0;
		}

		static int _client_fileinforeq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_fileinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FILEINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_fileinforeq), packet_get_size(packet));
				return -1;
			}

			{
				char const* filename;

				if (!(filename = packet_get_str_const(packet, sizeof(t_client_fileinforeq), MAX_FILENAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FILEINFOREQ packet (missing or too long tosfile)", conn_get_socket(c));
					return -1;
				}
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] file requested: \"{}\" - type = 0x{:02x}", conn_get_socket(c), filename, bn_int_get(packet->u.client_fileinforeq.type));

				/* TODO: if type is TOSFILE make bnetd to send default tosfile if selected is not found */
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_fileinforeply));
					packet_set_type(rpacket, SERVER_FILEINFOREPLY);
					bn_int_set(&rpacket->u.server_fileinforeply.type, bn_int_get(packet->u.client_fileinforeq.type));
					bn_int_set(&rpacket->u.server_fileinforeply.unknown2, bn_int_get(packet->u.client_fileinforeq.unknown2));
					/* Note from Sherpya:
					 * timestamp -> 0x852b7d00 - 0x01c0e863 b.net send this (bn_int),
					 * I suppose is not a long
					 * if bnserver-D2DV is bad diablo 2 crashes
					 * timestamp doesn't work correctly and starcraft
					 * needs name in client locale or displays hostname
					 */
					file_to_mod_time(c, filename, &rpacket->u.server_fileinforeply.timestamp);
					packet_append_string(rpacket, filename);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static const char* _attribute_req(t_account* reqacc, t_account* myacc, const char* key)
		{
			const char* result = "";
			const char* tval;

			if (!reqacc)
				goto out;
			if (reqacc != myacc && !strncasecmp(key, "BNET", 4))
				goto out;

			tval = account_get_strattr(reqacc, key);
			if (tval)
				result = tval;

		out:
			return result;
		}

		static int _client_statsreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			char const* name;
			char const* key;
			unsigned int name_count;
			unsigned int key_count;
			unsigned int i, j;
			unsigned int name_off;
			unsigned int keys_off;
			unsigned int key_off;
			t_account* reqacc, * myacc;

			if (packet_get_size(packet) < sizeof(t_client_statsreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_statsreq), packet_get_size(packet));
				return -1;
			}

			name_count = bn_int_get(packet->u.client_statsreq.name_count);
			key_count = bn_int_get(packet->u.client_statsreq.key_count);

			for (i = 0, name_off = sizeof(t_client_statsreq); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1);

			if (i < name_count) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSREQ packet (only {} names of {})", conn_get_socket(c), i, name_count);
				return -1;
			}
			keys_off = name_off;

			if (!(rpacket = packet_create(packet_class_bnet)))
				return -1;

			packet_set_size(rpacket, sizeof(t_server_statsreply));
			packet_set_type(rpacket, SERVER_STATSREPLY);
			bn_int_set(&rpacket->u.server_statsreply.name_count, name_count);
			bn_int_set(&rpacket->u.server_statsreply.key_count, key_count);
			bn_int_set(&rpacket->u.server_statsreply.requestid, bn_int_get(packet->u.client_statsreq.requestid));

			myacc = conn_get_account(c);

			for (i = 0, name_off = sizeof(t_client_statsreq); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1) {
				reqacc = accountlist_find_account(name);
				if (!reqacc)
					reqacc = myacc;

				for (j = 0, key_off = keys_off; j < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)); j++, key_off += std::strlen(key) + 1) {
					if (*key == '\0')
						continue;
					packet_append_string(rpacket, _attribute_req(reqacc, myacc, key));
				}
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_loginreq1(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_loginreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq1), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq1), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ1 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				t_packet* const rpacket = packet_create(packet_class_bnet);
				if (!rpacket)
				{
					return -1;
				}

				packet_set_size(rpacket, sizeof(t_server_loginreply1));
				packet_set_type(rpacket, SERVER_LOGINREPLY1);

				// too many logins? [added by NonReal]
				if (prefs_get_max_concurrent_logins() > 0)
				{
					if (prefs_get_max_concurrent_logins() <= connlist_login_get_length())
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_get_max_concurrent_logins(), connlist_login_get_length());
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
						return -1;
					}
				}

				/* fail if no account */
				if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
				}
				else
					/* already logged in */
					if (connlist_find_connection_by_account(account) && !IsHostBot(username) && !account_get_auth_botlogin(account) && prefs_get_kick_old_login() == 0) {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
					}
					else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
					}
					else if (account_get_auth_lock(account) == 1) {	/* default to false */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
					}
					else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_loginreq1.sessionkey)) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_loginreq1.sessionkey));
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
					}
					else {
						struct {
							bn_int ticks;
							bn_int sessionkey;
							bn_int passhash1[5];
						} temp;
						char const* oldstrhash1;
						t_hash oldpasshash1;
						t_hash oldpasshash2;
						t_hash trypasshash2;

						if ((oldstrhash1 = account_get_pass(account))) {
							bn_int_set(&temp.ticks, bn_int_get(packet->u.client_loginreq1.ticks));
							bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_loginreq1.sessionkey));
							if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (corrupted passhash1?)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
							}
							else {
								hash_to_bnhash((t_hash const*)&oldpasshash1, temp.passhash1);	/* avoid warning */

								bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
								bnhash_to_hash(packet->u.client_loginreq1.password_hash2, &trypasshash2);

								if (hash_eq(trypasshash2, oldpasshash2) == 1) {
									conn_login(c, account, username);
									eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (correct password)", conn_get_socket(c), username);
									bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
#ifdef WITH_LUA
									if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
									{
										// feature to break login from Lua
										conn_set_state(c, conn_state_destroy);
										return -1;
									}
#endif
#ifdef WIN32_GUI
									guiOnUpdateUserList();
#endif
								}
								else {
									eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (wrong password)", conn_get_socket(c), username);
									conn_increment_passfail_count(c);
									bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_FAIL);
								}
							}
						}
						else {
							conn_login(c, account, username);
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (no password)", conn_get_socket(c), username);
							bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY1_MESSAGE_SUCCESS);
#ifdef WIN32_GUI
							guiOnUpdateUserList();
#endif
						}
					}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		void client_init_email(t_connection* c, t_account* account)
		{
			t_packet* packet;
			char const* email;

			if (!c || !account)
				return;
			if (!(email = account_get_email(account))) {
				if ((packet = packet_create(packet_class_bnet))) {
					packet_set_size(packet, sizeof(t_server_setemailreq));
					packet_set_type(packet, SERVER_SETEMAILREQ);
					conn_push_outqueue(c, packet);
					packet_del_ref(packet);
				}
			}
			return;
		}

		static int _client_loginreq2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			int success = 0;

			if (packet_get_size(packet) < sizeof(t_client_loginreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq2), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;
				char supports_locked_reply = 0;
				t_clienttag clienttag = conn_get_clienttag(c);

				if (clienttag == CLIENTTAG_STARCRAFT_UINT || clienttag == CLIENTTAG_BROODWARS_UINT || clienttag == CLIENTTAG_SHAREWARE_UINT ||
					clienttag == CLIENTTAG_DIABLORTL_UINT || clienttag == CLIENTTAG_DIABLOSHR_UINT || clienttag == CLIENTTAG_WARCIIBNE_UINT ||
					clienttag == CLIENTTAG_DIABLO2DV_UINT || clienttag == CLIENTTAG_STARJAPAN_UINT || clienttag == CLIENTTAG_DIABLO2ST_UINT ||
					clienttag == CLIENTTAG_DIABLO2XP_UINT || clienttag == CLIENTTAG_WARCRAFT3_UINT || clienttag == CLIENTTAG_WAR3XP_UINT)
				{
					if (conn_get_versionid(c) >= 0x0000000b)
						supports_locked_reply = 1;
				}

				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq2), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGINREQ2 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_loginreply2));
				packet_set_type(rpacket, SERVER_LOGINREPLY2);

				// too many logins? [added by NonReal]
				if (prefs_get_max_concurrent_logins() > 0) {
					if (prefs_get_max_concurrent_logins() <= connlist_login_get_length()) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_get_max_concurrent_logins(), connlist_login_get_length());
						if (supports_locked_reply) {
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
							packet_append_string(rpacket, "Too many concurrent logins. Try again later.");
						}
						else {
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
						}

						packet_del_ref(rpacket);
						return -1;
					}
				}

				/* fail if no account */
				if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no such account)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_NONEXIST);
				}
				/* already logged in */
				else if (connlist_find_connection_by_account(account) && !IsHostBot(username) && !account_get_auth_botlogin(account) && prefs_get_kick_old_login() == 0) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
					if (supports_locked_reply) {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						packet_append_string(rpacket, "This account is already logged in.");
					}
					else {

						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
					if (supports_locked_reply) {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						packet_append_string(rpacket, "This account is barred from bnet access.");
					}
					else {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
					if (supports_locked_reply)
					{
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_LOCKED);
						std::string msgtemp = localize(c, "This account has been locked");
						msgtemp += account_get_locktext(c, account, true);
						packet_append_string(rpacket, msgtemp.c_str());
					}
					else {
						bn_int_set(&rpacket->u.server_loginreply1.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
					}
				}
				else if (conn_get_sessionkey(c) != bn_int_get(packet->u.client_loginreq2.sessionkey)) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] login for \"{}\" refused (expected session key 0x{:08x}, got 0x{:08x})", conn_get_socket(c), username, conn_get_sessionkey(c), bn_int_get(packet->u.client_loginreq2.sessionkey));
					bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
				}
				else {
					struct {
						bn_int ticks;
						bn_int sessionkey;
						bn_int passhash1[5];
					} temp;
					char const* oldstrhash1;
					t_hash oldpasshash1;
					t_hash oldpasshash2;
					t_hash trypasshash2;

					if ((oldstrhash1 = account_get_pass(account))) {
						bn_int_set(&temp.ticks, bn_int_get(packet->u.client_loginreq2.ticks));
						bn_int_set(&temp.sessionkey, bn_int_get(packet->u.client_loginreq2.sessionkey));
						if (hash_set_str(&oldpasshash1, oldstrhash1) < 0) {
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (corrupted passhash1?)", conn_get_socket(c), username);
							bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
						}
						else {
							hash_to_bnhash((t_hash const*)&oldpasshash1, temp.passhash1);	/* avoid warning */

							bnet_hash(&oldpasshash2, sizeof(temp), &temp);	/* do the double hash */
							bnhash_to_hash(packet->u.client_loginreq2.password_hash2, &trypasshash2);

							if (hash_eq(trypasshash2, oldpasshash2) == 1) {
								conn_login(c, account, username);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (correct password)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
								success = 1;
							}
							else {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (wrong password)", conn_get_socket(c), username);
								conn_increment_passfail_count(c);
								bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_BADPASS);
							}
						}
					}
					else {
						conn_login(c, account, username);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" logged in (no password)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_loginreply2.message, SERVER_LOGINREPLY2_MESSAGE_SUCCESS);
						success = 1;
					}
				}
				if (success && account) {

#ifdef WITH_LUA
					if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
					{
						// feature to break login from Lua
						conn_set_state(c, conn_state_destroy);
						packet_del_ref(rpacket);
						return -1;
					}
#endif

#ifdef WIN32_GUI
					guiOnUpdateUserList();
#endif
					client_init_email(c, account);
				}

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_loginreqw3(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_loginreq_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_LOGINREQ_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_loginreq_w3), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;
				char const* account_salt;
				char const* account_verifier;
				const char* conn_client_public_key;
				int i;

				/* PELISH: Does not need to check conn_client_public_key != NULL because we testing packet size */
				conn_client_public_key = (char*)packet_get_data_const(packet, offsetof(t_client_loginreq_w3, client_public_key), 32);


				if (!(username = packet_get_str_const(packet, sizeof(t_client_loginreq_w3), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_LOGINREQ_W3 (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_loginreply_w3));
				packet_set_type(rpacket, SERVER_LOGINREPLY_W3);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], 0);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.server_public_key[i], 0);

				{
					/* too many logins? */
					if (prefs_get_max_concurrent_logins() > 0 && prefs_get_max_concurrent_logins() <= connlist_login_get_length()) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] login denied, too many concurrent logins. max: {}. current: {}.", conn_get_socket(c), prefs_get_max_concurrent_logins(), connlist_login_get_length());
						bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
					}
					else
					{
						bool noaccount = false;
						if (!(account = accountlist_find_account(username))) {

							/* create defaul accounts with default password! *//*
							if (ToLower(username) == ToLower("HostBotAccount"))
							{
								char salt[BCRYPT_HASHSIZE];
								char hash[BCRYPT_HASHSIZE];
								bcrypt_gensalt(10, salt);
								bcrypt_hashpw("Xl7rlkirtiznmvB", salt, hash);
								accountlist_create_account(username, hash);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" create new bot account!", conn_get_socket(c), username);
							}
							else if (ToLower(username) == ToLower("AdminAccount"))
							{
								char salt[BCRYPT_HASHSIZE];
								char hash[BCRYPT_HASHSIZE];
								bcrypt_gensalt(10, salt);
								bcrypt_hashpw("JYik1geEwKeNwj6tTCpC", salt, hash);
								auto tmpacc = accountlist_create_account(username, hash);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" create new admin account!", conn_get_socket(c), username);
								account_set_admin(tmpacc);
								account_set_command_groups(tmpacc, 255);
							}
							else
							{*/
								noaccount = true;
							//}
						}
						if (noaccount)
						{
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (no such account)", conn_get_socket(c), username);
							bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
						}
						else
							/* already logged in */
							if (connlist_find_connection_by_account(account) && !IsHostBot(username) && !account_get_auth_botlogin(account) && prefs_get_kick_old_login() == 0) {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (already logged in)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_ALREADY);
							}
							else if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
								bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_BADACCT);
							}
							else if (!(account_salt = account_get_salt(account))) {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check (even though account has no salt)", conn_get_socket(c), username);
								conn_set_loggeduser(c, username);
								bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
							}
							else if (!(account_verifier = account_get_verifier(account))) {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check (even though account has no verifier)", conn_get_socket(c), username);
								conn_set_loggeduser(c, username);
								bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
								xfree((void*)account_salt);
							}
							else {

								for (i = 0; i < 32; i++) {
									bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], account_salt[i]);
								}

								BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
								BigInt verifier = BigInt((unsigned char*)account_verifier, 32, 1, false);
								BnetSRP3 srp3 = BnetSRP3(username, salt);

								BigInt client_public_key = BigInt((unsigned char*)conn_client_public_key, 32, 1, false);
								BigInt server_public_key = srp3.getServerSessionPublicKey(verifier);

								server_public_key.getData((unsigned char*)&rpacket->u.server_loginreply_w3.server_public_key, 32, 4, false);

								BigInt hashed_server_secret_ = srp3.getHashedServerSecret(client_public_key, verifier);
								BigInt client_proof = srp3.getClientPasswordProof(client_public_key, server_public_key, hashed_server_secret_);
								BigInt server_proof = srp3.getServerPasswordProof(client_public_key, client_proof, hashed_server_secret_);

								char* conn_client_proof = (char*)client_proof.getData(20, 4, false);
								char* conn_server_proof = (char*)server_proof.getData(20, 4, false);

								conn_set_client_proof(c, conn_client_proof);
								conn_set_server_proof(c, conn_server_proof);

								xfree((void*)account_verifier);
								xfree((void*)account_salt);
								xfree(conn_client_proof);
								xfree(conn_server_proof);

								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account check", conn_get_socket(c), username);
								conn_set_loggeduser(c, username);
								bn_int_set(&rpacket->u.server_loginreply_w3.message, SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS);
							}
					}
				}

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		static int _client_passchangereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_passchangereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_PASSCHANGEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_passchangereq), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;
				char const* account_salt;
				char const* account_verifier;
				const char* conn_client_public_key;
				int i;

				/* PELISH: Does not need to check conn_client_public_key != NULL because we testing packet size */
				conn_client_public_key = (char*)packet_get_data_const(packet, offsetof(t_client_loginreq_w3, client_public_key), 32);


				if (!(username = packet_get_str_const(packet, sizeof(t_client_passchangereq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_PASSCHANGEREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_passchangereply));
				packet_set_type(rpacket, SERVER_PASSCHANGEREPLY);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.salt[i], 0);

				for (i = 0; i < 32; i++)
					bn_byte_set(&rpacket->u.server_loginreply_w3.server_public_key[i], 0);

				{
					bn_int_set(&rpacket->u.server_passchangereply.message, SERVER_PASSCHANGEREPLY_MESSAGE_REJECT);

					/* fail if no account */
					if (!(account = accountlist_find_account(username))) {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no such account)", conn_get_socket(c), username);
					}
					else
						/* fail if bnetlogin is disallowed */
						if (account_get_auth_bnetlogin(account) == 0) {	/* default to true */
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no bnet access)", conn_get_socket(c), username);
						}
						else
							/* fail if no salt */
							if ((account_salt = account_get_salt(account)) == NULL) {
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no SALT)", conn_get_socket(c), username);
							}
							else
								/* fail if no verifier */
								if ((account_verifier = account_get_verifier(account)) == NULL) {
									eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange for \"{}\" refused (no VERIFIER)", conn_get_socket(c), username);
									xfree((void*)account_salt);
								}
								else {

									for (i = 0; i < 32; i++) {
										bn_byte_set(&rpacket->u.server_passchangereply.salt[i], account_salt[i]);
									}

									BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
									BigInt verifier = BigInt((unsigned char*)account_verifier, 32, 1, false);
									BnetSRP3 srp3 = BnetSRP3(username, salt);

									BigInt client_public_key = BigInt((unsigned char*)conn_client_public_key, 32, 1, false);
									BigInt server_public_key = srp3.getServerSessionPublicKey(verifier);

									server_public_key.getData((unsigned char*)&rpacket->u.server_loginreply_w3.server_public_key, 32, 4, false);

									BigInt hashed_server_secret_ = srp3.getHashedServerSecret(client_public_key, verifier);
									BigInt client_proof = srp3.getClientPasswordProof(client_public_key, server_public_key, hashed_server_secret_);
									BigInt server_proof = srp3.getServerPasswordProof(client_public_key, client_proof, hashed_server_secret_);

									char* conn_client_proof = (char*)client_proof.getData(20, 4, false);
									char* conn_server_proof = (char*)server_proof.getData(20, 4, false);

									conn_set_client_proof(c, conn_client_proof);
									conn_set_server_proof(c, conn_server_proof);

									xfree((void*)account_verifier);
									xfree((void*)account_salt);
									xfree(conn_client_proof);
									xfree(conn_server_proof);

									eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" passed account passchange check", conn_get_socket(c), username);
									conn_set_loggeduser(c, username);
									bn_int_set(&rpacket->u.server_passchangereply.message, SERVER_PASSCHANGEREPLY_MESSAGE_ACCEPT);
								}
				}

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		static int _client_passchangeproofreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_passchangeproofreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PASSCHANGEPROOFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_passchangeproofreq), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				t_account* account;
				const char* salt;
				const char* verifier;

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] passchange proof requested", conn_get_socket(c));

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_passchangeproofreply));
				packet_set_type(rpacket, SERVER_PASSCHANGEPROOFREPLY);

				std::memset(&rpacket->u.server_passchangeproofreply.server_password_proof, 0, sizeof(rpacket->u.server_passchangeproofreply.server_password_proof));

				bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_PASSCHANGEPROOFREPLY_RESPONSE_BADPASS);

				if (!(username = conn_get_loggeduser(c))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got NULL username, 0x56ff before 0x55ff?", conn_get_socket(c));
				}
				else if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) passchange in 0x56ff for \"{}\" refused (no such account)", conn_get_socket(c), username);
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] passchange for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
				}
				else {
					int i;
					const char* client_password_proof;

					if (!(client_password_proof = (const char*)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, client_password_proof), 20))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] (W3) got bad PASSCHANGEPROOFREQ packet (missing hash)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}

					if (std::memcmp(client_password_proof, conn_get_client_proof(c), 20) == 0) {
						const char* server_proof = conn_get_server_proof(c);

						for (i = 0; i < 20; i++) {
							bn_byte_set(&rpacket->u.server_passchangeproofreply.server_password_proof[i], server_proof[i]);
						}

						salt = (const char*)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, salt), 32);
						verifier = (const char*)packet_get_data_const(packet, offsetof(t_client_passchangeproofreq, password_verifier), 32);
						account_set_salt(account, salt);
						account_set_verifier(account, verifier);

						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" successful passchange (right client password proof)", conn_get_socket(c), username);
						bn_int_set(&rpacket->u.server_passchangeproofreply.response, SERVER_PASSCHANGEPROOFREPLY_RESPONSE_OK);
					}
					else {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got wrong client password proof for \"{}\"", conn_get_socket(c), username);
						conn_increment_passfail_count(c);
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				conn_set_client_proof(c, NULL);
				conn_set_server_proof(c, NULL);
			}

			return 0;
		}

		static int _client_pingreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_pingreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PINGREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_pingreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_pingreply));
				packet_set_type(rpacket, SERVER_PINGREPLY);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_logonproofreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_logonproofreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LOGONPROOFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_logonproofreq), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				char const* plainpassword = conn_get_plainpassword(c);
				t_account* account;

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] logon proof requested", conn_get_socket(c));

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_logonproofreply));
				packet_set_type(rpacket, SERVER_LOGONPROOFREPLY);

				std::memset(&rpacket->u.server_logonproofreply.server_password_proof, 0, sizeof(rpacket->u.server_logonproofreply.server_password_proof));

				bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_BADPASS);

				if (!(username = conn_get_loggeduser(c))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got NULL username, 0x54ff before 0x53ff?", conn_get_socket(c));
				}
				else if (!(account = accountlist_find_account(username))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) login in 0x54ff for \"{}\" refused (no such account)", conn_get_socket(c), username);
				}
				else if (account_get_auth_lock(account) == 1) {	/* default to false */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (this account is locked)", conn_get_socket(c), username);
					bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM);
					std::string msgtemp = localize(c, "This account has been locked");
					msgtemp += account_get_locktext(c, account, true);
					packet_append_string(rpacket, msgtemp.c_str());
				}
				else {
					char newhash[512];
					const char* serverhash = "";
					const char* clienthashnew = conn_get_hash(c);
					t_hash clienthash;

					//					int i;
					const char* client_password_proof;

					/* PELISH: This can not occur - We already tested packet size which must be wrong firstly.
					   Also pvpgn will crash when will dereferencing NULL pointer (so we can't got this errorlog message)
					I vote for deleting this "if" */
					if (!(client_password_proof = (const char*)packet_get_data_const(packet, offsetof(t_client_logonproofreq, client_password_proof), 20))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] (W3) got bad LOGONPROOFREQ packet (missing hash)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}



					clienthash[0] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[0]);
					clienthash[1] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[4]);
					clienthash[2] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[8]);
					clienthash[3] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[12]);
					clienthash[4] = bn_int_get(packet->u.client_logonproofreq.client_password_proof[16]);

					serverhash = account_get_pass(account);

					eventlog(eventlog_level_error, __FUNCTION__, "Username {} Password {} Hash1 {} Hash2 {}", username, "hidden", serverhash, clienthashnew);


					//here
					if (IsHostBot(username) || (conn_get_ah_status(c) == 1 && conn_get_cheats(c) == 0))
					{
						/*if ( conn_get_client_proof( c ) && std::memcmp( client_password_proof, conn_get_client_proof( c ), 20 ) == 0 )
						{
							const char * server_proof = conn_get_server_proof( c );

							for ( i = 0; i < 20; i++ ) {
								bn_byte_set( &rpacket->u.server_logonproofreply.server_password_proof[ i ], server_proof[ i ] );
							}

							conn_login( c, account, username );
							eventlog( eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" logged in (right client password proof)", conn_get_socket( c ), username );
							if ( ( conn_get_versionid( c ) >= 0x0000000D ) && ( account_get_email( account ) == NULL ) )
								bn_int_set( &rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_EMAIL );
							else
								bn_int_set( &rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_OK );
							// by amadeo updates the userlist
#ifdef WIN32_GUI
							guiOnUpdateUserList( );
#endif

							if ( !IsHostBot( username ) )
							{
								unsigned int * userhardwareid = conn_get_hardwareid( c );
								std::string temphwidstr = fmt::format( "{0:X}-{1:X}-{2:X}-{3:X}", userhardwareid[ 0 ], userhardwareid[ 1 ], userhardwareid[ 2 ], userhardwareid[ 3 ] );
								std::string oldacchwid = account_get_hwid( account );

								if ( IsInBlackList( temphwidstr.c_str( ) ) ||
									IsInBlackList( oldacchwid.c_str( ) ) )
								{
									bn_int_set( &rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM );
									std::string msgtemp;
									msgtemp = localize( c, "Sorry, but you added to blacklist!" );
									packet_append_string( rpacket, msgtemp.c_str( ) );
								}
								else
								{								//eventlog( eventlog_level_debug, __FUNCTION__, "Get hwid for \"{}\"", username );
									if ( !oldacchwid.empty( ) && oldacchwid.length( ) > 5 )
									{
										if ( oldacchwid != temphwidstr )
										{
											eventlog( eventlog_level_error, __FUNCTION__, "User \"{}\" change hwid from: {} to {}", username, oldacchwid.c_str( ), temphwidstr.c_str( ) );
											account_set_hwid( account, temphwidstr );
										}
									}
									else
									{
										eventlog( eventlog_level_error, __FUNCTION__, "User \"{}\" set hwid: {}", username, temphwidstr.c_str( ) );
										account_set_hwid( account, temphwidstr );
									}
								}
								//eventlog( eventlog_level_debug, __FUNCTION__, "End get hwid for \"{}\"", username );
							}
						}
						else */
						if (serverhash && serverhash[0] != '\0' && clienthash && clienthash[0] != '\0' && plainpassword && plainpassword[0] != '\0') {

							bcrypt_hashpw(plainpassword, serverhash, newhash);
							if (strcmp(serverhash, newhash) == 0)
							{
								conn_login(c, account, username);
								eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) \"{}\" logged in (right password)", conn_get_socket(c), username);
								//if ( ( conn_get_versionid( c ) >= 0x0000000D ) && ( account_get_email( account ) == NULL ) )
								//	bn_int_set( &rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_EMAIL );
								//else

								bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_OK);
							}
							else
							{
								bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_BADPASS);
							}
#ifdef WITH_LUA
							if (lua_handle_user(c, NULL, NULL, luaevent_user_login) == 1)
							{
								// feature to break login from Lua
								conn_set_state(c, conn_state_destroy);
								packet_del_ref(rpacket);
								return -1;
							}
#endif
							// by amadeo updates the userlist
#ifdef WIN32_GUI
							guiOnUpdateUserList();
#endif

							//here
							if (!IsHostBot(username))
							{
								unsigned int* userhardwareid = conn_get_hardwareid(c);
								std::string temphwidstr = fmt::format("{0:X}-{1:X}-{2:X}-{3:X}", userhardwareid[0], userhardwareid[1], userhardwareid[2], userhardwareid[3]);
								std::string oldacchwid = account_get_hwid(account);
								if (IsInBlackList(temphwidstr.c_str()) ||
									IsInBlackList(oldacchwid.c_str()))
								{
									bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM);
									std::string msgtemp;
									msgtemp = localize(c, "Sorry, but you added to blacklist!");
									msgtemp += account_get_locktext(c, account, true);
									packet_append_string(rpacket, msgtemp.c_str());
								}
								else
								{
									//eventlog( eventlog_level_debug, __FUNCTION__, "Get hwid for \"{}\"", username );
									if (!oldacchwid.empty() && oldacchwid.length() > 5)
									{
										if (oldacchwid != temphwidstr)
										{
											eventlog(eventlog_level_error, __FUNCTION__, "User \"{}\" change hwid from: {} to {}", username, oldacchwid.c_str(), temphwidstr.c_str());
											account_set_hwid(account, temphwidstr);
										}
									}
									else
									{
										eventlog(eventlog_level_error, __FUNCTION__, "User \"{}\" set hwid: {}", username, temphwidstr.c_str());
										account_set_hwid(account, temphwidstr);
									}
								}
								//eventlog( eventlog_level_debug, __FUNCTION__, "End get hwid for \"{}\"", username );
							}
						}
						else {
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] (W3) got wrong client password proof for \"{}\"", conn_get_socket(c), username);
							conn_increment_passfail_count(c);
						}
					}
					else
					{
						//here
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] login for \"{}\" refused (no antihack found) - ({},{},{}=={} ?)", conn_get_socket(c), username, conn_get_ah_status(c), conn_get_cheats(c), conn_get_ah_version(c), ANTIHACK_VERSION);
						bn_int_set(&rpacket->u.server_logonproofreply.response, SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM);
						std::string msgtemp = "Please use antihack client!";


						if (conn_get_ah_version(c) != ANTIHACK_VERSION)
						{
							msgtemp = "Please update antihack client!";
						}

						if (conn_get_cheats(c) > 0)
						{
							msgtemp = "Please remove hacks and restart! Code 1:";
							msgtemp += std::to_string(conn_get_cheats(c));
							msgtemp += ". Code 2: ";
							msgtemp += std::to_string(conn_get_ah_version(c) - ANTIHACK_VERSION);
						}

						if (conn_get_ah_status(c) == 3)
						{
							msgtemp = "Please update launcher to " +std::to_string(conn_get_launcher_version(c) - LAUNCHER_VERSION) +"!";
						}

						if (conn_get_ah_status(c) == 4)
						{
							msgtemp = "Please update AHM.dll possible corrupted by anime virus!";
						}

						if (conn_get_ah_status(c) == 5)
						{
							msgtemp = "Please update ANTIHACK version to " + std::to_string(conn_get_ah_version(c) - ANTIHACK_VERSION) + "!";
						}

						//msgtemp += account_get_locktext( c, account, true );
						packet_append_string(rpacket, msgtemp.c_str());
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
				conn_set_client_proof(c, NULL);
				conn_set_server_proof(c, NULL);
			}
			clan_send_status_window(c);

			return 0;
		}

		static int _client_changegameport(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_changegameport)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad changegameport packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changegameport), packet_get_size(packet));
				return -1;
			}
			{
				unsigned short port = bn_short_get(packet->u.client_changegameport.port);
				if (port < 1024) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid port in changegameport packet: {}", conn_get_socket(c), (int)port);
					return -1;
				}

				conn_set_game_port(c, port);
			}

			return 0;
		}

		static int _client_friendslistreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_friendslistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FRIENDSLISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_friendslistreq), packet_get_size(packet));
				return -1;
			}
			{
				int frienduid;
				t_list* flist;
				t_friend* fr;
				t_account* account = conn_get_account(c);
				int i;
				int n = account_get_friendcount(account);
				int friendcount = 0;
				t_server_friendslistreply_status status;
				t_connection* dest_c;
				t_game* game;
				t_channel* channel;
				char stat;

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;

				packet_set_size(rpacket, sizeof(t_server_friendslistreply));
				packet_set_type(rpacket, SERVER_FRIENDSLISTREPLY);

				if ((flist = account_get_friends(account)) == NULL)
				{
					packet_del_ref(rpacket);
					return -1;
				}

				for (i = 0; i < n; i++) {
					frienduid = account_get_friend(account, i);
					if ((fr = friendlist_find_uid(flist, frienduid)) == NULL)
						continue;
					packet_append_string(rpacket, account_get_name(friend_get_account(fr)));
					game = NULL;
					channel = NULL;

					if (!(dest_c = connlist_find_connection_by_uid(frienduid))) {
						bn_byte_set(&status.location, FRIENDSTATUS_OFFLINE);
						bn_byte_set(&status.status, 0);
						bn_int_set(&status.clienttag, 0);
					}
					else {
						bn_int_set(&status.clienttag, conn_get_clienttag(dest_c));
						stat = 0;
						if ((friend_get_mutual(fr)))
							stat |= FRIEND_TYPE_MUTUAL;
						if ((conn_get_dndstr(dest_c)))
							stat |= FRIEND_TYPE_DND;
						if ((conn_get_awaystr(dest_c)))
							stat |= FRIEND_TYPE_AWAY;
						bn_byte_set(&status.status, stat);
						if ((game = conn_get_game(dest_c))) {
							if (game_get_flag(game) != game_flag_private)
								bn_byte_set(&status.location, FRIENDSTATUS_PUBLIC_GAME);
							else
								bn_byte_set(&status.location, FRIENDSTATUS_PRIVATE_GAME);
						}
						else if ((channel = conn_get_channel(dest_c))) {
							bn_byte_set(&status.location, FRIENDSTATUS_CHAT);
						}
						else {
							bn_byte_set(&status.location, FRIENDSTATUS_ONLINE);
						}
					}

					packet_append_data(rpacket, &status, sizeof(status));

					if (game)
						packet_append_string(rpacket, game_get_name(game));
					else if (channel)
						packet_append_string(rpacket, channel_get_name(channel));
					else
						packet_append_string(rpacket, "");

					friendcount++;
				}

				bn_byte_set(&rpacket->u.server_friendslistreply.friendcount, friendcount);

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_friendinforeq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_friendinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FRIENDINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_friendinforeq), packet_get_size(packet));
				return -1;
			}

			{
				t_connection const* dest_c;
				t_game const* game;
				t_channel const* channel;
				t_account* account = conn_get_account(c);
				int frienduid;
				t_friend* fr;
				t_list* flist;
				int n = account_get_friendcount(account);
				char type;

				if (n == 0)
					return 0;

				if (bn_byte_get(packet->u.client_friendinforeq.friendnum) > n) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] bad friend number in FRIENDINFOREQ packet", conn_get_socket(c));
					return -1;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;

				packet_set_size(rpacket, sizeof(t_server_friendinforeply));
				packet_set_type(rpacket, SERVER_FRIENDINFOREPLY);

				frienduid = account_get_friend(account, bn_byte_get(packet->u.client_friendinforeq.friendnum));
				if (frienduid < 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] friend number {} not found", conn_get_socket(c), (int)bn_byte_get(packet->u.client_friendinforeq.friendnum));
					return -1;
				}

				bn_byte_set(&rpacket->u.server_friendinforeply.friendnum, bn_byte_get(packet->u.client_friendinforeq.friendnum));

				flist = account_get_friends(account);
				fr = friendlist_find_uid(flist, frienduid);

				if (fr == NULL || (dest_c = connlist_find_connection_by_account(friend_get_account(fr))) == NULL) {
					bn_byte_set(&rpacket->u.server_friendinforeply.type, FRIEND_TYPE_NON_MUTUAL);
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_OFFLINE);
					bn_int_set(&rpacket->u.server_friendinforeply.clienttag, 0);
					packet_append_string(rpacket, "");
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
					return 0;
				}

				type = FRIEND_TYPE_NON_MUTUAL;
				if (friend_get_mutual(fr))
					type |= FRIEND_TYPE_MUTUAL;
				if ((conn_get_dndstr(dest_c)))
					type |= FRIEND_TYPE_DND;
				if ((conn_get_awaystr(dest_c)))
					type |= FRIEND_TYPE_AWAY;
				bn_byte_set(&rpacket->u.server_friendinforeply.type, type);
				if ((game = conn_get_game(dest_c))) {
					if (game_get_flag(game) != game_flag_private)
						bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_PUBLIC_GAME);
					else
						bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_PRIVATE_GAME);
					packet_append_string(rpacket, game_get_name(game));
				}
				else if ((channel = conn_get_channel(dest_c))) {
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_CHAT);
					packet_append_string(rpacket, channel_get_name(channel));
				}
				else {
					bn_byte_set(&rpacket->u.server_friendinforeply.status, FRIENDSTATUS_ONLINE);
					packet_append_string(rpacket, "");
				}

				bn_int_set(&rpacket->u.server_friendinforeply.clienttag, conn_get_clienttag(dest_c));

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		static int _client_atfriendscreen(t_connection* c, t_packet const* const packet)
		{
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got CLIENT_ARRANGEDTEAM_FRIENDSCREEN packet", conn_get_socket(c));

			const char* my_username = conn_get_username(c);
			if (!my_username)
			{
				return -1;
			}

			t_channel* my_channel = conn_get_channel(c);
			int my_channel_is_public = 1;
			if (my_channel)
			{
				my_channel_is_public = channel_get_flags(my_channel) & channel_flags_public;
			}


			t_packet* rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not create friendscreen server packet", conn_get_socket(c));
				return -1;
			}

			packet_set_size(rpacket, sizeof(t_server_arrangedteam_friendscreen));
			packet_set_type(rpacket, SERVER_ARRANGEDTEAM_FRIENDSCREEN);


			std::uint8_t available_players = 0;

			// begin search for mutual and available friends
			t_list* my_friend_list = account_get_friends(conn_get_account(c));
			t_elem* entry;
			LIST_TRAVERSE(my_friend_list, entry)
			{
				t_friend* _friend = static_cast<t_friend*>(elem_get_data(entry));
				if (!_friend)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
					continue;
				}

				if (available_players == std::numeric_limits<decltype(available_players)>::max())
				{
					eventlog(eventlog_level_info, __FUNCTION__, "Reached maximum amount of available players to send in packet");
					break;
				}

				// skip non-mutual friends
				if (friend_get_mutual(_friend) == FRIEND_NOTMUTUAL)
				{
					continue;
				}

				t_account* friend_account = friend_get_account(_friend);
				t_connection* friend_connection = connlist_find_connection_by_account(friend_account);
				// if user is offline, then continue to next friend
				if (!friend_connection)
				{
					continue;
				}

				const VersionCheck* const my_vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
				const VersionCheck* const friend_vc = select_versioncheck(conn_get_archtag(friend_connection), conn_get_clienttag(friend_connection), conn_get_versionid(friend_connection), conn_get_gameversion(friend_connection), conn_get_checksum(friend_connection));
				std::string my_version_tag = my_vc ? my_vc->get_version_tag() : "";
				std::string friend_version_tag = friend_vc ? friend_vc->get_version_tag() : "";
				// friend is using another game or is on a different version
				if (my_version_tag != friend_version_tag)
				{
					continue;
				}

				// friend has Do Not Disturb mode enabled
				if (conn_get_dndstr(friend_connection))
				{
					continue;
				}

				// friend is away
				if (conn_get_awaystr(friend_connection))
				{
					continue;
				}

				// friend is not in a channel
				t_channel* friend_channel = conn_get_channel(friend_connection);
				if (!friend_channel)
				{
					continue;
				}

				// // don't list YET if in same private channel
				if (!my_channel_is_public && (friend_channel == my_channel))
				{
					continue;
				}

				const char* friend_name = account_get_name(friend_account);
				eventlog(eventlog_level_trace, __FUNCTION__, "Friend {} is available for an AT Game.", friend_name);
				available_players += 1;
				packet_append_string(rpacket, friend_name);
			}


			// now list matching users in same private chan
			if (!my_channel_is_public)
			{
				for (t_connection* user_connection = channel_get_first(my_channel); user_connection; user_connection = channel_get_next())
				{
					if (available_players == std::numeric_limits<decltype(available_players)>::max())
					{
						eventlog(eventlog_level_info, __FUNCTION__, "Reached maximum amount of available players to send in packet");
						break;
					}

					// skip self
					if (user_connection == c)
					{
						continue;
					}

					const VersionCheck* const my_vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
					const VersionCheck* const user_vc = select_versioncheck(conn_get_archtag(user_connection), conn_get_clienttag(user_connection), conn_get_versionid(user_connection), conn_get_gameversion(user_connection), conn_get_checksum(user_connection));
					std::string my_version_tag = my_vc ? my_vc->get_version_tag() : "";
					std::string user_version_tag = user_connection ? user_vc->get_version_tag() : "";
					// user is using another game or is on a different version
					if (my_version_tag != user_version_tag)
					{
						continue;
					}

					// user is dnd
					if (conn_get_dndstr(user_connection))
					{
						continue;
					}

					// user is away
					if (conn_get_awaystr(user_connection))
					{
						continue;
					}

					const char* username = account_get_name(conn_get_account(user_connection));
					if (!username)
					{
						continue;
					}

					eventlog(eventlog_level_trace, __FUNCTION__, "user {} in private channel {} is available for an AT Game.", username, channel_get_name(my_channel));
					available_players += 1;
					packet_append_string(rpacket, username);
				}
			}

			if (available_players == 0)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "No available players");
			}

			bn_byte_set(&rpacket->u.server_arrangedteam_friendscreen.f_count, available_players);

			conn_push_outqueue(c, rpacket);

			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_atinvitefriend(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_clienttag ctag;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_invite_friend)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_INVITE_FRIEND packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_invite_friend), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);

			{
				int count_to_invite, count, id;
				char const* invited_usernames[8];
				t_account* members[MAX_TEAMSIZE];
				int i, n, offset, teammemcount;
				t_connection* dest_c;
				t_team* team;
				unsigned int teamid;

				count_to_invite = bn_byte_get(packet->u.client_arrangedteam_invite_friend.numfriends);
				count = bn_int_get(packet->u.client_arrangedteam_invite_friend.count);
				id = bn_int_get(packet->u.client_arrangedteam_invite_friend.id);
				teammemcount = count_to_invite + 1;

				if ((count_to_invite < 1) || (count_to_invite > 3)) {
					eventlog(eventlog_level_error, __FUNCTION__, "got invalid number of users to invite to game");
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] got ARRANGEDTEAM INVITE packet for {} invitees", conn_get_socket(c), count_to_invite);

				offset = sizeof(t_client_arrangedteam_invite_friend);

				for (i = 0; i < count_to_invite; i++) {
					if (!(invited_usernames[i] = packet_get_str_const(packet, offset, MAX_USERNAME_LEN))) {
						eventlog(eventlog_level_error, "handle_bnet", "Could not get username from invite packet");
						return -1;
					}
					else {
						offset += std::strlen(invited_usernames[i]) + 1;
						eventlog(eventlog_level_debug, "handle_bnet", "Added user {} to invite array.", invited_usernames[i]);
					}
				}

				members[0] = conn_get_account(c);
				for (i = 1; i < MAX_TEAMSIZE; i++) {
					if ((i < teammemcount)) {
						if (!(members[i] = accountlist_find_account(invited_usernames[i - 1]))) {
							eventlog(eventlog_level_error, __FUNCTION__, "got invitation for non-existent user \"{}\"", invited_usernames[i - 1]);
							return -1;
						}
					}
					else
						members[i] = NULL;
				}


				if (!(team = account_find_team_by_accounts(members[0], members, ctag))) {
					team = create_team(members, ctag);	//no need to free on return -1 because it's already in teamlist

					eventlog(eventlog_level_trace, __FUNCTION__, "this team has never played before, creating new team");
				}
				else {
					eventlog(eventlog_level_trace, __FUNCTION__, "this team has already played before");
				}

				teamid = team_get_teamid(team);
				account_set_currentatteam(conn_get_account(c), team_get_teamid(team));


				//Create the packet to send to each of the users you wanted to invite
				conn_part_channel(c);

				for (i = 0; i < teammemcount; i++) {

					if (!(dest_c = account_get_conn(team_get_member(team, i))))
						continue;

					if ((dest_c == c))
						continue;

					if (!(rpacket = packet_create(packet_class_bnet)))
						return -1;

					packet_set_size(rpacket, sizeof(t_server_arrangedteam_send_invite));
					packet_set_type(rpacket, SERVER_ARRANGEDTEAM_SEND_INVITE);

					bn_int_set(&rpacket->u.server_arrangedteam_send_invite.count, count);
					bn_int_set(&rpacket->u.server_arrangedteam_send_invite.id, id);
					{			/* trans support */
						unsigned short port = conn_get_game_port(c);
						unsigned int addr = conn_get_addr(c);

						trans_net(conn_get_addr(dest_c), &addr, &port);

						bn_int_nset(&rpacket->u.server_arrangedteam_send_invite.inviterip, addr);
						bn_short_set(&rpacket->u.server_arrangedteam_send_invite.port, port);
					}
					bn_byte_set(&rpacket->u.server_arrangedteam_send_invite.numfriends, count_to_invite);

					for (n = 0; n < teammemcount; n++) {
						if (n != i)
							packet_append_string(rpacket, account_get_name(team_get_member(team, n)));
					}

					//now send packet
					conn_push_outqueue(dest_c, rpacket);
					packet_del_ref(rpacket);

					account_set_currentatteam(conn_get_account(dest_c), teamid);
				}

				//now send a ACK to the inviter
				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_arrangedteam_invite_friend_ack));
				packet_set_type(rpacket, SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK);

				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.count, count);
				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.id, id);
				bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.timestamp, now);
				bn_byte_set(&rpacket->u.server_arrangedteam_invite_friend_ack.teamsize, count_to_invite + 1);

				/*
				 * five int's to fill
				 * fill with uid's of all teammembers, including the inviter
				 * and the rest with FFFFFFFF
				 * to be used when sever receives anongame search
				 * [Omega]
				 */
				for (i = 0; i < 5; i++) {

					if (i < teammemcount) {
						bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.info[i], team_get_memberuid(team, i));
					}
					else {		/* fill rest with FFFFFFFF */
						bn_int_set(&rpacket->u.server_arrangedteam_invite_friend_ack.info[i], 0xFFFFFFFF);
					}
				}

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

			}

			return 0;
		}

		static int _client_atacceptdeclineinvite(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_clienttag ctag;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_accept_decline_invite)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_ACCEPT_DECLINE_INVITE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_accept_decline_invite), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);

			{
				char const* inviter;
				t_connection* dest_c;

				//if user declined the invitation then
				if (bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.option) == CLIENT_ARRANGEDTEAM_DECLINE) {
					inviter = packet_get_str_const(packet, sizeof(t_client_arrangedteam_accept_decline_invite), MAX_USERNAME_LEN);
					dest_c = connlist_find_connection_by_accountname(inviter);

					eventlog(eventlog_level_info, "handle_bnet", "{} declined a arranged team game with {}", conn_get_username(c), inviter);

					if (!(rpacket = packet_create(packet_class_bnet)))
						return -1;
					packet_set_size(rpacket, sizeof(t_server_arrangedteam_member_decline));
					packet_set_type(rpacket, SERVER_ARRANGEDTEAM_MEMBER_DECLINE);

					bn_int_set(&rpacket->u.server_arrangedteam_member_decline.count, bn_int_get(packet->u.client_arrangedteam_accept_decline_invite.count));
					bn_int_set(&rpacket->u.server_arrangedteam_member_decline.action, SERVER_ARRANGEDTEAM_DECLINE);
					packet_append_string(rpacket, conn_get_username(c));

					conn_push_outqueue(dest_c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_atacceptinvite(t_connection* c, t_packet const* const packet)
		{
			// t_packet * rpacket;

			if (packet_get_size(packet) < sizeof(t_client_arrangedteam_accept_invite)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ARRANGEDTEAM_ACCEPT_INVITE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_arrangedteam_accept_invite), packet_get_size(packet));
				return -1;
			}
			/* conn_set_channel(c, "Arranged Teams"); */
			return 0;
		}

		typedef struct {
			t_connection* c;
			unsigned lnews;
			unsigned fnews;
		} t_motd_data;

		static int _news_cb(std::time_t date, t_lstr* lstr, void* data)
		{
			t_packet* rpacket;
			t_motd_data* motdd = (t_motd_data*)data;

			if (date < motdd->lnews)
				return -1;		/* exit traversing */

			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_motd_w3));
			packet_set_type(rpacket, SERVER_MOTD_W3);

			bn_byte_set(&rpacket->u.server_motd_w3.msgtype, SERVER_MOTD_W3_MSGTYPE);
			bn_int_set(&rpacket->u.server_motd_w3.curr_time, now);

			bn_int_set(&rpacket->u.server_motd_w3.first_news_time, motdd->fnews);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp, date);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp2, date);

			/* Append news to packet, we used the already cached len in the lstr */
			packet_append_lstr(rpacket, lstr);

			/* Send news packet */
			conn_push_outqueue(motdd->c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		// motd for warcraft 3 (http://img21.imageshack.us/img21/1808/j2py.png)
		static int _client_motdw3(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_clienttag ctag;
			t_motd_data motdd;

			if (packet_get_size(packet) < sizeof(t_client_motd_w3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_MOTD_W3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_motd_w3), packet_get_size(packet));
				return -1;
			}

			ctag = conn_get_clienttag(c);
			/* if in a game, remove user from his game */
			if (conn_get_game(c) != NULL)
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);

			/* News */
			motdd.lnews = bn_int_get(packet->u.client_motd_w3.last_news_time);
			motdd.fnews = news_get_firstnews();
			motdd.c = c;

			eventlog(eventlog_level_trace, __FUNCTION__, "lastnews() {} news_time {}", news_get_lastnews(), motdd.lnews);
			news_traverse(_news_cb, &motdd);

			/* Welcome Message */
			rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
				return -1;

			packet_set_size(rpacket, sizeof(t_server_motd_w3));
			packet_set_type(rpacket, SERVER_MOTD_W3);

			//bn_int_set(&rpacket->u.server_motd_w3.ticks,get_ticks());
			bn_byte_set(&rpacket->u.server_motd_w3.msgtype, SERVER_MOTD_W3_MSGTYPE);
			bn_int_set(&rpacket->u.server_motd_w3.curr_time, now);
			bn_int_set(&rpacket->u.server_motd_w3.first_news_time, motdd.fnews);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp, motdd.fnews + 1);
			bn_int_set(&rpacket->u.server_motd_w3.timestamp2, SERVER_MOTD_W3_WELCOME);


			// read text from w3motd.txt
			{
				std::string serverinfo;

				std::string filename = i18n_filename(prefs_get_motdw3file(), conn_get_gamelang_localized(c));
				std::FILE* fp = std::fopen(filename.c_str(), "r");
				if (fp)
				{
					while (char* buff = file_get_line(fp))
					{
						char* line = message_format_line(c, buff);
						fmt::format_to(std::back_inserter(serverinfo), "{}\n", line + 1);
						xfree((void*)line);
					}

					if (std::fclose(fp) == EOF)
					{
						eventlog(eventlog_level_error, __FUNCTION__, "Failed to close w3motd file \"{}\" after reading (std::fopen: {})", filename, std::strerror(errno));
					}
				}
				else
				{
					eventlog(eventlog_level_error, __FUNCTION__, "Failed to open w3motd file \"{}\"", filename);
					fmt::format_to(std::back_inserter(serverinfo), "An error has occurred.");
				}

				packet_append_string(rpacket, serverinfo.c_str());
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_realmlistreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmlistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMLISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmlistreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				t_elem const* curr;
				t_realm const* realm;
				t_server_realmlistreply_data realmdata;
				unsigned int count;

				packet_set_size(rpacket, sizeof(t_server_realmlistreply));
				packet_set_type(rpacket, SERVER_REALMLISTREPLY);
				bn_int_set(&rpacket->u.server_realmlistreply.unknown1, SERVER_REALMLISTREPLY_UNKNOWN1);
				count = 0;
				LIST_TRAVERSE_CONST(realmlist(), curr) {
					realm = (t_realm*)elem_get_data(curr);
					if (!realm_get_active(realm))
						continue;
					bn_int_set(&realmdata.unknown3, SERVER_REALMLISTREPLY_DATA_UNKNOWN3);
					bn_int_set(&realmdata.unknown4, SERVER_REALMLISTREPLY_DATA_UNKNOWN4);
					bn_int_set(&realmdata.unknown5, SERVER_REALMLISTREPLY_DATA_UNKNOWN5);
					bn_int_set(&realmdata.unknown6, SERVER_REALMLISTREPLY_DATA_UNKNOWN6);
					bn_int_set(&realmdata.unknown7, SERVER_REALMLISTREPLY_DATA_UNKNOWN7);
					bn_int_set(&realmdata.unknown8, SERVER_REALMLISTREPLY_DATA_UNKNOWN8);
					bn_int_set(&realmdata.unknown9, SERVER_REALMLISTREPLY_DATA_UNKNOWN9);
					packet_append_data(rpacket, &realmdata, sizeof(realmdata));
					packet_append_string(rpacket, realm_get_name(realm));
					packet_append_string(rpacket, realm_get_description(realm));
					count++;
				}
				bn_int_set(&rpacket->u.server_realmlistreply.count, count);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_realmlistreq110(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmlistreq_110)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMLISTREQ_110 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmlistreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				t_elem const* curr;
				t_realm const* realm;
				t_server_realmlistreply_110_data realmdata;
				unsigned int count;

				packet_set_size(rpacket, sizeof(t_server_realmlistreply_110));
				packet_set_type(rpacket, SERVER_REALMLISTREPLY_110);
				bn_int_set(&rpacket->u.server_realmlistreply_110.unknown1, SERVER_REALMLISTREPLY_110_UNKNOWN1);
				count = 0;
				LIST_TRAVERSE_CONST(realmlist(), curr) {
					realm = (t_realm*)elem_get_data(curr);
					if (!realm_get_active(realm))
						continue;
					bn_int_set(&realmdata.unknown1, SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1);
					packet_append_data(rpacket, &realmdata, sizeof(realmdata));
					packet_append_string(rpacket, realm_get_name(realm));
					packet_append_string(rpacket, realm_get_description(realm));
					count++;
				}
				bn_int_set(&rpacket->u.server_realmlistreply_110.count, count);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_claninforeq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			int count;
			char const* username;
			t_account* account;
			t_clanmember* clanmember;
			t_clan* clan;
			t_clantag clantag1, clantag2;

			if (packet_get_size(packet) < sizeof(t_client_claninforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_claninforeq), packet_get_size(packet));
				return -1;
			}

			count = bn_int_get(packet->u.client_claninforeq.count);
			clantag1 = bn_int_get(packet->u.client_claninforeq.clantag);
			clan = NULL;

			if (!(username = packet_get_str_const(packet, sizeof(t_client_claninforeq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANINFOREQ (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_error, __FUNCTION__, "requested claninfo for non-existent account");
				return -1;
			}

			if ((clanmember = account_get_clanmember(account)) && (clan = clanmember_get_clan(clanmember)))
				clantag2 = clan_get_clantag(clan);
			else
				clantag2 = 0;

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_claninforeply));
				packet_set_type(rpacket, SERVER_CLANINFOREPLY);
				bn_int_set(&rpacket->u.server_profilereply.count, count);
				if (clantag1 == clantag2) {
					t_bnettime bn_time;
					bn_long ltime;

					bn_byte_set(&rpacket->u.server_claninforeply.fail, 0);

					packet_append_string(rpacket, clan_get_name(clan));
					char status = clanmember_get_status(clanmember);
					packet_append_data(rpacket, &status, 1);
					std::time_t temp = clanmember_get_join_time(clanmember);
					bn_time = time_to_bnettime(temp, 0);
					bn_time = bnettime_add_tzbias(bn_time, -conn_get_tzbias(c));
					bnettime_to_bn_long(bn_time, &ltime);
					packet_append_data(rpacket, &ltime, 8);
				}
				else
					bn_byte_set(&rpacket->u.server_claninforeply.fail, 1);


				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_profilereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			int count;
			char const* username;
			t_account* account;
			t_clanmember* clanmember;
			t_clan* clan;
			bn_int clanTAG;

			if (packet_get_size(packet) < sizeof(t_client_profilereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROFILEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_profilereq), packet_get_size(packet));
				return -1;
			}

			count = bn_int_get(packet->u.client_profilereq.count);

			if (!(username = packet_get_str_const(packet, sizeof(t_client_profilereq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROFILEREQ (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_error, __FUNCTION__, "requested profile for non-existent account");
				return -1;
			}
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_profilereply));
				packet_set_type(rpacket, SERVER_PROFILEREPLY);
				bn_int_set(&rpacket->u.server_profilereply.count, count);
				bn_byte_set(&rpacket->u.server_profilereply.fail, 0);
				packet_append_string(rpacket, account_get_desc(account).c_str());
				packet_append_string(rpacket, account_get_loc(account).c_str());
				if ((clanmember = account_get_clanmember(account)) && (clan = clanmember_get_clan(clanmember)))
					bn_int_set(&clanTAG, clan_get_clantag(clan));
				else
					bn_int_set(&clanTAG, 0);
				packet_append_data(rpacket, clanTAG, 4);

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}


		static int _client_realmjoinreq109(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmjoinreq_109)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMJOINREQ_109 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmjoinreq_109), packet_get_size(packet));
				return -1;
			}

			{
				char const* realmname;
				t_realm* realm;

				if (!(realmname = packet_get_str_const(packet, sizeof(t_client_realmjoinreq_109), MAX_REALMNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMJOINREQ_109 (missing or too long realmname)", conn_get_socket(c));
					return -1;
				}

				if ((realm = realmlist_find_realm(realmname))) {
					unsigned int salt;
					struct {
						bn_int salt;
						bn_int sessionkey;
						bn_int sessionnum;
						bn_int secret;
						bn_int passhash[5];
					} temp;
					char const* pass_str;
					t_hash secret_hash;
					t_hash passhash;
					t_realm* prev_realm;

					/* FIXME: should we only set this after they log in to the realm server? */
					prev_realm = conn_get_realm(c);
					if (prev_realm) {
						if (prev_realm != realm) {
							realm_add_player_number(realm, 1);
							realm_add_player_number(prev_realm, -1);
							conn_set_realm(c, realm);
						}
					}
					else {
						realm_add_player_number(realm, 1);
						conn_set_realm(c, realm);
					}

					if ((pass_str = account_get_pass(conn_get_account(c)))) {
						if (hash_set_str(&passhash, pass_str) == 0) {
							hash_to_bnhash((t_hash const*)&passhash, temp.passhash);
							salt = bn_int_get(packet->u.client_realmjoinreq_109.seqno);
							bn_int_set(&temp.salt, salt);
							bn_int_set(&temp.sessionkey, conn_get_sessionkey(c));
							bn_int_set(&temp.sessionnum, conn_get_sessionnum(c));
							bn_int_set(&temp.secret, conn_get_secret(c));
							bnet_hash(&secret_hash, sizeof(temp), &temp);

							if ((rpacket = packet_create(packet_class_bnet))) {
								packet_set_size(rpacket, sizeof(t_server_realmjoinreply_109));
								packet_set_type(rpacket, SERVER_REALMJOINREPLY_109);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno, salt);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u1, 0x0);
								bn_short_set(&rpacket->u.server_realmjoinreply_109.u3, 0x0);	/* reg auth */
								bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1, 0x0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum, conn_get_sessionnum(c));
								{	/* trans support */
									unsigned int addr = realm_get_ip(realm);
									unsigned short port = realm_get_port(realm);

									trans_net(conn_get_addr(c), &addr, &port);

									bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr, addr);
									bn_short_nset(&rpacket->u.server_realmjoinreply_109.port, port);
								}
								bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey, conn_get_sessionkey(c));
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u5, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u6, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u7, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid, conn_get_versionid(c));
								bn_int_set(&rpacket->u.server_realmjoinreply_109.clienttag, conn_get_clienttag(c));
								hash_to_bnhash((t_hash const*)&secret_hash, rpacket->u.server_realmjoinreply_109.secret_hash);	/* avoid warning */
								packet_append_string(rpacket, conn_get_username(c));
								conn_push_outqueue(c, rpacket);
								packet_del_ref(rpacket);
							}
							return 0;
						}
						else
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] realm join for \"{}\" failed (unable to hash password)", conn_get_socket(c), conn_get_loggeduser(c));
					}
					else {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] realm join for \"{}\" failed (no password)", conn_get_socket(c), conn_get_loggeduser(c));
					}
				}
				else
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not find active realm \"{}\"", conn_get_socket(c), realmname);

				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_realmjoinreply_109));
					packet_set_type(rpacket, SERVER_REALMJOINREPLY_109);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno, bn_int_get(packet->u.client_realmjoinreq_109.seqno));
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u1, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum, 0);
					bn_short_set(&rpacket->u.server_realmjoinreply_109.u3, 0);
					bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr, 0);
					bn_short_nset(&rpacket->u.server_realmjoinreply_109.port, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u5, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u6, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u7, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.clienttag, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[0], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[1], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[2], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[3], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[4], 0);
					packet_append_string(rpacket, "");
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_charlistreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_unknown_37)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_37 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_37), packet_get_size(packet));
				return -1;
			}
			/*
			   0x0070:                           83 80 ff ff ff ff ff 2f    t,taran,......./
			   0x0080: ff ff ff ff ff ff ff ff   ff ff 03 ff ff ff ff ff    ................
			   0x0090: ff ff ff ff ff ff ff ff   ff ff ff 07 80 80 80 80    ................
			   0x00a0: ff ff ff 00
			   */
			if ((rpacket = packet_create(packet_class_bnet))) {
				char const* charlist;
				char* temp;

				packet_set_size(rpacket, sizeof(t_server_unknown_37));
				packet_set_type(rpacket, SERVER_UNKNOWN_37);
				bn_int_set(&rpacket->u.server_unknown_37.unknown1, SERVER_UNKNOWN_37_UNKNOWN1);
				bn_int_set(&rpacket->u.server_unknown_37.unknown2, SERVER_UNKNOWN_37_UNKNOWN2);

				if (!(charlist = account_get_closed_characterlist(conn_get_account(c), conn_get_clienttag(c), realm_get_name(conn_get_realm(c))))) {
					bn_int_set(&rpacket->u.server_unknown_37.count, 0);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
					return 0;
				}
				temp = xstrdup(charlist);

				{
					char const* tok1;
					char const* tok2;
					t_character const* ch;
					unsigned int count;

					count = 0;
					tok1 = (char const*)std::strtok(temp, ",");	/* std::strtok modifies the string it is passed */
					tok2 = std::strtok(NULL, ",");
					while (tok1) {
						if (!tok2) {
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" has bad character list \"{}\"", conn_get_socket(c), conn_get_username(c), temp);
							break;
						}

						if ((ch = characterlist_find_character(tok1, tok2))) {
							packet_append_ntstring(rpacket, character_get_realmname(ch));
							packet_append_ntstring(rpacket, ",");
							packet_append_string(rpacket, character_get_name(ch));
							packet_append_string(rpacket, character_get_playerinfo(ch));
							packet_append_string(rpacket, character_get_guildname(ch));
							count++;
						}
						else
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] character \"{}\" is missing", conn_get_socket(c), tok2);
						tok1 = std::strtok(NULL, ",");
						tok2 = std::strtok(NULL, ",");
					}
					xfree(temp);

					bn_int_set(&rpacket->u.server_unknown_37.count, count);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_unknown39(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_unknown_39)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_39 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_39), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		static int _client_adreq(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_adreq))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adreq), packet_get_size(packet));
				return -1;
			}

			/*
			eventlog(eventlog_level_debug, __FUNCTION__, "[%d] SID_CHECKAD { %d %d %d %d }", conn_get_socket(c), bn_int_get(packet->u.client_adreq.archtag),
				bn_int_get(packet->u.client_adreq.clienttag), bn_int_get(packet->u.client_adreq.prev_adid), bn_int_get(packet->u.client_adreq.ticks));
			*/

			const AdBanner* ad = AdBannerList.pick(conn_get_clienttag(c), conn_get_gamelang(c), bn_int_get(packet->u.client_adreq.prev_adid));
			if (!ad)
			{
				return 0;
			}

			t_packet* const rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "Could not create a packet");
				return -1;
			}
			packet_set_size(rpacket, sizeof(t_server_adreply));
			packet_set_type(rpacket, SERVER_ADREPLY);
			bn_int_set(&rpacket->u.server_adreply.adid, ad->get_id());
			bn_int_set(&rpacket->u.server_adreply.extensiontag, ad->get_extension_tag());
			file_to_mod_time(c, ad->get_filename().c_str(), &rpacket->u.server_adreply.timestamp);
			packet_append_string(rpacket, ad->get_filename().c_str());
			packet_append_string(rpacket, ad->get_url().c_str());
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_adack(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_adack)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADACK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adack), packet_get_size(packet));
				return -1;
			}

			/*
			   {
			   char const * tname;

			   eventlog(eventlog_level_info,__FUNCTION__,"[%d] ad acknowledgement for adid 0x%04x from \"%s\"",conn_get_socket(c),bn_int_get(packet->u.client_adack.adid),(tname = conn_get_chatname(c)));
			   conn_unget_chatname(c,tname);
			   }
			   */
			return 0;
		}

		static int _client_adclick(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_adclick)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADCLICK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adclick), packet_get_size(packet));
				return -1;
			}

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] ad click for adid 0x{:04x} from \"{}\"", conn_get_socket(c), bn_int_get(packet->u.client_adclick.adid), conn_get_username(c));

			return 0;
		}

		static int _client_adclick2(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_adclick2))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ADCLICK2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_adclick2), packet_get_size(packet));
				return -1;
			}

			eventlog(eventlog_level_trace, __FUNCTION__, "[{}] ad click2 for adid 0x{:04x} from \"{}\"", conn_get_socket(c), bn_int_get(packet->u.client_adclick2.adid), conn_get_username(c));

			const AdBanner* const ad = AdBannerList.find(conn_get_clienttag(c), conn_get_gamelang(c), bn_int_get(packet->u.client_adclick2.adid));
			if (!ad)
			{
				return 0;
			}

			t_packet* const rpacket = packet_create(packet_class_bnet);
			if (!rpacket)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "Could not create a packet");
				return -1;
			}
			packet_set_size(rpacket, sizeof(t_server_adclickreply2));
			packet_set_type(rpacket, SERVER_ADCLICKREPLY2);
			bn_int_set(&rpacket->u.server_adclickreply2.adid, ad->get_id());
			packet_append_string(rpacket, ad->get_url().c_str());
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_readmemory(t_connection* c, t_packet const* const packet)
		{
			unsigned int size, offset, request_id;

			if (packet_get_size(packet) < sizeof(t_client_readmemory)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad READMEMORY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_readmemory), packet_get_size(packet));
				return -1;
			}

			request_id = bn_int_get(packet->u.client_readmemory.request_id);

			size = (unsigned int)packet_get_size(packet);
			offset = sizeof(t_client_readmemory);

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] Received READMEMORY packet with Request ID: {} and Memory size: {}", conn_get_socket(c), request_id, size - offset);

#ifdef WITH_LUA
			std::vector<int> _data;
			for (int i = offset; i < size; i++)
			{
				_data.push_back(packet->u.data[i]);
			}
			lua_handle_client_readmemory(c, request_id, _data);
#endif

			return 0;
		}

		static int _client_statsupdate(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_statsupdate)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_statsupdate), packet_get_size(packet));
				return -1;
			}

			{
				char const* name;
				char const* key;
				char const* val;
				unsigned int name_count;
				unsigned int key_count;
				unsigned int i, j;
				unsigned int name_off;
				unsigned int keys_off;
				unsigned int key_off;
				unsigned int vals_off;
				unsigned int val_off;
				t_account* account;

				name_count = bn_int_get(packet->u.client_statsupdate.name_count);
				key_count = bn_int_get(packet->u.client_statsupdate.key_count);

				if (name_count != 1)
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious STATSUPDATE packet (name_count={})", conn_get_socket(c), name_count);

				for (i = 0, name_off = sizeof(t_client_statsupdate); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1);
				if (i < name_count) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (only {} names of {})", conn_get_socket(c), i, name_count);
					return -1;
				}
				keys_off = name_off;

				for (i = 0, key_off = keys_off; i < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)); i++, key_off += std::strlen(key) + 1);
				if (i < key_count) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSUPDATE packet (only {} keys of {})", conn_get_socket(c), i, key_count);
					return -1;
				}
				vals_off = key_off;

				if ((account = conn_get_account(c))) {
					if (account_get_auth_changeprofile(account) == 0) {	/* default to true */
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] stats update for \"{}\" refused (no change profile access)", conn_get_socket(c), conn_get_username(c));
						return -1;
					}
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] updating player profile for \"{}\"", conn_get_socket(c), conn_get_username(c));

					for (i = 0, name_off = sizeof(t_client_statsupdate); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1)
						for (j = 0, key_off = keys_off, val_off = vals_off; j < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)) && (val = packet_get_str_const(packet, val_off, MAX_ATTRVAL_STR)); j++, key_off += std::strlen(key) + 1, val_off += std::strlen(val) + 1)
							if (std::strlen(key) < 9 || strncasecmp(key, "profile\\", 8) != 0)
								eventlog(eventlog_level_error, __FUNCTION__, "[{}] got STATSUPDATE with suspicious key \"{}\" value \"{}\"", conn_get_socket(c), key, val);
							else
								account_set_strattr(account, key, val);
				}
			}

			return 0;
		}

		static int _client_playerinforeq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_playerinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_playerinforeq), packet_get_size(packet));
				return -1;
			}

			{
				char const* username;
				char const* info;
				t_account* account;

				if (!(username = packet_get_str_const(packet, sizeof(t_client_playerinforeq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ (missing or too long username)", conn_get_socket(c));
					return -1;
				}
				if (!(info = packet_get_str_const(packet, sizeof(t_client_playerinforeq) + std::strlen(username) + 1, MAX_PLAYERINFO_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PLAYERINFOREQ (missing or too long info)", conn_get_socket(c));
					return -1;
				}

				if (info[0] != '\0')
					conn_set_playerinfo(c, info);
				if (!username[0])
					username = conn_get_loggeduser(c);

				account = conn_get_account(c);

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_playerinforeply));
				packet_set_type(rpacket, SERVER_PLAYERINFOREPLY);

				if (account) {
					packet_append_string(rpacket, username);

					nonstd::optional<std::string> playerinfo = conn_get_playerinfo(c);
					if (playerinfo.has_value())
					{
						packet_append_string(rpacket, playerinfo.value().c_str());
					}
					else
					{
						// just send reversed client tag, it's better than nothing

						t_clienttag client_tag = conn_get_clienttag(c);
						char reversed_client_tag[5] = {};
						tag_uint_to_revstr(reversed_client_tag, client_tag);

						packet_append_string(rpacket, reversed_client_tag);
					}

					packet_append_string(rpacket, username);
				}
				else {
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, "");
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_progident2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_progident2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROGIDENT2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_progident2), packet_get_size(packet));
				return -1;
			}

			/* d2 uses this packet with clienttag = 0 to request the channel list */
			if (bn_int_get(packet->u.client_progident2.clienttag)) {
				if (tag_check_in_list(bn_int_get(packet->u.client_progident2.clienttag), prefs_get_allowed_clients())) {
					conn_set_state(c, conn_state_destroy);
					return 0;
				}
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] CLIENT_PROGIDENT2 clienttag=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_progident2.clienttag));

				/* Hmm... no archtag.  Hope we get it in CLIENT_AUTHREQ1 (but we won't if we use the shortcut) */

				conn_set_clienttag(c, bn_int_get(packet->u.client_progident2.clienttag));
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_channellist));
				packet_set_type(rpacket, SERVER_CHANNELLIST);
				{
					t_channel* ch;
					t_elem const* curr;

					LIST_TRAVERSE_CONST(channellist(), curr) {
						ch = (t_channel*)elem_get_data(curr);
						if ((!(channel_get_flags(ch) & channel_flags_clan)) && (!prefs_get_hide_temp_channels() || channel_get_permanent(ch)) && (!channel_get_clienttag(ch) || channel_get_clienttag(ch) == conn_get_clienttag(c)) && (!(channel_get_flags(ch) & channel_flags_thevoid)) &&	// don't display theVoid in channel list
							((channel_get_max(ch) != 0) || ((channel_get_max(ch) == 0) && (account_is_operator_or_admin(conn_get_account(c), channel_get_name(ch)) == 1))))	// don't display restricted channel for no admins/ops
							packet_append_string(rpacket, channel_get_name(ch));
					}
				}
				packet_append_string(rpacket, "");
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_joinchannel(t_connection* c, t_packet const* const packet)
		{
			t_account* account;
			char const* cname;
			int found = 1;
			t_clan* user_clan;
			t_clantag clantag;
			std::uint32_t clienttag;
			t_channel* channel;

			if (packet_get_size(packet) < sizeof(t_client_joinchannel)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOINCHANNEL packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_joinchannel), packet_get_size(packet));
				return -1;
			}

			account = conn_get_account(c);

			if (!(cname = packet_get_str_const(packet, sizeof(t_client_joinchannel), MAX_CHANNELNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOINCHANNEL (missing or too long cname)", conn_get_socket(c));
				return -1;
			}

			if ((channel = conn_get_channel(c)) && (strcasecmp(channel_get_name(channel), cname) == 0))
				return 0;		//we are already in this channel

			std::string tmpstr;
			clienttag = conn_get_clienttag(c);
			if ((clienttag == CLIENTTAG_WARCRAFT3_UINT) || (clienttag == CLIENTTAG_WAR3XP_UINT)) {
				switch (bn_int_get(packet->u.client_joinchannel.channelflag)) {
				case CLIENT_JOINCHANNEL_NORMAL:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_NORMAL channel \"{}\"", conn_get_socket(c), cname);

					if (prefs_get_ask_new_channel() && (!(channellist_find_channel_by_name(cname, conn_get_country(c), realm_get_name(conn_get_realm(c)))))) {
						found = 0;
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] didn't find channel \"{}\" to join", conn_get_socket(c), cname);
						message_send_text(c, message_type_channeldoesnotexist, c, cname);
					}
					break;
				case CLIENT_JOINCHANNEL_GENERIC:

					if ((user_clan = account_get_clan(account)) && (clantag = clan_get_clantag(user_clan)))
					{
						std::ostringstream ostr;
						ostr << "Clan " << clantag_to_str(clantag);
						tmpstr = ostr.str();
						cname = tmpstr.c_str();
					}
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_GENERIC channel \"{}\"", conn_get_socket(c), cname);

					/* don't have to do anything here */
					break;
				case CLIENT_JOINCHANNEL_CREATE:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_CREATE channel \"{}\"", conn_get_socket(c), cname);
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_JOINCHANNEL_CREATE channel \"{}\"", conn_get_socket(c), cname);
					/* don't have to do anything here */
					break;
				}

				if (found && conn_set_channel(c, cname) < 0)
					conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
			}
			else {

				// not W3
				if (conn_set_channel(c, cname) < 0)
					conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
			}
			// here we set channel flags on user
			channel_set_userflags(c);

			return 0;
		}

		static int _client_message(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_message)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MESSAGE packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_message), packet_get_size(packet));
				return -1;
			}

			{
				char const* text;
				t_channel const* channel;

				if (!(text = packet_get_str_const(packet, sizeof(t_client_message), MAX_MESSAGE_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MESSAGE (missing or too long text)", conn_get_socket(c));
					return -1;
				}

				conn_set_idletime(c);

				if ((channel = conn_get_channel(c)))
					channel_message_log(channel, c, 1, text);
				/* we don't log game commands currently */


				if (text[0] == '/')
				{
					account_add_newevent(conn_get_account(c), 1);

					handle_command(c, text);
				}
				else if (channel && !conn_quota_exceeded(c, text))
				{
					account_add_newevent(conn_get_account(c), 2);

					channel_message_send(channel, message_type_talk, c, text);
				}
				/* else discard */
			}

			return 0;
		}

		static int _glist_cb(t_game* game, void* data)
		{
			struct glist_cbdata* cbdata = (struct glist_cbdata*)data;
			char clienttag_str[5];
			t_server_gamelistreply_game glgame;
			unsigned int addr;
			unsigned short port;
			bn_int game_spacer = { 1, 0, 0, 0 };

			cbdata->tcount++;
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] considering listing game=\"{}\", pass=\"{}\" clienttag=\"{}\" gtype={}", conn_get_socket(cbdata->c), game_get_name(game), game_get_pass(game), tag_uint_to_str(clienttag_str, game_get_clienttag(game)), (int)game_get_type(game));

			if (prefs_get_hide_pass_games() && game_get_flag(game) == game_flag_private) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is passworded or has private flag", conn_get_socket(cbdata->c));
				return 0;
			}
			if (prefs_get_hide_started_games() && game_get_status(game) != game_status_open) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is not open", conn_get_socket(cbdata->c));
				return 0;
			}
			if (game_get_clienttag(game) != conn_get_clienttag(cbdata->c)) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is for a different client", conn_get_socket(cbdata->c));
				return 0;
			}
			if (cbdata->gtype != game_type_all && game_get_type(game) != cbdata->gtype) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is wrong type", conn_get_socket(cbdata->c));
				return 0;
			}
			const VersionCheck* const cbdata_c_vc = select_versioncheck(conn_get_archtag(cbdata->c), conn_get_clienttag(cbdata->c), conn_get_versionid(cbdata->c), conn_get_gameversion(cbdata->c), conn_get_checksum(cbdata->c));
			t_connection* gowner_c = game_get_owner(game);
			const VersionCheck* const gowner_c_vc = select_versioncheck(conn_get_archtag(gowner_c), conn_get_clienttag(gowner_c), conn_get_versionid(gowner_c), conn_get_gameversion(gowner_c), conn_get_checksum(gowner_c));
			if (cbdata_c_vc != nullptr &&
				gowner_c_vc != nullptr &&
				cbdata_c_vc->get_version_tag() != gowner_c_vc->get_version_tag())
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is wrong versiontag", conn_get_socket(cbdata->c));
				return 0;
			}
			bn_short_set(&glgame.gametype, gtype_to_bngtype(game_get_type(game)));
			bn_short_set(&glgame.unknown1, SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
			bn_short_set(&glgame.unknown3, SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
			addr = game_get_addr(game);
			port = game_get_port(game);
			trans_net(conn_get_addr(cbdata->c), &addr, &port);
			bn_short_nset(&glgame.port, port);
			bn_int_nset(&glgame.game_ip, addr);
			bn_int_set(&glgame.unknown4, SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
			bn_int_set(&glgame.unknown5, SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
			switch (game_get_status(game)) {
			case game_status_started:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_STARTED);
				break;
			case game_status_full:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_FULL);
				break;
			case game_status_open:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
				break;
			case game_status_done:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_DONE);
				break;
			default:
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] game \"{}\" has bad status={}", conn_get_socket(cbdata->c), game_get_name(game), (int)game_get_status(game));
				bn_int_set(&glgame.status, 0);
			}
			bn_int_set(&glgame.unknown6, SERVER_GAMELISTREPLY_GAME_UNKNOWN6);

			if (packet_get_size(cbdata->rpacket) + sizeof(glgame) + std::strlen(game_get_name(game)) + 1 + std::strlen(game_get_pass(game)) + 1 + std::strlen(game_get_info(game)) + 1 > MAX_PACKET_SIZE) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] out of room for games", conn_get_socket(cbdata->c));
				return -1;			/* no more room */
			}

			if (cbdata->counter) {
				packet_append_data(cbdata->rpacket, &game_spacer, sizeof(game_spacer));
			}

			packet_append_data(cbdata->rpacket, &glgame, sizeof(glgame));
			//here
			auto ownerconn = game_get_owner(game);
			if (conn_is_new_client(ownerconn))
				packet_append_string(cbdata->rpacket, game_get_name(game));
			else
				packet_append_string(cbdata->rpacket, game_get_visibledname(game));
			packet_append_string(cbdata->rpacket, game_get_pass(game));
			packet_append_string(cbdata->rpacket, game_get_info(game));
			cbdata->counter++;

			return 0;
		}

		static int _client_gamelistreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			char const* gamename;
			char const* gamepass;
			unsigned short bngtype;
			t_game_type gtype;
			t_clienttag clienttag;
			t_game* game;
			t_server_gamelistreply_game glgame;
			unsigned int addr;
			unsigned short port;
			char clienttag_str[5];

			if (packet_get_size(packet) < sizeof(t_client_gamelistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_gamelistreq), packet_get_size(packet));
				return -1;
			}

			if (!(gamename = packet_get_str_const(packet, sizeof(t_client_gamelistreq), MAX_GAMENAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ (missing or too long gamename)", conn_get_socket(c));
				return -1;
			}

			if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_gamelistreq) + std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ (missing or too long password)", conn_get_socket(c));
				return -1;
			}

			bngtype = bn_short_get(packet->u.client_gamelistreq.gametype);
			clienttag = conn_get_clienttag(c);
			gtype = bngreqtype_to_gtype(clienttag, bngtype);
			if (!(rpacket = packet_create(packet_class_bnet)))
				return -1;
			packet_set_size(rpacket, sizeof(t_server_gamelistreply));
			packet_set_type(rpacket, SERVER_GAMELISTREPLY);

			bn_int_set(&rpacket->u.server_gamelistreply.sstatus, 0);

			/* specific game requested? */
			if (gamename[0] != '\0') {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY looking for specific game tag=\"{}\" bngtype=0x{:08x} gtype={} name=\"{}\" pass=\"{}\"", conn_get_socket(c), tag_uint_to_str(clienttag_str, clienttag), bngtype, (int)gtype, gamename, gamepass);
				if ((game = gamelist_find_game(gamename, clienttag, gtype))) {
					/* game found but first we need to make sure everything is OK */
					bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 0);
					switch (game_get_status(game)) {
					case game_status_started:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_STARTED);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but started", conn_get_socket(c));
						break;
					case game_status_full:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_FULL);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but full", conn_get_socket(c));
						break;
					case game_status_done:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_NOTFOUND);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but done", conn_get_socket(c));
						break;
					case game_status_open:
					case game_status_loaded:
						if (std::strcmp(gamepass, game_get_pass(game))) {	/* passworded game must match password in request */
							bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_PASS);
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but is password protected and wrong password given", conn_get_socket(c));
							break;
						}

						if (game_get_status(game) == game_status_loaded) {
							bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_LOADED);
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found loaded game", conn_get_socket(c));
						}

						/* everything seems fine, lets reply with the found game */
						bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
						bn_short_set(&glgame.gametype, gtype_to_bngtype(game_get_type(game)));
						bn_short_set(&glgame.unknown1, SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
						bn_short_set(&glgame.unknown3, SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
						addr = game_get_addr(game);
						port = game_get_port(game);
						trans_net(conn_get_addr(c), &addr, &port);
						bn_short_nset(&glgame.port, port);
						bn_int_nset(&glgame.game_ip, addr);
						bn_int_set(&glgame.unknown4, SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
						bn_int_set(&glgame.unknown5, SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
						bn_int_set(&glgame.unknown6, SERVER_GAMELISTREPLY_GAME_UNKNOWN6);

						packet_append_data(rpacket, &glgame, sizeof(glgame));
						//here
						if (conn_is_new_client(game_get_owner(game)))
							packet_append_string(rpacket, game_get_name(game));
						else
							packet_append_string(rpacket, game_get_visibledname(game));
						packet_append_string(rpacket, game_get_pass(game));
						packet_append_string(rpacket, game_get_info(game));
						bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 1);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY specific game found", conn_get_socket(c));
						break;
					default:
						eventlog(eventlog_level_warn, __FUNCTION__, "[{}] game \"{}\" has bad status {}", conn_get_socket(c), game_get_name(game), game_get_status(game));
					}
				}
				else {
					bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 0);
					eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY specific game doesn't seem to exist", conn_get_socket(c));
				}
			}
			else {			/* list all public games of this type */
				struct glist_cbdata cbdata;

				if (gtype == game_type_all)
					eventlog(eventlog_level_debug, __FUNCTION__, "GAMELISTREPLY looking for public games tag=\"{}\" bngtype=0x{:08x} gtype=all", tag_uint_to_str(clienttag_str, clienttag), bngtype);
				else
					eventlog(eventlog_level_debug, __FUNCTION__, "GAMELISTREPLY looking for public games tag=\"{}\" bngtype=0x{:08x} gtype={}", tag_uint_to_str(clienttag_str, clienttag), bngtype, (int)gtype);

				cbdata.counter = 0;
				cbdata.tcount = 0;
				cbdata.c = c;
				cbdata.gtype = gtype;
				cbdata.rpacket = rpacket;
				gamelist_traverse(c, _glist_cb, &cbdata, gamelist_source_joinbutton);

				bn_int_set(&rpacket->u.server_gamelistreply.gamecount, cbdata.counter);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY sent {} of {} games", conn_get_socket(c), cbdata.counter, cbdata.tcount);
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_joingame(t_connection* c, t_packet const* const packet)
		{
			char const* gamename;
			char const* gamepass;
			t_game* game;
			t_game_type gtype;
			//	t_elem const * curr;
			//	t_game *currgame;
			//	t_connection * conn;

			if (packet_get_size(packet) < sizeof(t_client_join_game)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOIN_GAME packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_join_game), packet_get_size(packet));
				return -1;
			}

			if (!(gamename = packet_get_str_const(packet, sizeof(t_client_join_game), MAX_GAMENAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_JOIN_GAME (missing or too long gamename)", conn_get_socket(c));
				return -1;
			}

			if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_join_game) + std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_JOIN_GAME packet (missing or too long gamepass)", conn_get_socket(c));
				return -1;
			}



			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] trying to join game \"{}\" pass=\"{}\"", conn_get_socket(c), gamename, gamepass);

			if (conn_get_joingamewhisper_ack(c) == 0) {
				watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame);
				conn_set_joingamewhisper_ack(c, 1);	/* 1 = already whispered. We reset this each time user joins a channel */
				clanmember_on_change_status_by_connection(c);
			}

			if (conn_get_channel(c))
				conn_part_channel(c);

			if (!std::strcmp(gamename, "BNet") && !handle_anongame_join(c)) {
				gtype = game_type_anongame;
				gamename = NULL;
				return 0;		/* tmp: do not record any anongames as yet */
			}
			else {
				if (!(game = gamelist_find_game_available(gamename, conn_get_clienttag(c), game_type_all))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] unable to find game \"{}\" for user to join", conn_get_socket(c), gamename);
					return 0;
				}

				if (!game_check_access_for_player(game, c))
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] game join for \"{}\" to \"{}\" refused ( MMR LIMIT )", conn_get_socket(c), conn_get_username(c), gamename);
					/* If the user is not in a game, then map authorization
					will fail and keep them from playing. */
					return 0;
				}


				//here
				//conn_send_gamestats_to_player( c, c, game_get_statstype( game ) );


				//LIST_TRAVERSE_CONST( connlist( ), curr )
				//{
				//	conn = ( t_connection* )elem_get_data( curr );
				//	if ( conn && c != conn )
				//	{
				//		currgame = conn_get_game( conn );
				//		if ( currgame && currgame  == game )
				//		{
				//			conn_send_gamestats_to_player( conn, c, game_get_statstype( game ) );
				//			conn_send_gamestats_to_player( c, conn, game_get_statstype( game ) );
				//		}
				//	}

				//}



				gtype = game_get_type(game);
				gamename = game_get_name(game);
				if ((gtype == game_type_ladder && account_get_auth_joinladdergame(conn_get_account(c)) == 0) ||	/* default to true */
					(gtype != game_type_ladder && account_get_auth_joinnormalgame(conn_get_account(c)) == 0)) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] game join for \"{}\" to \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c), gamename);
					/* If the user is not in a game, then map authorization
					   will fail and keep them from playing. */
					return 0;
				}
			}


			if (conn_set_game(c, gamename, gamepass, "", gtype, STARTVER_UNKNOWN) < 0)
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" joined game \"{}\", but could not be recorded on server", conn_get_socket(c), conn_get_username(c), gamename);
			else
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" joined game \"{}\"", conn_get_socket(c), conn_get_username(c), gamename);

#ifdef WITH_LUA
			lua_handle_game(game, c, luaevent_game_userjoin);
#endif

			return 0;
		}

		static int _client_startgame1(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame1), packet_get_size(packet));
				return -1;
			}

			{
				char const* gamename;
				char const* gamepass;
				char const* gameinfo;
				unsigned short bngtype;
				unsigned int status;
				t_game* currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame1), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame1) + std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame1) + std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}


				bngtype = bn_short_get(packet->u.client_startgame1.gametype);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame1 status for game \"{}\" is 0x{:08x} (gametype = 0x{:04x})", conn_get_socket(c), gamename, bn_int_get(packet->u.client_startgame1.status), bngtype);
				status = bn_int_get(packet->u.client_startgame1.status) & CLIENT_STARTGAME1_STATUSMASK;

				if ((currgame = conn_get_game(c))) {
					switch (status) {
					case CLIENT_STARTGAME1_STATUS_STARTED:
						game_set_status(currgame, game_status_started);
						break;
					case CLIENT_STARTGAME1_STATUS_FULL:
						game_set_status(currgame, game_status_full);
						break;
					case CLIENT_STARTGAME1_STATUS_OPEN:
						game_set_status(currgame, game_status_open);
						break;
					case CLIENT_STARTGAME1_STATUS_DONE:
						game_set_status(currgame, game_status_done);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game \"{}\" is finished", conn_get_socket(c), gamename);
						break;
					}
				}
				else if (status != CLIENT_STARTGAME1_STATUS_DONE) {
					t_game_type gtype;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) ||	/* default to true */
						(gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))	/* default to true */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else
						conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW1);

					if ((rpacket = packet_create(packet_class_bnet))) {
						packet_set_size(rpacket, sizeof(t_server_startgame1_ack));
						packet_set_type(rpacket, SERVER_STARTGAME1_ACK);

						if (conn_get_game(c))
							bn_int_set(&rpacket->u.server_startgame1_ack.reply, SERVER_STARTGAME1_ACK_OK);
						else
							bn_int_set(&rpacket->u.server_startgame1_ack.reply, SERVER_STARTGAME1_ACK_NO);

						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status DONE to destroyed game", conn_get_socket(c));
			}

			return 0;
		}

		static int _client_startgame3(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame3), packet_get_size(packet));
				return -1;
			}

			{
				char const* gamename;
				char const* gamepass;
				char const* gameinfo;
				unsigned short bngtype;
				unsigned int status;
				t_game* currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame3), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame3) + std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame3) + std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}
				bngtype = bn_short_get(packet->u.client_startgame3.gametype);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame3 status for game \"{}\" is 0x{:08x} (gametype = 0x{:04x})", conn_get_socket(c), gamename, bn_int_get(packet->u.client_startgame3.status), bngtype);
				status = bn_int_get(packet->u.client_startgame3.status) & CLIENT_STARTGAME3_STATUSMASK;

				if ((currgame = conn_get_game(c))) {
					switch (status) {
					case CLIENT_STARTGAME3_STATUS_STARTED:
						game_set_status(currgame, game_status_started);
						break;
					case CLIENT_STARTGAME3_STATUS_FULL:
						game_set_status(currgame, game_status_full);
						break;
					case CLIENT_STARTGAME3_STATUS_OPEN1:
					case CLIENT_STARTGAME3_STATUS_OPEN:
						game_set_status(currgame, game_status_open);
						break;
					case CLIENT_STARTGAME3_STATUS_DONE:
						game_set_status(currgame, game_status_done);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game \"{}\" is finished", conn_get_socket(c), gamename);
						break;
					}
				}
				else if (status != CLIENT_STARTGAME3_STATUS_DONE) {
					t_game_type gtype;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) || (gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else
						conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW3);

					if ((rpacket = packet_create(packet_class_bnet))) {
						packet_set_size(rpacket, sizeof(t_server_startgame3_ack));
						packet_set_type(rpacket, SERVER_STARTGAME3_ACK);

						if (conn_get_game(c))
							bn_int_set(&rpacket->u.server_startgame3_ack.reply, SERVER_STARTGAME3_ACK_OK);
						else
							bn_int_set(&rpacket->u.server_startgame3_ack.reply, SERVER_STARTGAME3_ACK_NO);

						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status DONE to destroyed game", conn_get_socket(c));
			}

			return 0;
		}

		static int _client_startgame4(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame4)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame4), packet_get_size(packet));
				return -1;
			}

			if (conn_get_clienttag(c) == CLIENTTAG_STARCRAFT_UINT || conn_get_clienttag(c) == CLIENTTAG_BROODWARS_UINT)
			{
				// FIXME: (HarpyWar) Protection from hack attempt
				// Large map name size will cause crash Starcraft client for user who select an item in game list ("Join" area)
				// It occurs when the packet size of packet 0x0c in length interval 161-164
				// https://github.com/pvpgn/pvpgn-server/issues/159
				if (packet_get_size(packet) > 160)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got abnormal STARTGAME4 packet length (got {} bytes, hack attempt?)", conn_get_socket(c), packet_get_size(packet));
					return -1;
				}
			}

			// Quick hack to make W3 part channels when creating a game
			if (conn_get_channel(c))
				conn_part_channel(c);

			{
				char const* gamename;
				char const* gamepass;
				char const* gameinfo;
				unsigned short bngtype;
				unsigned int status;
				unsigned int flag;
				unsigned short option;
				t_game* currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame4), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame4) + std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame4) + std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}
				bngtype = bn_short_get(packet->u.client_startgame4.gametype);
				option = bn_short_get(packet->u.client_startgame4.option);
				status = bn_int_get(packet->u.client_startgame4.status);
				flag = bn_short_get(packet->u.client_startgame4.flag);

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame4 status for game \"{}\" is 0x{:08x} (gametype=0x{:04x} option=0x{:04x}, flag=0x{:04x})", conn_get_socket(c), gamename, status, bngtype, option, flag);
				if ((currgame = conn_get_game(c))) {
					if ((status & CLIENT_STARTGAME4_STATUSMASK_OPEN_VALID) == status) {
						if (status & CLIENT_STARTGAME4_STATUS_START)
							game_set_status(currgame, game_status_started);
						else if (status & CLIENT_STARTGAME4_STATUS_FULL)
							game_set_status(currgame, game_status_full);
						else
							game_set_status(currgame, game_status_open);
					}
					else {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown startgame4 status {} (clienttag: {})", conn_get_socket(c), status, clienttag_uint_to_str(conn_get_clienttag(c)));
					}

				}
				else if ((status & CLIENT_STARTGAME4_STATUSMASK_INIT_VALID) == status) {
					/*valid creation status would be:
					   0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x80, 0x81, 0x82, 0x83 */

					t_game_type gtype;
					bool allow_create_custom = false;
					t_game* game;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) || (gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else
					{
						//find is there any existing game with same name and allow the host to create game
						// with same name only when another game is already started or already done
						if ((!(game = gamelist_find_game_available(gamename, conn_get_clienttag(c), game_type_all))) &&
							(conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW4) == 0))
						{
							game_set_option(conn_get_game(c), bngoption_to_goption(conn_get_clienttag(c), gtype, option));
							if (status & CLIENT_STARTGAME4_STATUS_PRIVATE)
								game_set_flag(conn_get_game(c), game_flag_private);
							if (status & CLIENT_STARTGAME4_STATUS_FULL)
								game_set_status(conn_get_game(c), game_status_full);
							if (bngtype == CLIENT_GAMELISTREQ_LOADED) /* PELISH: seems strange but it is really needed for loaded games */
								game_set_status(conn_get_game(c), game_status_loaded);
							//FIXME: still need special handling for status disc-is-loss and replay

						}
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status 0x{:x} to unexistent game (clienttag: {})", conn_get_socket(c), status, clienttag_uint_to_str(conn_get_clienttag(c)));
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_startgame4_ack));
				packet_set_type(rpacket, SERVER_STARTGAME4_ACK);

				if (conn_get_game(c))
					bn_int_set(&rpacket->u.server_startgame4_ack.reply, SERVER_STARTGAME4_ACK_OK);
				else
					bn_int_set(&rpacket->u.server_startgame4_ack.reply, SERVER_STARTGAME4_ACK_NO);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			/* First, send an ECHO_REQ */
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_echoreq));
				packet_set_type(rpacket, SERVER_ECHOREQ);
				bn_int_set(&rpacket->u.server_echoreq.ticks, get_ticks());
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_closegame(t_connection* c, t_packet const* const packet)
		{
			t_game* game;

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] client closing game", conn_get_socket(c));
			if (packet_get_type(packet) == CLIENT_CLOSEGAME2 || ((conn_get_clienttag(c) != CLIENTTAG_WARCRAFT3_UINT) && (conn_get_clienttag(c) != CLIENTTAG_WAR3XP_UINT)))
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);
			else if ((game = conn_get_game(c)))
				game_set_status(game, game_status_started);

			return 0;
		}

		static int _client_gamereport(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_game_report)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAME_REPORT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_game_report), packet_get_size(packet));
				return -1;
			}

			{
				t_account* my_account;
				t_account* other_account;
				t_game* game;
				unsigned int player_count;
				unsigned int i, s;
				t_client_game_report_result const* result_data;
				unsigned int result_off;
				t_game_result result;
				char const* player;
				unsigned int player_off;
				t_game_result* results;

				player_count = bn_int_get(packet->u.client_gamerep.count);

				if (!(game = conn_get_game(c))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT when not in a game for user \"{}\"", conn_get_socket(c), conn_get_username(c));
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_GAME_REPORT: {} ({} players)", conn_get_socket(c), conn_get_username(c), player_count);
				my_account = conn_get_account(c);

				results = (t_game_result*)xmalloc(sizeof(t_game_result) * game_get_count(game));

				for (i = 0; i < game_get_count(game); i++)
					results[i] = game_result_none;

				for (i = 0, result_off = sizeof(t_client_game_report), player_off = sizeof(t_client_game_report) + player_count * sizeof(t_client_game_report_result); i < player_count; i++, result_off += sizeof(t_client_game_report_result), player_off += std::strlen(player) + 1) {
					/* PELISH: Fixme - Can this crash server (NULL pointer dereferencing)?? */
					if (!(result_data = (const t_client_game_report_result*)packet_get_data_const(packet, result_off, sizeof(t_client_game_report_result)))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got corrupt GAME_REPORT packet (missing results {}-{})", conn_get_socket(c), i + 1, player_count);
						break;
					}
					if (!(player = packet_get_str_const(packet, player_off, MAX_USERNAME_LEN))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got corrupt GAME_REPORT packet (missing players {}-{})", conn_get_socket(c), i + 1, player_count);
						break;
					}

					if (player[0] == '\0')	/* empty slots have empty player name */
						continue;

					if (i >= game_get_count(game)) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got more results than the game had players - ignoring extra results", conn_get_socket(c));
						break;
					}

					if (!(other_account = accountlist_find_account(player))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with unknown player \"{}\"", conn_get_socket(c), player);
						break;
					}

					// as player position in game structure and in game report might differ,
					// search for right position
					for (s = 0; s < game_get_count(game); s++)
					{
						if (game_get_player(game, s) == other_account) break;
					}

					if (s < game_get_count(game))
					{
						result = bngresult_to_gresult(bn_int_get(result_data->result));
						results[s] = result;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got player {} (\"{}\") result {}", conn_get_socket(c), i, player, game_result_get_str(result));
					}
					else
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT for non-participating player \"{}\"", conn_get_socket(c), player);
					}


				}

				if (i == player_count) {	/* if everything checked out... */
					char const* head;
					char const* body;

					if (!(head = packet_get_str_const(packet, player_off, MAX_GAMEREP_HEAD_STR)))
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with missing or too long report head", conn_get_socket(c));
					else {
						player_off += std::strlen(head) + 1;
						if (!(body = packet_get_str_const(packet, player_off, MAX_GAMEREP_BODY_STR)))
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with missing or too ling report body", conn_get_socket(c));
						else
							game_set_report(game, my_account, head, body);
					}
				}

				if (game_set_reported_results(game, my_account, results) < 0)
					xfree((void*)results);

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] finished parsing result... now leaving game", conn_get_socket(c));
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);
			}

			return 0;
		}

		static int _client_leavechannel(t_connection* c, t_packet const* const packet)
		{
			/* If this user in a channel, notify everyone that the user has left */
			if (conn_get_channel(c))
				conn_part_channel(c);
			return 0;
		}

		static int _client_ladderreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;


			if (packet_get_size(packet) < sizeof(t_client_ladderreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_ladderreq), packet_get_size(packet));
				return -1;
			}

			{
				t_ladder_entry entry;
				unsigned int i;
				unsigned int type;
				unsigned int start;
				unsigned int count;
				unsigned int idnum;
				t_account* account;
				t_clienttag clienttag;
				char const* timestr;
				t_bnettime bt;
				t_ladder_id id;
				t_ladder_sort sort;

				clienttag = conn_get_clienttag(c);

				type = bn_int_get(packet->u.client_ladderreq.type);
				start = bn_int_get(packet->u.client_ladderreq.startplace);
				count = bn_int_get(packet->u.client_ladderreq.count);
				idnum = bn_int_get(packet->u.client_ladderreq.id);

				/* eventlog(eventlog_level_debug,__FUNCTION__,"got LADDERREQ type=%u start=%u count=%u id=%u",type,start,count,id); */

				switch (idnum) {
				case CLIENT_LADDERREQ_ID_STANDARD:
					id = ladder_id_normal;
					break;
				case CLIENT_LADDERREQ_ID_IRONMAN:
					id = ladder_id_ironman;
					break;
				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder ladderreq.id=0x{:08x}", conn_get_socket(c), idnum);
					id = ladder_id_normal;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_ladderreply));
				packet_set_type(rpacket, SERVER_LADDERREPLY);

				bn_int_set(&rpacket->u.server_ladderreply.clienttag, clienttag);
				bn_int_set(&rpacket->u.server_ladderreply.id, idnum);
				bn_int_set(&rpacket->u.server_ladderreply.type, type);
				bn_int_set(&rpacket->u.server_ladderreply.startplace, start);

				switch (type) {
				case CLIENT_LADDERREQ_TYPE_HIGHESTRATED:
					sort = ladder_sort_highestrated;
					break;
				case CLIENT_LADDERREQ_TYPE_MOSTWINS:
					sort = ladder_sort_mostwins;
					break;
				case CLIENT_LADDERREQ_TYPE_MOSTGAMES:
					sort = ladder_sort_mostgames;
					break;
				default:
					sort = ladder_sort_default;
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown value for ladderreq.type={}", conn_get_socket(c), type);
				}

				LadderList* ladderList_active = ladders.getLadderList(LadderKey(id, clienttag, sort, ladder_time_active));
				LadderList* ladderList_current = ladders.getLadderList(LadderKey(id, clienttag, sort, ladder_time_current));

				for (i = start; i < start + count; i++) {
					const LadderReferencedObject* referencedObject = NULL;

					if (ladderList_active)
					{
						referencedObject = ladderList_active->getReferencedObject(i + 1);
					}

					if (!referencedObject && ladderList_current)
					{
						referencedObject = ladderList_current->getReferencedObject(i + 1);
					}

					account = NULL;
					if ((referencedObject) && (account = referencedObject->getAccount()))
					{
						bn_int_set(&entry.active.wins, account_get_ladder_active_wins(account, clienttag, id));
						bn_int_set(&entry.active.loss, account_get_ladder_active_losses(account, clienttag, id));
						bn_int_set(&entry.active.disconnect, account_get_ladder_active_disconnects(account, clienttag, id));
						bn_int_set(&entry.active.rating, account_get_ladder_active_rating(account, clienttag, id));
						bn_int_set(&entry.active.rank, account_get_ladder_active_rank(account, clienttag, id) - 1);
						if (!(timestr = account_get_ladder_active_last_time(account, clienttag, id)))
							timestr = BNETD_LADDER_DEFAULT_TIME;
						bnettime_set_str(&bt, timestr);
						bnettime_to_bn_long(bt, &entry.lastgame_active);

						bn_int_set(&entry.current.wins, account_get_ladder_wins(account, clienttag, id));
						bn_int_set(&entry.current.loss, account_get_ladder_losses(account, clienttag, id));
						bn_int_set(&entry.current.disconnect, account_get_ladder_disconnects(account, clienttag, id));
						bn_int_set(&entry.current.rating, account_get_ladder_rating(account, clienttag, id));
						bn_int_set(&entry.current.rank, account_get_ladder_rank(account, clienttag, id) - 1);
						if (!(timestr = account_get_ladder_last_time(account, clienttag, id)))
							timestr = BNETD_LADDER_DEFAULT_TIME;
						bnettime_set_str(&bt, timestr);
						bnettime_to_bn_long(bt, &entry.lastgame_current);
					}
					else {
						bn_int_set(&entry.active.wins, 0);
						bn_int_set(&entry.active.loss, 0);
						bn_int_set(&entry.active.disconnect, 0);
						bn_int_set(&entry.active.rating, 0);
						bn_int_set(&entry.active.rank, 0);
						bn_long_set_a_b(&entry.lastgame_active, 0, 0);

						bn_int_set(&entry.current.wins, 0);
						bn_int_set(&entry.current.loss, 0);
						bn_int_set(&entry.current.disconnect, 0);
						bn_int_set(&entry.current.rating, 0);
						bn_int_set(&entry.current.rank, 0);
						bn_long_set_a_b(&entry.lastgame_current, 0, 0);
					}

					bn_int_set(&entry.ttest[0], i);	// rank
					bn_int_set(&entry.ttest[1], 0);	//
					bn_int_set(&entry.ttest[2], 0);	//
					if (account)
						bn_int_set(&entry.ttest[3], account_get_ladder_high_rating(account, clienttag, id));
					else
						bn_int_set(&entry.ttest[3], 0);
					bn_int_set(&entry.ttest[4], 0);	//
					bn_int_set(&entry.ttest[5], 0);	//

					packet_append_data(rpacket, &entry, sizeof(entry));

					if (account)
						packet_append_string(rpacket, account_get_name(account));
					else
						packet_append_string(rpacket, " ");	/* use a space so the client won't show the user's own account when double-clicked on */
				}

				bn_int_set(&rpacket->u.server_ladderreply.count, i - start);

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_laddersearchreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_laddersearchreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERSEARCHREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_laddersearchreq), packet_get_size(packet));
				return -1;
			}

			{
				char const* playername;
				t_account* account;
				unsigned int idnum;
				unsigned int type;
				unsigned int rank;	/* starts at zero */
				t_ladder_id id;
				t_ladder_sort sort;
				t_clienttag ctag = conn_get_clienttag(c);

				idnum = bn_int_get(packet->u.client_laddersearchreq.id);

				switch (idnum) {
				case CLIENT_LADDERREQ_ID_STANDARD:
					id = ladder_id_normal;
					break;
				case CLIENT_LADDERREQ_ID_IRONMAN:
					id = ladder_id_ironman;
					break;
				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder laddersearchreq.id=0x{:08x}", conn_get_socket(c), idnum);
					id = ladder_id_normal;
				}

				type = bn_int_get(packet->u.client_laddersearchreq.type);
				switch (type) {
				case CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED:
					sort = ladder_sort_highestrated;
					break;
				case CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS:
					sort = ladder_sort_mostwins;
					break;
				case CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES:
					sort = ladder_sort_mostgames;
					break;
				default:
					sort = ladder_sort_default;
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder search type {}", conn_get_socket(c), type);
				}

				if (!(playername = packet_get_str_const(packet, sizeof(t_client_laddersearchreq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERSEARCHREQ packet (missing or too long playername)", conn_get_socket(c));
					return -1;
				}

				if (!(account = accountlist_find_account(playername)))
					rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
				else {
					LadderList* ladderList_active = ladders.getLadderList(LadderKey(id, ctag, sort, ladder_time_active));
					LadderList* ladderList_current = ladders.getLadderList(LadderKey(id, ctag, sort, ladder_time_current));
					unsigned int uid = account_get_uid(account);
					switch (type) {
					case CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED:
					case CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS:
					case CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES:
						if (!(rank = ladderList_active->getRank(uid)))
						{
							if (!(rank = ladderList_current->getRank(uid)) || ladderList_active->getReferencedObject(rank))
								rank = 0;
						}
						break;
					default:
						rank = 0;
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder search type {}", conn_get_socket(c), bn_int_get(packet->u.client_laddersearchreq.type));
					}

					if (rank == 0)
						rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
					else
						rank--;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_laddersearchreply));
				packet_set_type(rpacket, SERVER_LADDERSEARCHREPLY);
				bn_int_set(&rpacket->u.server_laddersearchreply.rank, rank);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_mapauthreq1(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_mapauthreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_mapauthreq1), packet_get_size(packet));
				return -1;
			}

			{
				char const* mapname;
				t_game* game;

				if (!(mapname = packet_get_str_const(packet, sizeof(t_client_mapauthreq1), MAP_NAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ1 packet (missing or too long mapname)", conn_get_socket(c));
					return -1;
				}

				game = conn_get_game(c);

				if (game) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested for map \"{}\" gametype \"{}\"", conn_get_socket(c), mapname, game_type_get_str(game_get_type(game)));
					game_set_mapname(game, mapname);
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested when not in a game", conn_get_socket(c));

				if ((rpacket = packet_create(packet_class_bnet))) {
					unsigned int val;

					if (!game) {
						val = SERVER_MAPAUTHREPLY1_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (not in a game)", conn_get_socket(c));
					}
					else if (strcasecmp(game_get_mapname(game), mapname) != 0) {
						val = SERVER_MAPAUTHREPLY1_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (map name \"{}\" does not match game map name \"{}\")", conn_get_socket(c), mapname, game_get_mapname(game));
					}
					else {
						game_set_status(game, game_status_started);

						if (game_get_type(game) == game_type_ladder) {
							val = SERVER_MAPAUTHREPLY1_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (in a ladder game)", conn_get_socket(c));
						}
						else if (ladder_check_map(game_get_mapname(game), game_get_maptype(game), conn_get_clienttag(c))) {
							val = SERVER_MAPAUTHREPLY1_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (is a ladder map)", conn_get_socket(c));
						}
						else {
							val = SERVER_MAPAUTHREPLY1_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map normal authorization", conn_get_socket(c));
						}
					}

					packet_set_size(rpacket, sizeof(t_server_mapauthreply1));
					packet_set_type(rpacket, SERVER_MAPAUTHREPLY1);
					bn_int_set(&rpacket->u.server_mapauthreply1.response, val);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_mapauthreq2(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_mapauthreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_mapauthreq2), packet_get_size(packet));
				return -1;
			}

			{
				char const* mapname;
				t_game* game;

				if (!(mapname = packet_get_str_const(packet, sizeof(t_client_mapauthreq2), MAP_NAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ2 packet (missing or too long mapname)", conn_get_socket(c));
					return -1;
				}

				game = conn_get_game(c);

				if (game) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested for map \"{}\" gametype \"{}\"", conn_get_socket(c), mapname, game_type_get_str(game_get_type(game)));
					game_set_mapname(game, mapname);
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested when not in a game", conn_get_socket(c));

				if ((rpacket = packet_create(packet_class_bnet))) {
					unsigned int val;

					if (!game) {
						val = SERVER_MAPAUTHREPLY2_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (not in a game)", conn_get_socket(c));
					}
					else if (strcasecmp(game_get_mapname(game), mapname) != 0) {
						val = SERVER_MAPAUTHREPLY2_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (map name \"{}\" does not match game map name \"{}\")", conn_get_socket(c), mapname, game_get_mapname(game));
					}
					else {
						game_set_status(game, game_status_started);

						if (game_get_type(game) == game_type_ladder) {
							val = SERVER_MAPAUTHREPLY2_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (in a ladder game)", conn_get_socket(c));
						}
						else if (ladder_check_map(game_get_mapname(game), game_get_maptype(game), conn_get_clienttag(c))) {
							val = SERVER_MAPAUTHREPLY2_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (is a ladder map)", conn_get_socket(c));
						}
						else {
							val = SERVER_MAPAUTHREPLY2_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map normal authorization", conn_get_socket(c));
						}
					}

					packet_set_size(rpacket, sizeof(t_server_mapauthreply2));
					packet_set_type(rpacket, SERVER_MAPAUTHREPLY2);
					bn_int_set(&rpacket->u.server_mapauthreply2.response, val);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_changeclient(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_changeclient))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CHANGECLIENT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_changeclient), packet_get_size(packet));
				return -1;
			}

			if (conn_get_clienttag(c) != CLIENTTAG_WAR3XP_UINT
				|| bn_int_get(packet->u.client_changeclient.clienttag) != CLIENTTAG_WARCRAFT3_UINT)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid attempt to change client from {} to {}", conn_get_socket(c), conn_get_clienttag(c), bn_int_get(packet->u.client_changeclient.clienttag));
				conn_set_state(c, conn_state_destroy);
				return -1;
			}

			conn_set_clienttag(c, bn_int_get(packet->u.client_changeclient.clienttag));

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] changed client to {}", conn_get_socket(c), bn_int_get(packet->u.client_changeclient.clienttag));

			return 0;
		}

		static int _client_clanmemberlistreq(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_clanmemberlist_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBERLIST_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmemberlist_req), packet_get_size(packet));
				return -1;
			}

			clan_send_memberlist(c, packet);
			return 0;
		}

		static int _client_clan_motdreq(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_clan_motdreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_MOTDREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_motdreq), packet_get_size(packet));
				return -1;
			}

			clan_send_motd_reply(c, packet);
			return 0;
		}

		static int _client_clan_motdchg(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_clan_motdreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_MOTDCHGREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_motdreq), packet_get_size(packet));
				return -1;
			}

			clan_save_motd_chg(c, packet);
			return 0;
		}

		static int _client_clan_disbandreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clan_disbandreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_DISBANDREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_disbandreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				t_clan* clan;
				t_clanmember* member;
				t_account* account;

				packet_set_size(rpacket, sizeof(t_server_clan_disbandreply));
				packet_set_type(rpacket, SERVER_CLAN_DISBANDREPLY);
				bn_int_set(&rpacket->u.server_clan_disbandreply.count, bn_int_get(packet->u.client_clan_disbandreq.count));

				if (!((account = conn_get_account(c)) && (clan = account_get_clan(account)) && (member = account_get_clanmember(account)) && (clanmember_get_status(member) >= CLAN_CHIEFTAIN))) {
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_DISBANDREQ packet (request without required privileges)", conn_get_socket(c));
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_NOT_AUTHORIZED);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
				else if ((clanlist_remove_clan(clan) == 0) && (clan_remove(clan_get_clantag(clan)) == 0)) {
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_SUCCESS);
					clan_close_status_window_on_disband(clan);
					clan_send_packet_to_online_members(clan, rpacket);
					packet_del_ref(rpacket);
					clan_destroy(clan);
				}
				else {
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_FAIL);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_clan_createreq(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_clan_createreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}

			clan_get_possible_member(c, packet);

			return 0;
		}

		static int _client_clan_createinvitereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			unsigned size;
			const char* clanname;
			const char* username;
			t_clantag clantag;
			unsigned offset;
			t_clan* clan;

			if ((size = packet_get_size(packet)) < sizeof(t_client_clan_createinvitereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createinvitereq), packet_get_size(packet));
				return -1;
			}
			offset = sizeof(t_client_clan_createinvitereq);

			if (!(clanname = packet_get_str_const(packet, offset, CLAN_NAME_MAX))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing clanname)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(clanname) + 1);

			if (packet_get_size(packet) < offset + 4) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing clantag)", conn_get_socket(c));
				return -1;
			}
			clantag = *((int*)packet_get_data_const(packet, offset, 4));
			offset += 4;

			if ((rpacket = packet_create(packet_class_bnet))) {
				if ((clan = clan_create(conn_get_account(c), clantag, clanname, NULL)) && clanlist_add_clan(clan)) {
					char membercount;
					if (packet_get_size(packet) < offset + 1) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing membercount)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}
					membercount = *((char*)packet_get_data_const(packet, offset, 1));
					clan_set_created(clan, -membercount);                               /* FIXME: We should also check if membercount == count of names on end of this packet */
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereq));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREQ);
					bn_int_set(&rpacket->u.server_clan_createinvitereq.count, bn_int_get(packet->u.client_clan_createinvitereq.count));
					bn_int_set(&rpacket->u.server_clan_createinvitereq.clantag, clantag);
					packet_append_string(rpacket, clanname);
					packet_append_string(rpacket, conn_get_username(c));
					packet_append_data(rpacket, packet_get_data_const(packet, offset, size - offset), size - offset); /* Pelish: we will send bad packet if we got bad packet... */
					offset++;
					do {
						username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
						if (username) {
							t_connection* conn;
							offset += (std::strlen(username) + 1);
							if ((conn = connlist_find_connection_by_accountname(username)) != NULL) {
								t_clanmember* clanmember;
								if (prefs_get_clan_newer_time() > 0) {
									clanmember = clan_add_member(clan, conn_get_account(conn), CLAN_NEW);
									clanmember_set_fullmember(clanmember, 1);      /* FIXME: do only this here and no clan_add_member() */
								}
								else {
									clanmember = clan_add_member(clan, conn_get_account(conn), CLAN_PEON);
									clanmember_set_fullmember(clanmember, 1);      /* FIXME: do only this here and no clan_add_member() */
								}
								conn_push_outqueue(conn, rpacket);
							}
						}
					} while (username && (offset < size));
				}
				else {
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
					bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
					bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, 0);
				}
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_clan_createinvitereply(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_connection* conn;
			t_clan* clan;
			const char* username;
			char status;

			if (packet_get_size(packet) < sizeof(t_client_clan_createinvitereply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createinvitereq), packet_get_size(packet));
				return -1;
			}
			std::size_t offset = sizeof(t_client_clan_createinvitereply);
			username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
			if (!username)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (bad username)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(username) + 1);
			if (packet_get_size(packet) < offset + 1) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (missing status)", conn_get_socket(c));
				return -1;
			}
			status = *((char*)packet_get_data_const(packet, offset, 1));
			if ((conn = connlist_find_connection_by_accountname(username)) == NULL)
				return -1;
			if ((clan = account_get_creating_clan(conn_get_account(conn))) == NULL)
				return -1;
			if ((status != CLAN_RESPONSE_ACCEPT) && (rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
				packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
				bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
				bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, status);
				packet_append_string(rpacket, conn_get_username(c));
				conn_push_outqueue(conn, rpacket);
				packet_del_ref(rpacket);
				if (clan) {
					clanlist_remove_clan(clan);
					clan_destroy(clan);
				}
			}
			else {
				int created = clan_get_created(clan);
				if (created > 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "clan {} has already been created", clan_get_name(clan));
					return 0;
				}
				created++;
				if ((created >= 0) && (rpacket = packet_create(packet_class_bnet))) {
					clan_set_created(clan, 1);
					clan_set_creation_time(clan, std::time(NULL));
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
					bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
					bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, CLAN_RESPONSE_SUCCESS);
					packet_append_string(rpacket, "");
					conn_push_outqueue(conn, rpacket);
					packet_del_ref(rpacket);
					clan_send_status_window_on_create(clan);
					clan_save(clan);
				}
				else
					clan_set_created(clan, created);
			}
			return 0;
		}

		static int _client_clanmember_rankupdatereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clanmember_rankupdate_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_RANKUPDATE_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmember_rankupdate_req), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL)
			{
				std::size_t offset = sizeof(t_client_clanmember_rankupdate_req);
				char status;
				t_clan* clan;
				t_clanmember* dest_member;
				t_clanmember* member;
				t_account* account;

				packet_set_size(rpacket, sizeof(t_server_clanmember_rankupdate_reply));
				packet_set_type(rpacket, SERVER_CLANMEMBER_RANKUPDATE_REPLY);
				bn_int_set(&rpacket->u.server_clanmember_rankupdate_reply.count,
					bn_int_get(packet->u.client_clanmember_rankupdate_req.count));
				const char* const username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
				if (!username)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] Could not retrieve username from CLANMEMBER_RANKUPDATE_REQ packet", conn_get_socket(c));
					packet_del_ref(rpacket);
					return -1;
				}

				offset += (std::strlen(username) + 1);
				if (packet_get_size(packet) < offset + 1)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_RANKUPDATE_REQ packet (missing status)", conn_get_socket(c));
					packet_del_ref(rpacket);
					return -1;
				}
				status = *((char*)packet_get_data_const(packet, offset, 1));

				account = (conn_get_account(c));

				if ((clan = account_get_clan(account)) && (member = clan_find_member(clan, account))
					&& (dest_member = clan_find_member_by_name(clan, username)) && (member != dest_member)) {
					if ((status < CLAN_PEON) || (status > CLAN_SHAMAN)) {
						/* PELISH: CLAN_NEW can not be promoted to anything
						 * and also noone can be promoted to CLAN_CHIEFTAIN */
						DEBUG1("trying to change to bad status {}", status);
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
					}
					else if ((((clanmember_get_status(member) == CLAN_SHAMAN) && (status < CLAN_SHAMAN) && (clanmember_get_status(dest_member) < CLAN_SHAMAN)) ||
						(clanmember_get_status(member) == CLAN_CHIEFTAIN)) &&
						(clanmember_set_status(dest_member, status) == 0)) {
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_SUCCESS);
						clanmember_on_change_status(dest_member);
					}
					else {
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
					}
				}
				else {
					bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_clanmember_removereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clanmember_remove_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_REMOVE_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmember_remove_req), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL) {
				t_account* acc;
				t_clan* clan;
				const char* username;
				t_clanmember* member;
				t_connection* dest_conn;
				packet_set_size(rpacket, sizeof(t_server_clanmember_remove_reply));
				packet_set_type(rpacket, SERVER_CLANMEMBER_REMOVE_REPLY);
				bn_int_set(&rpacket->u.server_clanmember_remove_reply.count,
					bn_int_get(packet->u.client_clanmember_remove_req.count));
				username = packet_get_str_const(packet, sizeof(t_client_clanmember_remove_req), MAX_USERNAME_LEN);
				bn_byte_set(&rpacket->u.server_clanmember_remove_reply.result,
					SERVER_CLANMEMBER_REMOVE_FAILED); // initially presume it failed

				if ((acc = conn_get_account(c)) && (clan = account_get_clan(acc)) && (member = clan_find_member_by_name(clan, username))) {
					dest_conn = clanmember_get_conn(member);
					if (clan_remove_member(clan, member) == 0) {
						t_packet* rpacket2;
						if (dest_conn) {
							clan_close_status_window(dest_conn);
							channel_rejoin(dest_conn);
						}
						if ((rpacket2 = packet_create(packet_class_bnet)) != NULL) {
							packet_set_size(rpacket2, sizeof(t_server_clanmember_removed_notify));
							packet_set_type(rpacket2, SERVER_CLANMEMBER_REMOVED_NOTIFY);
							packet_append_string(rpacket2, username);
							clan_send_packet_to_online_members(clan, rpacket2);
							packet_del_ref(rpacket2);
						}
						bn_byte_set(&rpacket->u.server_clanmember_remove_reply.result,
							SERVER_CLANMEMBER_REMOVE_SUCCESS);
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_clan_membernewchiefreq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clan_membernewchiefreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CLAN_MEMBERNEWCHIEFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL) {
				t_account* acc;
				t_clan* clan;
				t_clanmember* oldmember;
				t_clanmember* newmember;
				const char* username;
				packet_set_size(rpacket, sizeof(t_server_clan_membernewchiefreply));
				packet_set_type(rpacket, SERVER_CLAN_MEMBERNEWCHIEFREPLY);
				bn_int_set(&rpacket->u.server_clan_membernewchiefreply.count, bn_int_get(packet->u.client_clan_membernewchiefreq.count));
				username = packet_get_str_const(packet, sizeof(t_client_clan_membernewchiefreq), MAX_USERNAME_LEN);
				if ((acc = conn_get_account(c)) && (oldmember = account_get_clanmember(acc)) && (clanmember_get_status(oldmember) == CLAN_CHIEFTAIN) && (clan = clanmember_get_clan(oldmember)) && (newmember = clan_find_member_by_name(clan, username)) && (clanmember_set_status(oldmember, CLAN_GRUNT) == 0) && (clanmember_set_status(newmember, CLAN_CHIEFTAIN) == 0)) {
					clanmember_on_change_status(oldmember);
					clanmember_on_change_status(newmember);
					bn_byte_set(&rpacket->u.server_clan_membernewchiefreply.result, SERVER_CLAN_MEMBERNEWCHIEFREPLY_SUCCESS);
					clan_send_packet_to_online_members(clan, rpacket);
					packet_del_ref(rpacket);
				}
				else {
					bn_byte_set(&rpacket->u.server_clan_membernewchiefreply.result, SERVER_CLAN_MEMBERNEWCHIEFREPLY_FAILED);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		static int _client_clan_invitereq(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_account* account;
			t_clan* clan;
			t_clanmember* member;
			t_clantag clantag;
			const char* username;
			t_connection* conn;
			t_account* conn_account;
			char response_code;

			if (packet_get_size(packet) < sizeof(t_client_clan_invitereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_invitereq), packet_get_size(packet));
				return -1;
			}

			if (!(username = packet_get_str_const(packet, sizeof(t_client_clan_invitereq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREQ packet (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (rpacket = packet_create(packet_class_bnet)) {

				// user not authorized
				if (!((account = conn_get_account(c)) &&
					(clan = account_get_clan(account)) &&
					(member = account_get_clanmember(account)) &&
					(clanmember_get_status(member) >= CLAN_SHAMAN) &&
					(clantag = clan_get_clantag(clan)))) {
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREQ packet (request without required privileges)", conn_get_socket(c));
					response_code = CLAN_RESPONSE_NOT_AUTHORIZED;
				}
				else {

					// target user not online
					if (!((conn = connlist_find_connection_by_accountname(username)) &&
						(conn_account = conn_get_account(conn)))) {
						response_code = CLAN_RESPONSE_NOT_FOUND;

						// target user already in a clan or creating a clan
					}
					else if (account_get_clanmember_forced(conn_get_account(conn))) {
						response_code = CLAN_RESPONSE_NOT_FOUND;

						// clan already ful
					}
					else if (clan_get_member_count(clan) >= prefs_get_clan_max_members()) {
						response_code = CLAN_RESPONSE_CLAN_FULL;

						// valid invitereq
					}
					else {
						if (prefs_get_clan_newer_time() > 0) {
							clan_add_member(clan, conn_account, CLAN_NEW);
						}
						else {
							clan_add_member(clan, conn_account, CLAN_PEON);
						}
						packet_set_size(rpacket, sizeof(t_server_clan_invitereq));
						packet_set_type(rpacket, SERVER_CLAN_INVITEREQ);
						bn_int_set(&rpacket->u.server_clan_invitereq.count, bn_int_get(packet->u.client_clan_invitereq.count));
						bn_int_set(&rpacket->u.server_clan_invitereq.clantag, clantag);
						packet_append_string(rpacket, clan_get_name(clan));
						packet_append_string(rpacket, conn_get_username(c));
						conn_push_outqueue(conn, rpacket);
						packet_del_ref(rpacket);
						return 0;
					}
				}

				packet_set_size(rpacket, sizeof(t_server_clan_invitereply));
				packet_set_type(rpacket, SERVER_CLAN_INVITEREPLY);
				bn_byte_set(&rpacket->u.server_clan_invitereply.result, response_code);
				bn_int_set(&rpacket->u.server_clan_invitereply.count, bn_int_get(packet->u.client_clan_invitereq.count));

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		static int _client_clan_invitereply(t_connection* c, t_packet const* const packet)
		{
			t_packet* rpacket;
			t_account* acc;
			t_clan* clan;
			t_clanmember* member;
			const char* username;
			t_connection* conn;
			t_account* conn_account;
			t_clan* conn_clan;
			t_clanmember* conn_member;
			char status;

			if (packet_get_size(packet) < sizeof(t_client_clan_invitereply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}
			std::size_t offset = sizeof(t_client_clan_invitereply);
			if (!(username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (missing username)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(username) + 1);
			if (packet_get_size(packet) < offset + 1) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (missing status)", conn_get_socket(c));
				return -1;
			}
			status = *((char*)packet_get_data_const(packet, offset, 1));

			// reply without prior request
			if (!((acc = conn_get_account(c)) &&
				(member = account_get_clanmember_forced(acc)) &&
				(clan = clanmember_get_clan(member)) &&
				(clanmember_get_fullmember(member) == 0))) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREPLY packet (reply without prior request)", conn_get_socket(c));
				return -1;

				// invalid inviter
			}
			else if (!((conn = connlist_find_connection_by_accountname(username)) &&
				(conn_account = conn_get_account(conn)) &&
				(conn_clan = account_get_clan(conn_account)) &&
				(conn_member = account_get_clanmember(conn_account)) &&
				(clanmember_get_status(conn_member) >= CLAN_SHAMAN) &&
				(clan_get_clantag(clan) == clan_get_clantag(conn_clan)))) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREPLY packet (invalid inviter)", conn_get_socket(c));
				return -1;
			}

			if (rpacket = packet_create(packet_class_bnet)) {
				packet_set_size(rpacket, sizeof(t_server_clan_invitereply));
				packet_set_type(rpacket, SERVER_CLAN_INVITEREPLY);
				bn_int_set(&rpacket->u.server_clan_invitereply.count, bn_int_get(packet->u.client_clan_invitereply.count));

				if (status != CLAN_RESPONSE_ACCEPT) {
					clan_remove_member(clan, member);
					bn_byte_set(&rpacket->u.server_clan_invitereply.result, status);
				}
				else {
					if (clan_get_member_count(clan) >= prefs_get_clan_max_members()) {
						clan_remove_member(clan, member);
						bn_byte_set(&rpacket->u.server_clan_invitereply.result, CLAN_RESPONSE_CLAN_FULL);
					}
					else
					{
						clanmember_set_fullmember(member, 1);
						if (conn_get_channel(c))
						{
							channel_set_userflags(c);

							std::string channelname("Clan " + std::string(clantag_to_str(clan_get_clantag(clan))));
							if (conn_set_channel(c, channelname.c_str()) < 0)
							{
								conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
							}
							clanmember_set_online(c);
						}
						clan_send_status_window(c);
						bn_byte_set(&rpacket->u.server_clan_invitereply.result, CLAN_RESPONSE_SUCCESS);
					}
				}
			}

			conn_push_outqueue(conn, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		static int _client_crashdump(t_connection* c, t_packet const* const packet)
		{
			return 0;
		}

		static int _client_setemailreply(t_connection* c, t_packet const* const packet)
		{
			char const* email;
			t_account* account;

			if (!(email = packet_get_str_const(packet, sizeof(t_client_setemailreply), MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad SETEMAILREPLY packet", conn_get_socket(c));
				return -1;
			}
			if (std::strlen(email) == 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}]userdeclinedtosetanemailaddress", conn_get_socket(c));
				return 0;
			}
			if (!(account = conn_get_account(c))) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL account for connection in setemail request");
				return -1;
			}
			if (account_get_email(account)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" already have email set, ignore set email", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (account_set_email(account, email) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] failed to init account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), email);
				return 0;
			}
			else
			{
				account_set_email_verified(account, false);
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] init account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), email);

				if (prefs_get_verify_account_email() == 1)
				{
					account_generate_email_verification_code(account);
				}
			}
			return 0;
		}

		static int _client_changeemailreq(t_connection* c, t_packet const* const packet)
		{
			char const* oldaddr;
			char const* newaddr;
			char const* username;
			char const* email;
			t_account* account;
			int pos;

			pos = sizeof(t_client_changeemailreq);
			if (!(username = packet_get_str_const(packet, pos, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad username in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(username) + 1);
			if (!(oldaddr = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad old email in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(oldaddr) + 1);
			if (!(newaddr = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad new email in CHANGEEMAILREQ packet", conn_get_socket(c));
				return -1;
			}
			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] change email for \"{}\" refused (no such account)", conn_get_socket(c), username);
				return 0;
			}
			if (!(email = account_get_email(account)) || !email[0]) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] account \"{}\" do not have email set, ignore changing", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (strcasecmp(email, oldaddr)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" email mismatch, ignore changing", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (account_set_email(account, newaddr) < 0) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] failed to change account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), newaddr);
				return 0;
			}
			else
			{
				account_set_email_verified(account, false);
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] change account \"{}\" email to \"{}\"", conn_get_socket(c), account_get_name(account), newaddr);

				if (prefs_get_verify_account_email() == 1)
				{
					account_generate_email_verification_code(account);
				}
			}
			return 0;
		}

		static int _client_getpasswordreq(t_connection* c, t_packet const* const packet)
		{
			char const* username;
			char const* try_email;
			char const* email;
			t_account* account;
			int pos;

			pos = sizeof(t_client_getpasswordreq);
			if (!(username = packet_get_str_const(packet, pos, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad username in GETPASSWORDREQ packet", conn_get_socket(c));
				return -1;
			}
			pos += (std::strlen(username) + 1);
			if (!(try_email = packet_get_str_const(packet, pos, MAX_EMAIL_STR))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad email in GETPASSWORDREQ packet", conn_get_socket(c));
				return -1;
			}
			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] get password for \"{}\" refused (no such account)", conn_get_socket(c), username);
				return 0;
			}
			if (!(email = account_get_email(account)) || !email[0]) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" do not have email set, ignore get password", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			if (strcasecmp(email, try_email)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" email mismatch, ignore get password", conn_get_socket(c), account_get_name(account));
				return 0;
			}
			/* TODO: send mail to user with the real password or changed password!?
			 * (as we cannot get the real password back, we should only change the password)     --Soar */
			eventlog(eventlog_level_info, __FUNCTION__, "[{}] get password for account \"{}\" to email \"{}\"", conn_get_socket(c), account_get_name(account), email);
			return 0;
		}



		static int _client_extrawork(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_extrawork)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad EXTRAWORK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_extrawork), packet_get_size(packet));
				return -1;
			}
			{
				short gametype;
				short length;
				const char* data;
				std::string data_s;

				gametype = bn_short_get(packet->u.client_extrawork.gametype);
				length = bn_short_get(packet->u.client_extrawork.length);

				if (!(data = (const char*)packet_get_raw_data_const(packet, sizeof(t_client_extrawork)))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad EXTRAWORK packet (missing or too long data)", conn_get_socket(c));
					return -1;
				}
				// extract substring with given length
				data_s = std::string(data).substr(0, length);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] Received EXTRAWORK packet with GameType: {} and Length: {} ({})", conn_get_socket(c), gametype, length, data_s.c_str());

#ifdef WITH_LUA
				lua_handle_client_extrawork(c, gametype, length, data_s.c_str());
#endif
			}
			return 0;
		}


		//here
		static int _client_ah_scan_response(t_connection* c, t_packet const* const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_ah_scan_response)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ANTIHACK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_ah_scan_response), packet_get_size(packet));
				return -1;
			}
			{
				//eventlog( eventlog_level_error, __FUNCTION__, "[{}] Test AH Packet", conn_get_socket( c ));
				int ahver = bn_int_get(packet->u.client_ah_scan_response.ah_version);
				int command = bn_int_get(packet->u.client_ah_scan_response.command);
				uint32_t var1 = bn_int_get(packet->u.client_ah_scan_response.val1);
				uint32_t var2 = bn_int_get(packet->u.client_ah_scan_response.val2);
				uint32_t var3 = bn_int_get(packet->u.client_ah_scan_response.val3);
				uint32_t var4 = bn_int_get(packet->u.client_ah_scan_response.val4);

				unsigned int offset = sizeof(t_client_ah_scan_response);
				if (command == 0xC0CE0)
				{
					const void* voicedata = packet_get_raw_data_const(packet, offset);
					conn_client_voice_response(c, var1, var2, var3, voicedata);
					return 0;
				}

				const char* strvar = packet_get_str_const(packet, sizeof(t_client_ah_scan_response), 512);
				offset += (strvar ? std::strlen(strvar) + 1 : 0);
				const char* strvar2 = packet_get_str_const(packet, offset, 512);
				offset += (strvar2 ? std::strlen(strvar2) + 1 : 0);
				const char* strvar3 = packet_get_str_const(packet, offset, 512);
				offset += (strvar3 ? std::strlen(strvar3) + 1 : 0);
				const char* strvar4 = packet_get_str_const(packet, offset, 512);
				offset += (strvar4 ? std::strlen(strvar4) + 1 : 0);


				conn_client_ah_scan_response(c, command, ahver, var1, var2, var3, var4, strvar ? strvar : "",
					strvar2 ? strvar2 : "", strvar3 ? strvar3 : "", strvar4 ? strvar4 : "");
			}
			return 0;
		}

	}

}
