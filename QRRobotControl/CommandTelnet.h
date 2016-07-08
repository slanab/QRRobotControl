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
	string getFullCommand() {
		return commandFull;
	}	
	string getCommandWord(){
		return commandWord;
	}
	int getResponseNum() {
		return responseNum;
	}
private:
	string commandFull;
	string commandWord;
	string parameter;
	int responseNum;

};

