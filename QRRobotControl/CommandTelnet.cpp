#include "CommandTelnet.h"


CommandTelnet::CommandTelnet()
{
}


CommandTelnet::~CommandTelnet()
{
}

int correctAngle(int angle) {
	double correction = 1.0;
	int corrected = angle*correction;
	return corrected;
}

int setResponseNum(string commandWord) {
	return 10;
}

string setLastResponse(string commandWord, string parameter) {
	if (commandWord == "publish" && parameter == "camera") {
		return "streaming camera";
	}
	if (commandWord == "odometrystart") {
		return "motorspeed";
	}
	if ((commandWord == "forward") || (commandWord == "backward")) {
		return "motion stopped";
	}
	if ((commandWord == "left") || (commandWord == "right")) {
		return "direction stop";
	}

	return "";
}
// TODO: Command is properly processed only if it follows "word parameter" structure. Add some error correction.
CommandTelnet::CommandTelnet(string command) {
	commandFull = command; // Command has to end with a newline symbol to be performed
	int space = command.find(" ");
	commandWord = command.substr(0, space); // Command word starts at 0 until the first space	
	parameter = command.substr(space + 1); // Parameter starts after the space and goes to the end of the command
	// Robot is currently consistently underrotating, need to correct the angle before passing the command to the robot
	if ((commandWord == "left") || (commandWord == "right")) { 
		int newAngle = correctAngle(stoi(parameter)); // Convert angle parameter to int and correct it
		parameter = to_string(newAngle);
		commandFull = commandWord + " " + parameter + '\n';
	}	
	responseNum = setResponseNum(commandWord);
	lastResponse = setLastResponse(commandWord, parameter);
}