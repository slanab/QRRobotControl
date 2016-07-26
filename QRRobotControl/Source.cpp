#include <iostream>
#include <string>
#include <limits>
#include <windows.h> 

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
VideoCapture cap;
Mat frameGlobal;
bool searchNextQR = false;
bool isCommandComplete = true;

// Function declarations
Mat updateFrame();
void performCommands(string commands);
void performSingleCommand(CommandTelnet command);
void sendCommand(string command);
void waitForSocketResponse(int responseCycles);
void waitForSocketResponse(CommandTelnet command);
void waitForSocketResponseCorrectAngle(CommandTelnet command); 
void correctAngle(double commandAngle, double realAngle); 
double extractAngle(string response);

Mat updateFrame() {
	Mat frameTemp;
	bool bSuccess;
	bSuccess = cap.read(frameTemp);
	if (!bSuccess)
	{
		cout << "Cannot read a frame from video stream" << endl;
		return frameTemp;
	}
	imshow("Oculus Prime", frameTemp);		
	waitKey(1);
	return frameTemp;
}

void correctAngle(double commandAngle, double realAngle) {	
	double angleDifference = commandAngle - realAngle;
	if (abs(angleDifference) > 1) {		
		int angle = round(angleDifference);
		if (angle < 0) {
			string command = "right " + to_string(abs(angle));
			performCommands(command);
		}
		else {
			string command = "left " + to_string(abs(angle));
			performCommands(command);
		}
	}
	else {
	}
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

void waitForSocketResponseCorrectAngle(CommandTelnet command) {
	bool isFinalResponseReceived = false;
	bool isOdometryReceived = false;
	double realAngle = 0;
	for (int i = 0; i < command.getResponseNum(); i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
		if (command.getLastResponse() != "" && response.find(command.getLastResponse()) != -1) {
			if (isOdometryReceived) {
				break;
			}
			isFinalResponseReceived = true;
		}
		if (response.find("distanceangle") != -1) {
			isOdometryReceived = true;
			realAngle = extractAngle(response);
			if (isFinalResponseReceived) {
				break;
			}
		}
	}
	double commandAngle = 0;
	if (command.getCommandWord() == "left") {
		commandAngle = command.getAngle();
	}
	else if (command.getCommandWord() == "right") {
		commandAngle = 0 - command.getAngle();
	}
	correctAngle(commandAngle, realAngle);
}

void waitForSocketResponse(CommandTelnet command) {
	for (int i = 0; i < command.getResponseNum(); i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
		if (command.getLastResponse() != "" && response.find(command.getLastResponse()) != -1) {
			break;
		}
	}
}

void waitForSocketResponse(int responseCycles) {
	for (int i = 0; i < responseCycles; i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	}
}

void checkCommandFinish(string finalResponse) {
	string response = socketResponse();
	cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	if (response.find(finalResponse) != -1) {
		isCommandComplete = true;
	}
}

void sendCommand(string command) {
	command = command + '\n';
	socketSend(command.data(), command.length());
}

void performSingleCommand(CommandTelnet command) {
	cout << "Performing command " << command.getCommandFull() << endl;

	updateFrame();

	string commandFull = command.getCommandFull();
	sendCommand(commandFull);
	if (command.isOdometryNeeded()) {
		waitForSocketResponseCorrectAngle(command);
	}
	else {
		waitForSocketResponse(command);
	}
}

void performSingleCommand(string commandLine) {
	cout << "Prforming command" << commandLine;
	CommandTelnet command(commandLine);
	sendCommand(commandLine);
}

// Send command and wait for it to be completed. Command completion is determined by responses received from the robot.
void performCommands(string commands) {		
	// Multiple commands can be encoded in one QR code, separated by new line sumbols, here commands are processes one by one	
	stringstream commandStream(commands);
	string commandLine;
	while (getline(commandStream, commandLine, '\n')) {	// Get a single command from the QR code
		CommandTelnet command(commandLine);		
		if (command.getCommandWord() == "forward" || command.getCommandWord() == "backward") { //Break long distances into smaller movements			
			double distance = command.getDistance();
			while (distance > 0) {
				string commandString = command.getCommandWord() + " 0.5";
				if (distance < 0.5) {
					commandString = command.getCommandWord() + " " + to_string(distance);
				}
				CommandTelnet shortCommand(commandString);
				performSingleCommand(shortCommand);
				distance = distance - 0.5;
			}
		}
		else {
			performSingleCommand(command);
		}
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
	waitForSocketResponse(2); // Get 2 responses from the socket

	// Obtaining robot's webcam stream	
	performCommands("publish camera"); 	
	cap = VideoCapture(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}

	performCommands("odometrystart");

	//performCommands("backward 0.4\nright 90");				// QR 1
	//performCommands("right 90\nforward 6");					// QR 2
	//performCommands("backward 1\nleft 90\nforward 3.5");		// QR 3
	//performCommands("left 90\nforward 3\n");					// QR 4
	//performCommands("strobeflash on 1000 40");				// QR 5


	// Processing frames for QR codes
	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);	

	int cycles = 0;
	string command = "";
	string lastCommand = "";

	vector<string> commandBuffer = { "left 90\n", "forward 0.5\n", "backward 0.5\n", "right 90\n" };
	Mat frame;
	bool bSuccess;
	while (true) { // Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
		cout << "Main loop\n";
		
		
		bSuccess = cap.read(frame); // read a new frame from video  
		if (!bSuccess) //if not success, break loop  
		{
			cout << "Cannot read a frame from video stream in main loop" << endl;
			break;
		}
		imshow("Oculus Prime", frame);		

		if (isCommandComplete) { // Fetch a new command from the buffer if the command buffer is not empty
			if (commandBuffer.size() != 0) {
				sendCommand(commandBuffer[0]);
				commandBuffer.erase(commandBuffer.begin());
				isCommandComplete = false;
			}
		}
		else { // Get a response from the robot and set isCommandComplete to true if a response indicating that the command is completed was received
			checkCommandFinish("motion stopped"); 
		}


		/**
		 * Look for QR codes in the frame, decode if a code is found, and perform decoded command
		 * If robot sees the same QR code for a while, it will not repeat the same commands while QR code is in the frame.
		 * However, if QR code is taken away from the frame (ex. robot moves and sees the same command on a different QR code), 
		 * last command is forgotten so the robot would perform newly seen command even if it is the same as the previos command. 
		 */
		//Mat grey;
		//cvtColor(frame, grey, CV_BGR2GRAY);
		//int width = frame.cols;
		//int height = frame.rows;
		//uchar *raw = (uchar *)grey.data;
		//Image image(width, height, "Y800", raw, width * height);
		//int numQRs = scanner.scan(image); // Will return 0 if no QR codes are found
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
	//cin.ignore(numeric_limits<streamsize>::max(), '\n');
	return 0;
}