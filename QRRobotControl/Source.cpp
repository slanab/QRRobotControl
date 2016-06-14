#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <iostream>

#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <zbar.h>  

#pragma comment(lib, "Ws2_32.lib")

using namespace cv;
using namespace std;
using namespace zbar;

SOCKET s; //Socket handle
int port_num = 4444;
char *IP_ADD = "207.23.183.214";
bool motor_fun(char *, char *);

//CONNECTTOHOST – Connects to a remote host
int ConnectToHost(int PortNo, char* IPAddress)
{
	//Start up Winsock…
	WSADATA wsadata;
	int error = WSAStartup(0x0202, &wsadata);

	//Did something happen?
	if (error)
	{
		printf("error1");
		return false;
	}

	//Did we get the right Winsock version?
	if (wsadata.wVersion != 0x0202)
	{
		WSACleanup(); //Clean up Winsock
		printf("error2");
		return false;
	}

	//Fill out the information needed to initialize a socket…
	SOCKADDR_IN target; //Socket address information
	target.sin_family = AF_INET; // address family Internet
	target.sin_port = htons(PortNo); //Port to connect on
	target.sin_addr.s_addr = inet_addr(IPAddress); //Target IP

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //Create socket
	if (s == INVALID_SOCKET)
	{
		printf("error3");
		return false; //Couldn't create the socket
	}

	//Try connecting...
	if (connect(s, (SOCKADDR *)&target, sizeof(target)) == SOCKET_ERROR)
	{
		printf("error4");
		return 0; //Couldn't connect
	}
	else
		printf("success accured");
	return 1; //Success
}

//CLOSECONNECTION – shuts down the socket and closes any connection on it
void CloseConnection()
{
	//Close the socket if it exists
	if (s)
		closesocket(s);

	WSACleanup(); //Clean up Winsock
}

void performCommand(string command) {
	if (command != "") {
		cout << "Command to be performed " << command << endl;		
		send(s, command.data(), 13, 0);		
		int iResult;
		int buffer_len = 200;
		char  buffer[200];
		iResult = recv(s, buffer, buffer_len, 0);
		cout << buffer << '\n';
	} else {
		// No command received
	}
}

int main(int argc, char* argv[])
{
	// Obtaining robot's webcam stream
	string filename = "rtmp://207.23.183.214:1935/oculusPrime/stream1";
	VideoCapture cap(filename);
	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot open the video cam" << endl;
		return -1;
	}
	double dWidth = cap.get(CV_CAP_PROP_FRAME_WIDTH); //get the width of frames of the video  
	double dHeight = cap.get(CV_CAP_PROP_FRAME_HEIGHT); //get the height of frames of the video  
	cout << "Frame size : " << dWidth << " x " << dHeight << endl;
	namedWindow("MyVideo", CV_WINDOW_AUTOSIZE); //create a window called "MyVideo"

	if (!ConnectToHost(port_num, IP_ADD))
	{
		return 0;
	}	

	// Processing the frames for the QR codes
	ImageScanner scanner;
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);		
	while (1)
	{
		Mat frame;
		bool bSuccess = cap.read(frame); // read a new frame from video  
		if (!bSuccess) //if not success, break loop  
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}
		Mat grey;
		cvtColor(frame, grey, CV_BGR2GRAY);
		int width = frame.cols;
		int height = frame.rows;
		uchar *raw = (uchar *)grey.data;
		// wrap image data   
		Image image(width, height, "Y800", raw, width * height);
		// scan the image for barcodes   
		int n = scanner.scan(image);
		string command = "";
		// extract results   
		for (Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
			vector<Point> vp;
			// do something useful with results   
			command = symbol->get_data();
			cout << "decoded " << symbol->get_type_name() << " symbol \"" << command << '"' << " " << endl;
			int n = symbol->get_location_size();
			for (int i = 0; i<n; i++) {
				vp.push_back(Point(symbol->get_location_x(i), symbol->get_location_y(i)));
			}
		}
		putText(frame, command, cvPoint(10, 100), FONT_HERSHEY_COMPLEX_SMALL, 4, cvScalar(0, 0, 0), 1, CV_AA);
		imshow("MyVideo", frame); //show the frame in "MyVideo" window  
		performCommand(command);
		if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop  
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}
	CloseConnection();
	return 0;
}