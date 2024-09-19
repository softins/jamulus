# Notes regarding TCP use in a connected session
## 5 Sep 2024

### Current operation when client clicks on Connect

All these steps use UDP.

1. Client starts sending an audio stream to the server. This audio stream continues in parallel with the protocol exchange below.

   a. Server does not yet start sending an audio stream to the client.

2. Server sees the audio stream and looks up the source IP:port in its channel table, finding no channel that matches.

3. Server allocates a new channel in the channels array and stores the source IP:port in it. The channel index becomes the Channel ID of the connected client.

4. Server sends a `CLIENT_ID` message to the client, containing the Channel ID mentioned above.

   a. Client replies with `ACKN (CLIENT_ID)`. *Note that all messages that do not begin `CLM_` need to be acked by the receiving side. For clarity, these `ACKN` messages will not be mentioned below.*

5. Server sends `CONN_CLIENTS_LIST` to the client, containing a list of all current clients in the session.

6. Server sends `REQ_SPLIT_MESS_SUPPORT` to ask the client if it supports split messages.

7. Client sends back `SPLIT_MESS_SUPPORTED` immediately.

7. Server sends `REQ_NETW_TRANSPORT_PROPS` to ask for the clients network transport parameters.

8. Client sends `NETW_TRANSPORT_PROPS` containing the codec, packet size, number of channels, bitrate, etc.

9. Server sends `REQ_JITT_BUF_SIZE` to ask for the client's required jitter buffer sizes.

10. Client sends `JIT_BUF_SIZE`, containing the positions of the "server" jitter buffer slider in the Settings dialog. This is telling the server what size jitter buffer to use for receiving audio data from the client. (The position of the "client" jitter buffer slider is not needed by the server, as it is only used locally in the client).

11. Server sends `REQ_CHANNEL_INFOS` to ask for the identity information for the channel.

12. Client sends `CHANNEL_INFOS` containing the identity information from the user's profile settings in the client (country, instrument, skill level, name, city).

13. Now that the server has received the `CHANNEL_INFOS` from the client, it starts to send the mixed audio stream to the client.

14. Server sends `CHAT_TEXT` containing the server welcome message, if any. If there is none, this message is skipped.

15. Server sends `VERSION_AND_OS` to tell the client the version of Jamulus on the server and the server platform.

After this, messages are sent by either side when there is something to notify:

* From server to client:

  - `CLM_CHANNEL_LEVEL_LIST` - list of audio levels for each channel. Sent every 250ms by a timer.
  - `RECORDER_STATE` - current state of the server-based recording. Sent when the state changes?
  - `JITT_BUF_SIZE` - the size of the receiving jitter buffer for this connection on the server. Sent in Auto mode when the value changes.
  - `CLM_PING_MS` - sent in response to a `CLM_PING_MS` received from the client. For client-side ping time calculation.
  - `CONN_CLIENTS_LIST` - list of connected clients. Sent when the list changes due to a client connecting or leaving. This message could be large on a server with many clients.

* From client to server:

  - `CLM_PING_MS` - contains a timestamp and requests the server to send back the same timestamp, so that the round-trip time can be measured. Sent every 500ms by a timer.
  - `NETW_TRANSPORT_PROPS` - specifies codec, packet size, bitrate, etc. Sent when the user changes Audio Channels, Audio Quality, Buffer Delay or Small Network Buffers.
  - `CHANNEL_GAIN` - specifies the user's requested gain for a specific channel. Sent when the user moves a fader, but rate limited to avoid many changes in succession being sent.
  - `JITT_BUF_SIZE` - specifies the requested size of the server's jitter buffer, or Auto.
  - `CLM_DISCONNECTION` - sent when the user clicks on Disconnect, or connects to another server.

### Enhancement for TCP support

1. As soon as the server sees audio from a new client and creates a channel for it, it will send `CLM_TCP_SUPPORTED` to the client.

2. The server will then send the `CLIENT_ID` message as above, containing the channel ID that has been allocated.

3. An older version of client that does not support TCP will ignore the `CLM_TCP_SUPPORTED` message and continue operating in the normal way just on UDP.

4. A newer client that supports TCP will receive the `CLM_TCP_SUPPORTED` message, and will note that the server supports TCP.

5. When a newer client receives the `CLIENT_ID` message from a server it knows supports TCP, the client will open a TCP connection to the server (on the same port number as UDP), and as its first message over the connection it will send a `CLIENT_ID` message containing the channel ID that it received over UDP.

6. The server will accept the TCP connection, and will wait for the first message to arrive via that connection. This should be the `CLIENT_ID` message.

7. The server will lookup the channel specified by the `CLIENT_ID` message, and *will check that the IP address of the channel matches the remote address of the TCP connection*. If it does not, it will close the connection. This prevents hijacking of a session by sending another client's ID.

8. If the TCP connection matches the client channel, the socket descriptor will be stored in the channel.

9. Any messages from the client that arrive over TCP will be handled in the same way as messages received over UDP. Responses will be send back over TCP too.

10. Messages generated by the server will also be sent over TCP, if there is an active socket descriptor stored for the channel. If not, they will be sent over UDP in the normal way.

11. Audio packets will continue to always use the UDP socket.

NOTE THAT PING MESSAGES MUST STILL GO OVER UDP, SO THAT PING CALCULATIONS ARE NOT AFFECTED BU POTENTIAL TCP RETRIES. MAYBE ALSO THE LEVEL MESSAGES.

Also say something about disconnecting.

if the server receivs a disconnection of the TCP socket, it will revert to UDP. it could then send another `CLM_TCP_SUPPORTED` to invite the client to re-establish a TCP connection.

