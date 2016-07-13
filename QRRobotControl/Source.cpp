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
		int position = response.find("motion stopped");
		if (position != -1) {
			cout << "Position of motion " << position << endl;
			break;
		}
	}
}

void performCommand(string commands) 
{		
	cout << "Sending commands: " << commands << endl;
	// Multiple commands can be encoded in one QR code, separated by new line sumbols, here commands are processes one by one	
	stringstream commandStream(commands);
	string commandLine;
	while (getline(commandStream, commandLine, '\n')) {	// Get a single command from the QR code
		CommandTelnet command(commandLine);		
		string commandFull = command.getFullCommand();
		cout << "Single command: " << commandFull;
		socketSend(commandFull.data(), commandFull.length());
		// Need to wait for a few cycles (depends on the command) of responses for command to complete, otherwise command might not complete
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
	performCommand("publish camera\n"); 
	VideoCapture cap(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}

	// Testing code for the experiment 
	//performCommand("backward 0.4\nright 90"); // QR1
	//performCommand("right 90\n forward 7\n"); // QR2
	//performCommand("backward 1\nleft 90\nforward 3.5"); // QR3
	//performCommand("left 90\nforward 5.5\n"); // QR4
	//performCommand("strobeflash on\n"); // QR5

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
				}
			}
			
		} else { // No QR detected
			if (cycles >= 5) { // Reset commands if there is no QR code in the frame for 5 cycles (arbitrary value, might need to be adjusted)
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