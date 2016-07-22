#include <iostream>
#include <limits>
#include <thread>
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

void performCommand(string commands);

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

void waitForSocketResponse(CommandTelnet command) {
	cout << "Wait for " << command.getResponseNum() << " cycles max\n";
	for (int i = 0; i < command.getResponseNum(); i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
		if (command.getLastResponse() != "" && response.find(command.getLastResponse()) != -1) {
			cout << "Found '" << command.getLastResponse() << "' in the response. \n";
			break;
		}
	}
}

void correctAngle(int commandAngle, double realAngle) {	
	double angleDifference = commandAngle - abs(realAngle);	
	if (abs(angleDifference) > 3) {
		cout << "Need to correct angle from " << realAngle << " to " << commandAngle << " Diff " << abs(angleDifference) << endl;		
		int angle = angleDifference;
		
		performCommand("strobeflash on 500 30");
		// Perform command: go left of rotated too much right, vise versa
	}
}

double extractAngle(string response) {
	stringstream responseStream(response);
	string responseLine;
	double realAngle;
	while (getline(responseStream, responseLine, '\n')) {
		cout << "Checking line " << responseLine << endl;
		if (responseLine.find("distanceangle") != -1) {
			cout << "Found distance angle line, start processing\n";
			int pos = 0;
			for (int i = 0; i < 3; i++) {
				pos = responseLine.find(" ", pos);
				cout << "Space is at index " << pos << endl;
				pos++;
			}
			cout << "Last space is at index " << pos << endl;
			realAngle = stod(responseLine.substr(pos));
			break;
		}
	}
	return realAngle;
}

void waitForSocketResponseCorrectAngle(CommandTelnet command) {
	cout << "Called angle correction wait\n";
	bool isFinalResponseReceived = false;
	bool isOdometryReceived = false;
	double realAngle = 0;
	cout << "Wait for " << command.getResponseNum() << " cycles max\n";
	for (int i = 0; i < command.getResponseNum(); i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
		if (command.getLastResponse() != "" && response.find(command.getLastResponse()) != -1) {
			cout << "Found '" << command.getLastResponse() << "' in the response. \n";
			if (isOdometryReceived) {
				break;
			}
			cout << "Need to wait for odometry info\n";			
			isFinalResponseReceived = true;
		}
		if (response.find("distanceangle") != -1) {
			cout << "Odometry received\n";
			isOdometryReceived = true;
			realAngle = extractAngle(response);
			if (isFinalResponseReceived) {
				break;
			}
		}
	}
	correctAngle(command.getAngle(), realAngle);	
}

void waitForSocketResponse(int responseCycles) {
	cout << "Wait for " << responseCycles << " cycles\n";
	for (int i = 0; i < responseCycles; i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	}
}

void sendCommand(string command) {
	command = command + '\n';
	socketSend(command.data(), command.length());
}

// Send command and wait for it to be completed. Command completion is determined by responses received from the robot.
void performCommand(string commands) {		
	cout << "Sending commands: " << commands << endl;
	// Multiple commands can be encoded in one QR code, separated by new line sumbols, here commands are processes one by one	
	stringstream commandStream(commands);
	string commandLine;
	while (getline(commandStream, commandLine, '\n')) {	// Get a single command from the QR code
		CommandTelnet command(commandLine);		
		string commandFull = command.getCommandFull();
		cout << "Single command: " << commandFull << endl; 
		updateFrame(); 
		sendCommand(commandFull);
		if (command.isOdometryNeeded()) {
			waitForSocketResponseCorrectAngle(command); 
		}
		else {
			waitForSocketResponse(command);
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
	performCommand("publish camera"); 	
	cap = VideoCapture(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}

	performCommand("odometrystart");


	//performCommand("forward 1");
	performCommand("left 90");
	//performCommand("right 90");
	//performCommand("backward 1");

	// Testing code for the experiment 	
	//performCommand("backward 0.4\nright 90"); // QR1
	//performCommand("right 90\n forward 7\n"); // QR2
	//performCommand("backward 1\nleft 90\nforward 3.5"); // QR3
	//performCommand("left 90\nforward 5.5\n"); // QR4
	//performCommand("strobeflash on\n"); // QR5

	// Processing frames for QR codes
	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);	

	int cycles = 0;
	string command = "";
	string lastCommand = "";
	
	while (waitKey(30) != 27) { // Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  

		Mat frame;
		bool bSuccess;
		bSuccess = cap.read(frame); // read a new frame from video  
		if (!bSuccess) //if not success, break loop  
		{
			cout << "Cannot read a frame from video stream in main loop" << endl;
			break;
		}
		imshow("Oculus Prime", frame);		

		Mat grey;
		cvtColor(frame, grey, CV_BGR2GRAY);
		int width = frame.cols;
		int height = frame.rows;
		uchar *raw = (uchar *)grey.data;
		Image image(width, height, "Y800", raw, width * height);

		/**
		 * Look for QR codes in the frame, decode if a code is found, and perform decoded command
		 * If robot sees the same QR code for a while, it will not repeat the same commands while QR code is in the frame.
		 * However, if QR code is taken away from the frame (ex. robot moves and sees the same command on a different QR code), 
		 * last command is forgotten so the robot would perform newly seen command even if it is the same as the previos command. 
		 */
		int numQRs = scanner.scan(image); // Will return 0 if no QR codes are found
		if (numQRs) { // Found a QR code in the frame
			cycles = 0; // Reset number of cycles of seeing no QR
			// Extract results of QR decoding, perform decoded command
			for (SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
				lastCommand = command;
				command = symbol->get_data();
				cout << "Decoded command: " << command << endl;
				cout << "Previous command was: " << lastCommand << endl;
				if ((command != "") && (command != lastCommand)) {		// Do not repeat the same command over and over while the same QR code is in the camera frame					
					cout << "Perform command " << command << endl;
					performCommand(command);
					//searchNextQR = true;
				}
			}			
		} else { // No QR detected
			//cout << "No QR detected\n";
			if (cycles >= 5) { // Reset commands if there is no QR code in the frame for 5 cycles (arbitrary value, might need to be adjusted)
				lastCommand = "";
				command = "";
				//cout << "No qr cycles " << cycles << endl;
				if (searchNextQR && cycles >= 20) {
					performCommand("strobeflash on 500 30\nbackward 0.1");
					cycles = 0;
				}
			}
			cycles++; // Increment cycles each time no QR is detected;			
		}
	}
	closeConnection();
	cout << "Press enter to exit";
	//cin.ignore(numeric_limits<streamsize>::max(), '\n');
	return 0;
}