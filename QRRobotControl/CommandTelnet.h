#pragma once
#include <string>
#include <iostream>

using namespace std;

class CommandTelnet
{
public:
	CommandTelnet();
	~CommandTelnet();

	string getCommandFull();
	string getCommandWord();
	string getParameter();	
	string getLastResponse();
	string getType();
	int getResponseNum();
	bool isOdometryNeeded();
	bool isComplete();
	bool isFinalRecevied();
	int getAngle();
	double getDistance();

	void setCommand(string command);
	void setCompletion(bool isCompleted);
	void setFinalFlag(bool isFinal);
	void setOdometryNeed(bool isOdomNeeded);
	void setAngleCoefficient(double coeff);

private:
	string commandFull;
	string commandWord;
	string parameter;	
	string lastResponse;
	string type;
	int responseNum;
	bool expectOdometry;
	bool isCompleted;
	bool finalRecevied;
	int angle;
	double distance;
	double angleCoefficient;

	void adjust(CommandTelnet &command);
};

