#include "db_vac.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Poco/Data/Common.h>
#include <Poco/Data/MetaColumn.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Session.h>

#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include <Poco/URI.h>
#include <Poco/Exception.h>
#include <string>

//typedef Poco::Tuple<std::string, std::string, int> VAC_INFO;
/*
bool sanitize_steamid

{
    bool bRejected=false; // has strName been rejected?
 
    // Step through each character in the string until we either hit
    // the end of the string, or we rejected a character
    for (unsigned int nIndex=0; nIndex < strName.length() && !bRejected; nIndex++)
    {
        // If the current character is an alpha character, that's fine
        if (isalpha(strName[nIndex]))
            continue;
 
        // If it's a space, that's fine too
        if (strName[nIndex]==' ')
            continue;
 
        // Otherwise we're rejecting this input
        bRejected = true;
    }
} 
*/

bool DB_VAC::querySteam(std::string steam_id, SteamVacInfo &vac_info)
{
	Poco::URI uri("http://api.steampowered.com/ISteamUser/GetPlayerBans/v1/?key=<api key>&format=json&steamids=" + steam_id);
	Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());

	// prepare path
	std::string path(uri.getPathAndQuery());
	if (path.empty()) path = "/";

	// send request
	Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	session.sendRequest(req);

	// get response
	Poco::Net::HTTPResponse res;
	//http://www.appinf.com/docs/poco/Poco.Net.HTTPResponse.html#17176
	std::cout << res.getStatus() << " " << res.getReason() << std::endl;

	// print response
	std::istream &is = session.receiveResponse(res);

	boost::property_tree::ptree pt;
	boost::property_tree::read_json(is, pt);

	vac_info.SteamID = pt.get<std::string>("players..SteamId", "FAILED");
	vac_info.NumberOfVACBans = pt.get<std::string>("players..NumberOfVACBans", "FAILED");
	vac_info.VACBanned = pt.get<std::string>("players..VACBanned", "FAILED");
	vac_info.DaysSinceLastBan = pt.get<std::string>("players..DaysSinceLastBan", "FAILED");
	vac_info.EconomyBan = pt.get<std::string>("players..EconomyBan", "FAILED");
	
	if ((vac_info.SteamID == "FAILED") || (vac_info.NumberOfVACBans == "FAILED")|| (vac_info.VACBanned == "FAILED") || (vac_info.DaysSinceLastBan == "FAILED") || (vac_info.EconomyBan == "FAILED"))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void DB_VAC::updateVAC(Poco::Data::Session &db_session, std::string &steam_id)
{
	SteamVacInfo vac_info;
	bool status = querySteam(steam_id, vac_info);
	if (status)
	{
		// Save to DB
		Poco::Data::Statement insert(db_session);
//		insert << "INSERT INTO 'VAC BANS' (\"SteamID\", \"Number of Vac Bans\", \"Days Since Last Ban\", \"Last Check\") VALUES(:steamid, :number_of_bans, :days_since_last_bans, :last_check)",
//					Poco::Data::now(vac_info.SteamID), Poco::Data::now(vac_info.NumberOfVACBans), Poco::Data::now(vac_info.DaysSinceLastBan), Poco::DateTime(), Poco::Data::now;
	//	insert.execute();
		// TODO: Check for VAC BANS
		// TODO: Add RCON KICK / BAN
	}
	else
	{
		// FAILED STEAM QUERY
		std::cout << "extdb: Steam VAC Query Failed: " + steam_id << std::endl;
	}
}

std::string DB_VAC::callProtocol(AbstractExt *extension, std::string input_str)
{
	// TODO Exception Handling ?
	std::string steam_id = input_str; /// TODO: Santized INPUT
	Poco::Data::Session db_session = extension->getDBSession_mutexlock();
	Poco::Data::Statement select(db_session);
	select << ("SELECT \"Number of Vac Bans\" FROM `VAC BANS` where SteamID="  + steam_id);
	select.execute();
	Poco::Data::RecordSet rs(select);
	
	
	if (rs.moveFirst())
	{
		Poco::DateTime last_check;
		Poco::DateTime now;
		int tzd;
		bool last_check_status = Poco::DateTimeParser::tryParse("%e-%n-%Y", rs.value("Last Check").convert<std::string>(), last_check, tzd);
		//bool last_check_status = Poco::DateTimeParser::tryParse("%e-%n-%Y", std::string(rs.column[std::size_t(0)]), last_check);
		//if (last_check_status)
			//last_check = Poco::DateTimeParser::parse("%e-%n-%Y", rs.column["Last Check"], last_check);
			
		if (now - last_check >= Poco::Timespan(7*Poco::Timespan::DAYS));
		{
			updateVAC(db_session, steam_id);
		}
	}
	else
	{
		updateVAC(db_session, steam_id);
	}
}

//		if rs.column["Last Check"]
		//rs.column["SteamID"]
		//rs.column["Number of Vac Bans"]
		//rs.column["Days Since Last Ban"]