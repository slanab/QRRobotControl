#pragma once
#include <string>
#include <iostream>

using namespace std;

class CommandTelnet
{
public:
	CommandTelnet();
	~CommandTelnet();
	CommandTelnet(string command);

	string getCommandFull();
	string getCommandWord();
	string getParameter();	
	string getLastResponse();
	string getType();
	int getResponseNum();
	bool isOdometryNeeded();
	int getAngle();
	double getDistance();

	void setCommand(string command);
	void setResponseNum(int num);
	void setLastResponse(string response);

	void printCommand();

private:
	string commandFull;
	string commandWord;
	string parameter;	
	string lastResponse;
	string type;
	int responseNum;
	bool expectOdometry;
	int angle;
	double distance;

	void setResponseEnd(CommandTelnet &command);
};

