#include "CommandTelnet.h"

CommandTelnet::CommandTelnet()
{
}


CommandTelnet::~CommandTelnet()
{
}

string CommandTelnet::getCommandFull() {
	return commandFull;
}
string CommandTelnet::getCommandWord() {
	return commandWord;
}
string CommandTelnet::getParameter() {
	return parameter;
}
string CommandTelnet::getLastResponse(){
	return lastResponse;
}
string CommandTelnet::getType(){
	return type;
}
int CommandTelnet::getResponseNum(){
	return responseNum;
}
bool CommandTelnet::isOdometryNeeded(){
	return expectOdometry;
}
int CommandTelnet::getAngle(){
	return angle;
}
double CommandTelnet::getDistance() {
	return distance;
}

void CommandTelnet::setResponseNum(int num) {
	responseNum = num;
}
void CommandTelnet::setLastResponse(string response) {
	lastResponse = response;
}

// Sets maximum wait time and response that should be the last useful response depending on the command sent
void CommandTelnet::setResponseEnd(CommandTelnet& command) {
	string commandWord = command.getCommandWord();
	string parameter = command.getParameter();
	command.responseNum = 10;
	command.expectOdometry = false;
	command.angle = 0;
	command.distance = 0;
	command.type = "";
	if (commandWord == "publish" && parameter == "camera") {
		command.lastResponse = "streaming camera";
	} else if (commandWord == "odometrystart") {
		command.lastResponse = "motorspeed";
	} else if ((commandWord == "forward") || (commandWord == "backward")) {
		command.lastResponse = "motion stopped";
		command.expectOdometry = true;
		command.distance = stod(command.parameter);
		command.type = "motion";
	} else if ((commandWord == "left") || (commandWord == "right")) {
		command.lastResponse = "motion stopped";
		command.expectOdometry = true;
		command.angle = stoi(parameter);
		command.type = "motion";
	}
	else if (commandWord == "strobeflash") {
		command.lastResponse = "spotlightbrightness 0";
	}
	else {
		command.lastResponse = "";		
	}	
}

void CommandTelnet::printCommand() {
	cout << "Full command is: " << this->commandFull << endl;
	cout << "Command word is: '" << this->commandWord << "' Parameter is: '" << this->parameter << "' Response to wait for: '" << this->lastResponse << "' Number of responses to wait: '" << this->responseNum << "'\n";
	cout << "Command type is: " << this->type;
}

// TODO: Command is properly processed only if it follows "word parameter" structure. Add some error correction.
CommandTelnet::CommandTelnet(string command) {
	commandFull = command;
	int space = command.find(" ");
	commandWord = command.substr(0, space); // Command word starts at 0 until the first space	
	parameter = command.substr(space + 1); // Parameter starts after the space and goes to the end of the command
	setResponseEnd(*this); 
}

void CommandTelnet::setCommand(string command) {
	int space = command.find(" ");
	commandFull = command + "\n";	
	commandWord = command.substr(0, space); // Command word starts at 0 until the first space	
	parameter = command.substr(space + 1); // Parameter starts after the space and goes to the end of the command
	setResponseEnd(*this);
}