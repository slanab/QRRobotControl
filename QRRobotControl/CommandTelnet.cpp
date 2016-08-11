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
bool CommandTelnet::isComplete(){
	return isCompleted;
}
bool CommandTelnet::isFinalRecevied() {
	return finalRecevied;
}
int CommandTelnet::getAngle(){
	return angle;
}
float CommandTelnet::getDistance() {
	return distance;
}

void CommandTelnet::setCompletion(bool isComplete) {
	isCompleted = isComplete;
}
void CommandTelnet::setFinalFlag(bool isFinal) {
	finalRecevied = isFinal;
}
void CommandTelnet::setOdometryNeed(bool isOdomNeeded) {
	expectOdometry = isOdomNeeded;
}
void CommandTelnet::setAngleCoefficient(float coeff) {
	angleCoefficient = coeff;
}
void CommandTelnet::setDistanceCoefficient(float coeff) {
	distanceCoefficient = coeff;
}

// Command re-initialization
void CommandTelnet::setCommand(string command) {
	int space = command.find(" ");
	commandFull = command + "\n";
	commandWord = command.substr(0, space); // Command word starts at 0 until the first space	
	parameter = command.substr(space + 1); // Parameter starts after the space and goes to the end of the command
	isCompleted = false;
	finalRecevied = false;
	responseNum = 10;
	expectOdometry = false;
	angle = 0;
	distance = 0;
	type = "";
	lastResponse = "Blablablabalbalab"; // Hopefully this string is never found
	adjust(*this);
}

// Modifies parameters based on the command type
void CommandTelnet::adjust(CommandTelnet& command) {
	string commandWord = command.getCommandWord();
	string parameter = command.getParameter();
	if (commandWord == "publish" && parameter == "camera") {
		command.lastResponse = "streaming camera";
	} else if (commandWord == "odometrystart") {
		command.lastResponse = "motorspeed";
	} else if ((commandWord == "forward") || (commandWord == "backward")) {
		command.lastResponse = "motion stopped";
		command.expectOdometry = true;
		command.distance = stod(command.parameter);
		command.type = "motion";
		if (distanceCoefficient != 1.0) {
			command.distance = stod(parameter) * command.distanceCoefficient;
			command.commandFull = command.commandWord + " " + to_string(command.distance) + "\n";
		}
	} else if ((commandWord == "left") || (commandWord == "right")) {
		command.lastResponse = "motion stopped";
		command.expectOdometry = true;
		command.angle = stoi(parameter);
		command.type = "motion";		
		if (angleCoefficient != 1.0) {		
			command.angle = stoi(parameter) * command.angleCoefficient;
			command.commandFull = command.commandWord + " " + to_string(command.angle) + "\n";
		}
	} else if ((commandWord == "lefttimed") || (commandWord == "righttimed")) {
		command.lastResponse = "direction stop";
	} else if (commandWord == "strobeflash") {
		command.lastResponse = "spotlightbrightness 0";
	} else if (commandWord == "nudge") {
		command.lastResponse = "direction stop";
	}
}

