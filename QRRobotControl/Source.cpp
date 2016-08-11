#include <iostream>
#include <limits>
#include <windows.h> 
#include <string>

#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <zbar.h>  

#include "socketConnection.h"
#include "CommandTelnet.h"

using namespace cv;
using namespace std;
using namespace zbar;

// Constants
int port_num_telnet = 4444; 
char *IP_ADD = "207.23.183.214"; // Robot IP
string filename = "rtmp://207.23.183.214:1935/oculusPrime/stream1";

// Global variables
vector<string> commandBuffer = { };
// Sequence of commands used to look for QR codes in case next QR code is not in the frame
vector<string> recoveryCommands = { "nudge right", "nudge right", "nudge right", "nudge left", "nudge left", "nudge left", "nudge left"};
CommandTelnet currentCommand;

// Function declarations
string waitForSocketResponse(int responseCycles);
void sendCommand(string command);
void correctAngle(float commandAngle, float realAngle);
float extractAngle(string response);
void completeNextCommand();
void completeCommand(string commandLine);

// *** Socket access ***
string waitForSocketResponse(int responseCycles) {
	string response;
	for (int i = 0; i < responseCycles; i++) {
		response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	}
	return response;
}

void sendCommand(string command) {
	cout << "!!!!!!!!!! Sending command to the robot " << command << endl;
	command = command + '\n';
	socketSend(command.data(), command.length());
}
// *********************

void printVector() {
	cout << "Commands in the buffer: \n";
	for (int i = 0; i < commandBuffer.size(); i++) {
		cout << commandBuffer.at(i) << endl;
	}
}

float extractAngle(string response) {
	stringstream responseStream(response);
	string responseLine;
	float realAngle = 0;;
	while (getline(responseStream, responseLine, '\n')) {
		if (responseLine.find("distanceangle") != -1) {
			int pos = 0;
			for (int i = 0; i < 3; i++) {
				pos = responseLine.find(" ", pos);
				pos++;
			}
			if (responseLine.at(pos) == '-' || isdigit(responseLine.at(pos))) {
				realAngle = stod(responseLine.substr(pos));
			}
			break;
		}
	}
	return realAngle;
}

void calibrateAngle() {
	string response;
	float realAngle = 0;
	float totalAngle = 0;
	float coefficient = 1;
	for (int j = 0; j < 4; j++) {
		sendCommand("left 90");
		for (int i = 0; i < 10; i++) {
			response = waitForSocketResponse(1);
			if (response.find("distanceangle 0") != -1) {
				realAngle = extractAngle(response);
				totalAngle = totalAngle + extractAngle(response);
				break;
			}
		}
	}
	float averageAngle = totalAngle / 4;
	coefficient = 90.0 / averageAngle;
	currentCommand.setAngleCoefficient(coefficient);
	float overrotatedAngle = totalAngle - 360;
	string command = "right " + to_string(overrotatedAngle);
	completeCommand(command);
	cout << "---------------------------------------\nAngle calibration complete\n---------------------------------------\n\n";
}

float extractDistance(string response) {
	stringstream responseStream(response);
	string responseLine;
	float realDistance = 0;;
	while (getline(responseStream, responseLine, '\n')) {
		if (responseLine.find("distanceangle") != -1) {
			int pos = 0;
			for (int i = 0; i < 2; i++) {
				pos = responseLine.find(" ", pos);
				pos++;
			}
			if (responseLine.at(pos) == '-' || isdigit(responseLine.at(pos))) {
				realDistance = stod(responseLine.substr(pos));
				break;
			}
		}
	}
	return realDistance;
}

void calibrateDistance() {
	vector<string> commands = { "forward 1", "backward 1", "forward 1", "backward 1" };
	string response;
	float realDistance = 0;
	float totalAbsDistance = 0;
	float totalDistance = 0;
	float coefficient = 1;
	for (int j = 0; j < 4; j++) {
		sendCommand(commands.at(j));
		for (int i = 0; i < 10; i++) {
			response = waitForSocketResponse(1);
			if ((response.find("distanceangle") != -1) && (response.find("distanceangle 0") == -1)){
				realDistance = extractDistance(response);
				totalAbsDistance = totalAbsDistance + abs(realDistance);
				totalDistance = totalDistance + realDistance;
				break;
			}
		}
	}
	float averageDistance = totalAbsDistance / 4;
	coefficient = 1000 / averageDistance;
	currentCommand.setDistanceCoefficient(coefficient);
	float distance = (1000 - totalDistance) / 1000;
	string command = "forward " + to_string(distance);
	completeCommand(command);
	cout << "---------------------------------------\nDistance calibration complete\n---------------------------------------\n\n";
}

// If the difference between command angle and real angle obtained from the gyro is more than 7 degrees, a turn command will be called to compensate for the difference
void correctAngle(float commandAngle, float realAngle) {
	float angleDifference = commandAngle - realAngle;
	cout << "Expected angle " << commandAngle << " Real angle " << realAngle << " Angle difference " << angleDifference << endl;
	if (abs(angleDifference) > 7) {
		int angle = round(angleDifference);
		string command = "";
		if (angle < 0) {
			command = "right " + to_string(abs(angle));
		}
		else {
			command = "left " + to_string(abs(angle));
		}
		cout << "!!!!!!!!!!!!!! Correct angle: add command " << command << endl;
		commandBuffer.insert(commandBuffer.begin(), command);
	}
	else {
		cout << "No further angle correction necessary\n";
	}
}

// Checks if the response obtained from the robot contains words that indicate command completion
bool checkCommandFinish(string finalResponse) {	
	string response = waitForSocketResponse(1);
	if (currentCommand.isOdometryNeeded()) {	
		if (response.find("distanceangle") != -1) {
			currentCommand.setOdometryNeed(false); // Stop waiting for odometry info after receiving angle from gyro
			float realAngle = extractAngle(response);
			float commandAngle = 0; // Stays 0 if motion is FW or BW
			if (currentCommand.getCommandWord() == "left") {
				commandAngle = stoi(currentCommand.getParameter());
			}
			else if (currentCommand.getCommandWord() == "right") {
				commandAngle = 0 - stoi(currentCommand.getParameter());
			}
			correctAngle(commandAngle, realAngle);
		}
		else {
		}
	}
	if (response.find(finalResponse) != -1) {
		currentCommand.setFinalFlag(true);		
	}

	if (currentCommand.isFinalRecevied() && !currentCommand.isOdometryNeeded()) {
		cout << "Current command completed\n";
		currentCommand.setCompletion(true);
		return true;
	}
	return false;
}

// Send first command in the buffer to the robot and set up global variables to track progress. Do not wait for completion
void startNextCommand() {		
	string commandLine = commandBuffer[0];
	cout << "Prforming command " << commandLine << endl;
	currentCommand.setCommand(commandLine);
	sendCommand(currentCommand.getCommandFull());
	commandBuffer.erase(commandBuffer.begin());
}

// Send first command in the buffer to the robot and wait for an appropriate response indicating that the command is complete.
// Program is blocked until the command is completed. No video update is received.
void completeNextCommand() {
	if (commandBuffer.empty()) {
		cout << "No commands to complete\n";
		return;
	}
	startNextCommand();
	for (int j = 0; j < 10; j++) {
		if (checkCommandFinish(currentCommand.getLastResponse())) {		
			currentCommand.setCompletion(true);			
			return;
		}
	}	
	cout << "Command timed out\n";
}

void completeCommand(string commandLine) {
	commandBuffer.insert(commandBuffer.begin(), commandLine);
	completeNextCommand();
}

// Looks for QR codes in the frame and returns encoded message if QR code is found. Returns an empty string if no QR message found.
string getQRData(ImageScanner &scanner, Mat frame) {
	Mat grey;
	cvtColor(frame, grey, CV_BGR2GRAY);
	int width = frame.cols;
	int height = frame.rows;
	uchar *raw = (uchar *)grey.data;
	Image image(width, height, "Y800", raw, width * height);
	int numQRs = scanner.scan(image); // Will return 0 if no QR codes are found
	string command = "";
	for (SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
		command = symbol->get_data();
		cout << "Decoded command: " << command << endl;
	}		
	return command;
}

// Parse commands separated by newline symbolds and add to command buffer
void addCommandsToBuffer(string commands) {
	stringstream commandStream(commands);
	string commandLine;
	while (getline(commandStream, commandLine, '\n')) {	// Get a single command from the QR code
		commandBuffer.push_back(commandLine);
		cout << "Added to buffer: " << commandLine << endl;
	}
}

int main(int argc, char* argv[])
{
	// Establishing socket connection
	if (!connectToHost(port_num_telnet, IP_ADD))
	{ 
		cout << "Cannot connect to the host" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}		
	waitForSocketResponse(2);

	// Calibration routine that uses gyro data to improve angle and distance precision
	completeCommand("odometrystart");
	calibrateDistance();
	calibrateAngle();
	
	// Obtaining robot's webcam stream		
	completeCommand("publish camera");	
	cout << "Camera image is loading. Please stay patient.\n";
	VideoCapture cap (filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}		

	// First QR is located to the right of the initial robot position
	commandBuffer = { "right 90" };

	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
	int i = 0;
	int j = 0;
	int commandI = 0;
	while (true) { 
		// Update video frame
		Mat frame;
		if (!cap.read(frame)) 
		{
			cout << "Cannot read a frame from video stream in main loop" << endl;
			break;
		}
		imshow("Oculus Prime", frame);	

		// Do command processing only every 15 iterations to avoid camera freeze
		if (i >= 15) {		
			if (!currentCommand.isComplete()) { // Check if last command was completed while waiting for iterations
				checkCommandFinish(currentCommand.getLastResponse());
			} 
			if (currentCommand.isComplete()) {
				if (commandBuffer.size() != 0) { // Keep processing buffered commands until none is left
					startNextCommand();
				}
			}			
			i = 0;
		}		
		i++;

		// Look for QR codes in the frame if the robot has no commands left to perform 
		if (currentCommand.isComplete() && commandBuffer.empty()) {
			string commands = getQRData(scanner, frame);
			if (commands != "") { // QR commands found
				cout << "Got commands after " << j << endl;
				addCommandsToBuffer(commands);
				i = 15; // Jumpstart next instruction
				j = 0;
				commandI = 0;
			}
			else { // If no QR found for 30 cycles - start using commands from recovery vector
				if (j >= 30) {
					completeCommand(recoveryCommands.at(commandI));
					j = 0;
					commandI++;
				}
				j++;
			}			
		}

		// Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
		if (waitKey(30) == 27) {
			break;
		}		
	}
	sendCommand("odometrystop");
	waitForSocketResponse(1);
	closeConnection();
	cout << "Press enter to exit";
	cin.ignore(numeric_limits<streamsize>::max(), '\n');
	return 0;
}