#include <iostream>
#include <limits>

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

int port_num_telnet = 4444; 
char *IP_ADD = "207.23.183.214"; // Robot IP
string filename = "rtmp://207.23.183.214:1935/oculusPrime/stream1";

void waitForSocketResponse(int responseCycles) {
	cout << "Wait for " << responseCycles << " cycles\n";
	for (int i = 0; i < responseCycles; i++) {
		string response = socketResponse();
		cout << "Response received:\n**********\n" << response << "**********\n" << endl;
	}
}

void performCommand(string commands, int responseCycles) 
{		
	cout << "Sending commands: " << commands << endl;
	// Multiple commands can be encoded in one QR code, separated by new line sumbols. 
	// This piece of code separates commands and processes them one by one	
	stringstream commandStream(commands);
	string commandLine;
	while (getline(commandStream, commandLine, '\n')) {	// Get a single command from the QR code
		CommandTelnet command(commandLine);		
		string commandFull = command.getFullCommand();
		cout << "Single command: " << commandFull;
		socketSend(commandFull.data(), commandFull.length());
		// Need to wait for 3-4 cycles (depends on the command) of robot responding for command to complete, otherwise command might not complete
		waitForSocketResponse(command.getResponseNum());
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
	performCommand("publish camera\n", 2); 
	VideoCapture cap(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}

	// Testing code for the experiment, will go on QR code later
	performCommand("right 180\nforward 0.1\nleft 90", 4); // QR1
	//waitForSocketResponse(1);
	//performCommand("right 90\n forward 1\n", 4); // QR2
	//waitForSocketResponse(1);
	//performCommand("backward 1\nleft 90\nforward 3", 4); // QR3
	//waitForSocketResponse(1);
	//performCommand("left 90\nforward 4\n", 4); // QR4
	//waitForSocketResponse(1);
	//performCommand("strobeflash on\n", 4); // QR5

	// Processing the frames for the QR codes
	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);		
	int cycles = 0;
	string command = "";
	string lastCommand = "";
	
	while (1)
	{
		Mat frame;
		bool bSuccess = cap.read(frame); // read a new frame from video  
		if (!bSuccess) //if not success, break loop  
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}
		imshow("Oculus Prime", frame);

		Mat grey;
		cvtColor(frame, grey, CV_BGR2GRAY);
		int width = frame.cols;
		int height = frame.rows;
		uchar *raw = (uchar *)grey.data;
		Image image(width, height, "Y800", raw, width * height);

		// Scan the image for QR codes   
		int n = scanner.scan(image);
		if (n == 0) { // Reset commands if there is no QR code in the frame for 5 cycles (arbitrary value, might need to be adjusted)
			if (cycles >= 5) { // Wait for 5 cycles, QR code is not detected each cycle
				if (lastCommand != "" || command != "") {
					cout << "No QR codes detected for 5 cycles, reset commands\n";
					lastCommand = "";
					command = "";
				}				
			}
			else {
				cycles++; // Increment cycles each time no QR is detected;
			}
		}
		else {
			cycles = 0; // Reset cycles each time QR code is detected
		}
		
		// Extract results of QR decoding, perform decoded command
		for (SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
			lastCommand = command;
			command = symbol->get_data();
			cout << "Decoded command: " << command << endl;
			cout << "Previous command was: " << lastCommand << endl;
			if ((command != "") && (command != lastCommand)) {		// Ignore repeating the same command over and over while QR code is in the camera frame					
				cout << "Perform command " << command << endl;
				performCommand(command, 3);
			}
		}

		// Wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
		if (waitKey(30) == 27) 
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}
	closeConnection();
	return 0;
}