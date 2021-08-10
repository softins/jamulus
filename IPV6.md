# Adding IPv6 support to Directory Servers

Some notes for discussion about adding Directory support for IPv6.

## Server registration

* All servers must still support IPv4 connections.

* If a server also supports IPv6, this should be available from the same
  server entry. i.e. there should not be two entries in the Directory
  for the one server.

* Servers must register with a Directory Server using IPv4, never IPv6.
  This is because the Directory registers the server using the IPv4 address
  and port number from which the registration request originates.

* The existing server registration message contains the local IPv4 address
  of the registering server's public interface. If the server is behind NAT,
  this will be the LAN address, not the WAN address (the WAN address is the
  address from which the registration packet arrives, as mentioned in the above point).
  The Directory stores both the WAN and LAN addresses for the registering server.

* Servers will initially register for IPv4 using the existing protocol, for compatibility
  with Directory Servers that do not support IPv6.

* A Server that supports IPv6 will follow up with an enhanced registration message
  that additionally contains the IPv6 address of its public interface, as well as
  all the information currently in the existing registration message. This message
  will be ignored by a Directory Server that does not support IPv6. If the Directory
  server does support IPv6, it will respond with a registration successful message.
  This might also need to be a new message for unambiguity.

* The Directory Server receiving the enhanced registration message (over IPv4) will create or update the
  existing Server entry to include the supplied IPv6 address too.

* A server that receives the enhanced registration successful message can note that
  the Directory Server support IPv6, and could elect to send only the enhanced registration
  message for registration renewals.

* A Directory Server that supports IPv6 and receives a legacy registration message for
  a Server it knows about and has an IPv6 address for must not clear the IPv6 address it holds.

## Client operation

* It might be worthwhile to put a "Use IPv6" checkbox in the Advanced Settings and store it in the ini file.

* A client that does not support IPv6 will request the server list from the Directory
  as currently. It will receive the existing format list that does not contain IPv6
  addresses.

* A client that supports IPv6 must still send the request for the server list over IPv4.
  This is because most listed servers will still need to send an empty message to the
  client's IPv4 address.

* A client that supports IPv6 may send an enhanced server list request. A Directory Server
  that does not understand the message, or does not support IPv6, will ignore this request.

* A server that understands the enhanced request and supports IPv6 will respond with a server list
  that includes both IPv4 and (if available) IPv6 addresses for the servers.

* A client that receives the enhanced server list can continue just to send the enhanced list request
  to that Directory Server. Each time the client user changes list, the client will start by sending
  both the legacy and enhanced list request.

* !!! Currently, when the Directory sends the list to a client, it also tells all the servers
  in that list to send an empty message to the client's IPv4 address and port. This IPv4 address
  and port are taken from the source address/port of the list request. What should we do about
  servers that are registered with both IPv4 and IPv6 addresses, when sending a client the
  enhanced server list? Possibilities:

  - The client sends the legacy list request from IPv4 and the enhanced request from IPv6?

  - The client sends both from IPv4, but includes its IPv6 address in the enhanced request?

  - Should we support a Directory Server that can only communicate on IPv4, but that does
    support the storing of servers' supplied IPv6 addresses? We probably should. If so,
    then the client needs to send its IPv6 address in the enhanced request.

* If the directory server is serving an enhanced list, it should tell all listed servers to
  send the normal empty message to the client's IPv4 address, and those servers registered with
  an IPv6 address also to send an empty message to the client's IPv6 address.

* !!! What about the pings performed by the Connect dialog? If a client supports IPv6, and a
  server has both IPv4 and IPv6 addresses, we should probably list it twice in the list, and
  send a ping to both addresses, so the user can observe any difference and choose which to connect to.

