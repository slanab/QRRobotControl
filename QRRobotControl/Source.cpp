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
vector<string> commandBuffer = { "forward 1", "backward 1" };
CommandTelnet currentCommand;
bool searchNextQR = false;
bool isCommandComplete = true;
bool isAngleReceived = false;

// Function declarations
void sendCommand(string command);
void correctAngle(double commandAngle, double realAngle); 
double extractAngle(string response);

string waitForSocketResponse(int responseCycles) {
	string response;
	for (int i = 0; i < responseCycles; i++) {
		response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	}
	return response;
}

double extractAngle(string response) {
	stringstream responseStream(response);
	string responseLine;
	double realAngle;
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

void correctAngle(double commandAngle, double realAngle) {
	double angleDifference = commandAngle - realAngle;
	if (abs(angleDifference) > 1) {
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

void checkCommandFinish(string finalResponse) {
	bool waitForAngle = false;
	string response = waitForSocketResponse(1);
	if (currentCommand.getType() == "motion") {		
		waitForAngle = true;
		if (response.find("distanceangle") != -1) {
			isAngleReceived = true;
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
	if ((response.find(finalResponse) != -1) && (!waitForAngle)) {
		//waitForSocketResponse(1);
		isCommandComplete = true;
	}
}

void sendCommand(string command) {
	command = command + '\n';
	socketSend(command.data(), command.length());
}

string processCommand(string command) {
	cout << "Start on command " << command << endl;
	currentCommand.setCommand(command);
	string finalResponse = currentCommand.getLastResponse();	

	if (currentCommand.getCommandWord() == "forward" || currentCommand.getCommandWord() == "backward") {
		// Break long distance into smaller ones 
		if (currentCommand.getDistance() > 0.5) {
			isCommandComplete = true; // Ignore current command, divide into two smaller ones
			commandBuffer.erase(commandBuffer.begin()); 
			double distanceLeft = currentCommand.getDistance() - 0.5;
			string newCommand = currentCommand.getCommandWord() + " " + to_string(distanceLeft);
			commandBuffer.insert(commandBuffer.begin(), newCommand);
			newCommand = currentCommand.getCommandWord() + " 0.5";
			commandBuffer.insert(commandBuffer.begin(), newCommand);					
			return finalResponse;
		}
	}

	if (currentCommand.getType() == "motion") {
		isAngleReceived = false;
		cout << "Perform angle correction\n";
		sendCommand(currentCommand.getCommandFull());
	}
	else {
		sendCommand(currentCommand.getCommandFull());
	}
	commandBuffer.erase(commandBuffer.begin());
	isCommandComplete = false;	
	return finalResponse;
}

// Sendd command to the robot and waits for an appropriate response indicating that the command is complete.
void completeCommand(string commandLine) {
	isCommandComplete = false;
	cout << "Prforming command" << commandLine;
	int it = 0;
	currentCommand.setCommand(commandLine);
	sendCommand(currentCommand.getCommandFull());
	while (!isCommandComplete) {
		cout << "Wait for " << currentCommand.getLastResponse() << endl;
		checkCommandFinish(currentCommand.getLastResponse());
		it++;
		if (it >= 10) {
			cout << "Command timedout\n";
			break;
		}
	}
	isCommandComplete = true;
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
	completeCommand("odometrystart");

	// Obtaining robot's webcam stream		
	completeCommand("publish camera");	
	VideoCapture cap (filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}	

	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);	

	int cycles = 0;
	string command = "";
	string lastCommand = "";

	Mat frame;
	bool bSuccess;
	int i = 15;
	isCommandComplete = true;
	string finalResponse = "";

	while (true) { // Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
		if (!commandBuffer.empty()) {
			cout << "Vector content: ";
			for (int it = 0; it < commandBuffer.size(); ++it) {
				cout << commandBuffer[it] << ' ';
			}
		}

		// Update video frame
		bSuccess = cap.read(frame); // read a new frame from video  
		if (!bSuccess) //if not success, break loop  
		{
			cout << "Cannot read a frame from video stream in main loop" << endl;
			break;
		}
		imshow("Oculus Prime", frame);		

		if (i >= 15) {		// Do command processing only every 15 iterations to avoid camera freeze
			if (!isCommandComplete) { // Check if command was completed while waiting foriterations
				checkCommandFinish(finalResponse);
			} 
			if (isCommandComplete) { 
				if (commandBuffer.size() != 0) {
					finalResponse = processCommand(commandBuffer[0]);
				}
				else { // Only look for QR codes when the robot is done performing all commands and has nothing to do
					//waitForSocketResponse(1);
					//cout << "All commands completed, look for QR codes now\n";
					Mat grey;
					cvtColor(frame, grey, CV_BGR2GRAY);
					int width = frame.cols;
					int height = frame.rows;
					uchar *raw = (uchar *)grey.data;
					Image image(width, height, "Y800", raw, width * height);
					int numQRs = scanner.scan(image); // Will return 0 if no QR codes are found
					//cout << "Number of QRs found " << numQRs << endl;
				}
			}			
			i = 0;
		}
		i++;
		/**
		 * Look for QR codes in the frame, decode if a code is found, and perform decoded command
		 * If robot sees the same QR code for a while, it will not repeat the same commands while QR code is in the frame.
		 * However, if QR code is taken away from the frame (ex. robot moves and sees the same command on a different QR code), 
		 * last command is forgotten so the robot would perform newly seen command even if it is the same as the previos command. 
		 */

		//if (numQRs) { // Found a QR code in the frame
		//	cycles = 0; // Reset number of cycles of seeing no QR
		//	// Extract results of QR decoding, perform decoded command
		//	for (SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
		//		lastCommand = command;
		//		command = symbol->get_data();
		//		cout << "Decoded command: " << command << endl;
		//		cout << "Previous command was: " << lastCommand << endl;
		//		if ((command != "") && (command != lastCommand)) {		// Do not repeat the same command over and over while the same QR code is in the camera frame					
		//			cout << "Perform command " << command << endl;
		//			performCommands(command);
		//			//searchNextQR = true;
		//		}
		//	}			
		//} else { // No QR detected
		//	if (cycles >= 5) { // Reset commands if there is no QR code in the frame for 5 cycles (arbitrary value, might need to be adjusted)
		//		lastCommand = "";
		//		command = "";
		//		if (searchNextQR && cycles >= 20) {
		//			performCommands("strobeflash on 500 30\nbackward 0.1");
		//			cycles = 0;
		//		}
		//	}
		//	cycles++; // Increment cycles each time no QR is detected;			
		//}
		if (waitKey(30) == 27) {
			break;
		}
	}
	closeConnection();
	cout << "Press enter to exit";
	cin.ignore(numeric_limits<streamsize>::max(), '\n');
	return 0;
}