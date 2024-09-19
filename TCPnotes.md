# Notes regarding TCP implementation
## 4 Sep 2024

I have picked this up again and revised the proposed sequence of operations for TCP support.

The summary is that TCP need only be used as a fallback when it is determined that a UDP message failed due to fragmentation, and that the directory or server explicitly supports TCP.

### Current operation when client opens Connect dialog

All these steps use UDP.

1. Client sends `CLM_REQ_SERVER_LIST` to the selected directory server to ask for a list of registered servers. It then starts a 2.5 sec re-request timer.

2. Directory server fetches its internal list of registered servers, and sends a `CLM_SEND_EMPTY_MESSAGE` to each listed server, with the IP and UDP port of the requesting client as parameters.

3. Directory server sends `CLM_RED_SERVER_LIST` (reduced server list) to client.

   a. If/when client receives `CLM_RED_SERVER_LIST`, it populates its list of servers with the reduced info, and sets an internal flag to say it has done so. It checks this flag to avoid processing a repeat reduced server list.

   b. If the list is large and fragmented, and the path does not correctly pass fragments, the client will not receive the list.

4. Directory server sends `CLM_SERVER_LIST` to client. It does this immediately after sending the reduced list above.

   a. If/when the client receives `CLM_SERVER_LIST`, it populates its list of servers with the full info, replacing any existing list that contained reduced info. It then stops the 2.5 sec re-request timer mentioned above, so that the server list is not requested again.

   b. If the list is large and fragmented, and the path does not correctly pass fragments, the client will not receive the list, and the request time will be left running to retry.
   
5. While client is displaying the server list, it periodically pings each server with `CLM_PING_MS_WITHNUMCLIENTS` including a timestamp in the message.

6. Each pinged server, when it receives the ping, will create a `CLM_PING_MS_WITHNUMCLIENTS` in reply, containing a copy of the received timestamp, and the number of clients currently connected to that server.

7. When the client receives the reply, it can calculate the round-trip time from the received timestamp and the current time.

8. If the number of connected clients returned is different from the previously received number for that server, the client sends a `CLM_REQ_CONN_CLIENTS_LIST` to the server.

   a. The server responds with a list of clients in a `CLM_CONN_CLIENTS_LIST`. Most servers only have a small number of clients connected, and this message is not large enough to need IP fragmentation.

   b. If the server is a large one with many clients connected (e.g. for a choir, big band or WorldJam green room), the client list may be large enough to be fragmented by the IP layer. In that case, it might not be received by the requesting client.

9. If/when the client receives the client list from the server, it can display the list of connected clients under the relevant server, if this is enabled in the GUI.

10. The five steps above continue until the user clicks Connect or closes the dialog.

### Enhancement for TCP support

1. A directory server will have a command-line option (and maybe a setting in the GUI and ini-file) to enable or disable TCP operation. Default TBD.

2. If the directory server has TCP enabled, then *after* it has sent `CLM_SERVER_LIST` by UDP, it will send a new message `CLM_TCP_SUPPORTED` to the client.

   a. An older version of client that does not support TCP will ignore this message and continue operating in the normal way just on UDP.

   b. A newer client that supports TCP should received the `CLM_TCP_SUPPORTED` message *after* it has received and processed the UDP server list, unless fragmentation prevented it from doing the latter.

   c. If such a client has already processed a full server list from `CLM_SERVER_LIST`, it will have no need to open a TCP connection to the directory, so this will be skipped.

   d. If the client receives `CLM_TCP_SUPPORTED` having *not* received and processed a `CLM_SERVER_LIST`, it will open a TCP connection to the directory server, and request the server list again over the TCP connection.

3. If the directory server accepts a TCP connection and receives a `CLM_REQ_SERVER_LIST` over it, it will process the request in the same way as for a UDP request, with the following differences:

   a. There is no need for the directory to send `CLM_SEND_EMPTY_MESSAGE` to the servers in the list, since that was already done in response to the original UDP request.

   b. There is no need for the directory to send `CLM_RED_SERVER_LIST` to the client, since the TCP connection is reliable, so the directory server just sends the `CLM_SERVER_LIST` over the TCP connection.

4. When the client has received the `CLM_SERVER_LIST` over TCP, it closes the TCP connection, populates its list of servers in the connect dialog in the normal way and stops the 2.5 sec re-request timer.

5. The client starts pinging each listed server as normal, using UDP, and the server responds with a ping including the timestamp and number of clients, as described above.

6. As above, if the number of connected clients has changed, the client sends a `CLM_REQ_CONN_CLIENTS_LIST` over UDP in the normal way.

7. If a server in the list supports TCP, when it has sent a reply to the client list request with `CLM_CONN_CLIENTS_LIST`, it will follow it immediately with a `CLM_TCP_SUPPORTED`.

   a. An older version of client that does not support TCP will ignore the `CLM_TCP_SUPPORTED` message and continue operating in the normal way just on UDP.

   b. A newer client that supports TCP should received the `CLM_TCP_SUPPORTED` message *after* it has received and processed the UDP client list, unless fragmentation prevented it from doing the latter.

   c. If such a client has already processed a client list from `CLM_CONN_CLIENTS_LIST`, it will have no need to open a TCP connection to the server, so this will be skipped.

   d. If the client receives `CLM_TCP_SUPPORTED` having *not* received and processed a `CLM_CONN_CLIENTS_LIST`, it will open a TCP connection to the server, and request the client list again over the TCP connection.

8. If the server accepts a TCP connection and receives a `CLM_REQ_CONN_CLIENTS_LIST` over it, it will process the request in the same way as for a UDP request, but will send the reply over the TCP connection.

9. When the client has received the `CLM_CONN_CLIENTS_LIST` over TCP, it closes the TCP connection and updates the list of clients for that server in the GUI.

   a. Consideration was given to keeping the TCP connection open for sending repeat requests, but this would require keeping track of potentially many open connections, one for each server, so closing the connection as soon as the response has been received is easier.

### Conclusion

By sending the `CLM_TCP_SUPPORTED` message immediately *after* sending a potentially large list of servers or connected clients allows a client easily to determine whether or not it needs to fall back to TCP without the necessity of timeouts or other delays. It will only need to use TCP if it has not already succeeded in receiving the message over UDP.

It is assumed that the server will only be configured to offer TCP fallback if the server operator has also configured any firewall to allow the inbound TCP connections.

There is also no need to worry about TCP port numbers, as TCP is only used as a fallback when necessary, and all the port opening and pinging has already been set up with UDP as at present.

If there are no problems seen with the above approach, I plan to get started with implementation over the next few weeks. As mentioned at the start of this Issue, I have had the TCP server side working in test mode for some months.
