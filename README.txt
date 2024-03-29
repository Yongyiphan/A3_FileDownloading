Server Parameters
1. Server TCP Port number
2. Server UDP Port number
3. Search Directory for download
4. Sliding Window size (between 1 to 100)
5. Packet loss rate (between 0.0f to 1.0f)
6. Ack Timer (between 10ms to 500ms)

Client Parameters
1. Server IP Address (get from server console)
2. Server's TCP Port number
3. Server's UDP Port number
4. Client's UDP Port number
5. Directory to store downloaded files
6. Sliding window size (refer to Server paramters)
7. Packet loss rate ( refer to Server parameters)


Available Commands:
/q for quiting client
/l for listing downloadable files

Extras:
Most common functions between Server.cpp and Client.cpp have been implemented within Utils.h/.cpp for convenience.
