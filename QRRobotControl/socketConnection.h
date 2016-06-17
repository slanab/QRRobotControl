using namespace std;

int connectToHost(int PortNo, char* IPAddress);
void socketSend(const char* command, int len);
string socketResponse();
void closeConnection();