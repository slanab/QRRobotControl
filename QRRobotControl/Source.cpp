#include <iostream>
#include <limits>

#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <zbar.h>  

#include "socketConnection.h"

using namespace cv;
using namespace std;
using namespace zbar;

int port_num_telnet = 4444; 
char *IP_ADD = "207.23.183.214"; // Robot IP
string filename = "rtmp://207.23.183.214:1935/oculusPrime/stream1";

void performCommand(string commands, int responseCycles) 
{		
	cout << "Sending commands: " << commands;
	// Break down commands into one by one	
	string command = "";
	stringstream commandStream(commands);
	while (getline(commandStream, command, '\n')) {
		cout << "Single command: " << command;
		command = command + '\n';	// Add new line to the command, else command won't be sent
		socketSend(command.data(), command.length());
		for (int i = 0; i < responseCycles; i++) {
			string response = socketResponse();
			cout << "Response received:\n**********\n" << response << "**********\n" << endl;
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
	string response;
	response = socketResponse();
	response = socketResponse();

	// Obtaining robot's webcam stream	
	performCommand("publish camera", 2); // 2 responses

	VideoCapture cap(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot connect to the webcam" << endl;
		cin.ignore(numeric_limits<streamsize>::max(), '\n');
		return -1;
	}

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