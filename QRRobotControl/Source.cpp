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
vector<string> commandBuffer = { "left 90", "right 90" };
CommandTelnet currentCommand;
double coefficient = 1;
bool searchNextQR = false;
bool isCommandComplete = true;
bool isFinalRecevied = false;
bool waitForAngle = false;

// Function declarations
string waitForSocketResponse(int responseCycles);
void sendCommand(string command);
void correctAngle(double commandAngle, double realAngle); 
double extractAngle(string response);

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
	cout << "Vector: \n";
	for (int i = 0; i < commandBuffer.size(); i++) {
		cout << commandBuffer.at(i) << endl;
	}
}

double extractAngle(string response) {
	stringstream responseStream(response);
	string responseLine;
	double realAngle = 0;;
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
	double realAngle = 0;
	double totalAngle = 0;
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
		cout << "Sent angle 90, moved by " << realAngle << endl;
	}
	double averageAngle = totalAngle / 4;
	coefficient = 90.0 / averageAngle;
	cout << "Average angle is " << averageAngle << " so coefficient is " << coefficient << endl;
}

void correctAngle(double commandAngle, double realAngle) {
	double angleDifference = commandAngle - realAngle;
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

bool checkCommandFinish(string finalResponse) {	
	cout << "Checking if command is complete\n";
	string response = waitForSocketResponse(1);
	if (waitForAngle) {				
		if (response.find("distanceangle") != -1) {
			cout << "Found angle response\n";
			waitForAngle = false;
			double realAngle = extractAngle(response);
			double commandAngle = 0; // Stays 0 if motion is FW or BW
			if (currentCommand.getCommandWord() == "left") {
				commandAngle = currentCommand.getAngle();
			}
			else if (currentCommand.getCommandWord() == "right") {
				commandAngle = 0 - currentCommand.getAngle();
			}
			correctAngle(commandAngle, realAngle);
		}
	}
	if (response.find(finalResponse) != -1) {
		cout << "Found final response\n";
		isFinalRecevied = true;		
	}

	if (isFinalRecevied && !waitForAngle) { // Command is considered finished when final response is recevied and we don't need to wait for odometry angle anymore
		isCommandComplete = true;
		return true;
	}
	return false;
}

// Send first command in the buffer to the robot and set up global variables to track progress. Do not wait for completion
void startNextCommand() {
	isCommandComplete = false;
	isFinalRecevied = false;
	string commandLine = commandBuffer[0];
	cout << "Prforming command " << commandLine << endl;
	currentCommand.setCommand(commandLine);
	sendCommand(currentCommand.getCommandFull());
	commandBuffer.erase(commandBuffer.begin());
	if (currentCommand.getType() == "motion") {
		waitForAngle = true;
	}
}

// Send first command in the buffer to the robot and wait for an appropriate response indicating that the command is complete.
// Program is blocked until the command is completed. No video update is received.
void completeNextCommand() {
	if (commandBuffer.empty()) {
		cout << "No commands to complete\n";
		return;
	}
	startNextCommand();
	cout << "Wait for " << currentCommand.getLastResponse() << endl;
	for (int j = 0; j < 10; j++) {
		if (checkCommandFinish(currentCommand.getLastResponse())) {			
			isCommandComplete = true;
			cout << "Command completed\n";
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

	// Obtaining robot's webcam stream		
	completeCommand("publish camera");	
	VideoCapture cap (filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}	

	completeCommand("odometrystart");
	//calibrateAngle();

	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);	

	int i = 0;
	while (true) { // Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
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
			if (!isCommandComplete) { // Check if last command was completed while waiting for iterations
				checkCommandFinish(currentCommand.getLastResponse());
			} 
			if (isCommandComplete) { 
				if (commandBuffer.size() != 0) { // Keep processing buffered commands until none is left
					startNextCommand();
				}
			}			
			i = 0;
		}		
		i++;

		// Look for QR codes in the frame if the robot has no commands left to perform 
		if (isCommandComplete && commandBuffer.empty()) {
			string commands = getQRData(scanner, frame);
			if (commands != "") {
				addCommandsToBuffer(commands);
				i = 15; // Jumpstart next instruction
			}
		}

		// Exit the program if esc key is pressed for 30 seconds
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