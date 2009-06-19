#include "Frelon.h"
#include "RegEx.h"

using namespace lima;
using namespace std;

const double Frelon::SerialLine::TimeoutSingle = 0.5;
const double Frelon::SerialLine::TimeoutNormal = 2.0;
const double Frelon::SerialLine::TimeoutMax    = 3.0;
const double Frelon::SerialLine::TimeoutReset  = 5.0;


Frelon::SerialLine::SerialLine(EspiaSerialLine& espia_ser_line)
	: m_espia_ser_line(espia_ser_line)
{
	m_espia_ser_line.setLineTerm("\r\n");
	m_espia_ser_line.setTimeout(TimeoutNormal);
	m_multi_line_cmd = true;
}

void Frelon::SerialLine::write(const std::string& buffer, bool no_wait)
{
}

void Frelon::SerialLine::read(std::string& buffer, int len, double timeout)
{
}

void Frelon::SerialLine::readStr(std::string& buffer, int len, double timeout)
{
}

void Frelon::SerialLine::getNumAvailBytes(int &avail)
{

}

void Frelon::SerialLine::splitMsg(const string& msg, 
				  map<MsgPart, string>& msg_parts) const
{
	msg_parts.clear();

	RegEx re("^(?P<sync>>)?"
		 "(?P<cmd>[A-Z]+)"
		 "((?P<req>\\?)|(?P<val>[0-9]+))?"
		 "(?P<term>[\r\n]+)?$");

	RegEx::FullNameMatchType match;
	if (!re.matchName(msg, match))
		throw LIMA_HW_EXC(InvalidValue, "Invalid Frelon message");

	typedef pair<MsgPart, string> KeyPair;
	static const KeyPair key_list[] = {
		KeyPair(MsgSync, "sync"), KeyPair(MsgCmd, "cmd"), 
		KeyPair(MsgVal,  "val"),  KeyPair(MsgReq, "req"),  
		KeyPair(MsgTerm, "term"),
	};
	static int key_len = sizeof(key_list) / sizeof(key_list[0]);

	const KeyPair *it, *end = key_list + key_len;
	for (it = key_list; it != end; ++it) {
		const MsgPart& key = it->first;
		const string&  grp = it->second;
		msg_parts[key] = string(match[grp].start, match[grp].end);
	}
}


Frelon::Frelon(EspiaSerialLine& espia_ser_line)
	: m_ser_line(espia_ser_line)
{

}
