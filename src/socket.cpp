/******************************************************************************\
 * Copyright (c) 2004-2024
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
\******************************************************************************/

#include "socket.h"
#include "server.h"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#endif

/* Implementation *************************************************************/

// Connections -------------------------------------------------------------
// it is important to do the following connections in this class since we
// have a thread transition

// we have different connections for client and server, created after Init in corresponding constructor

CSocket::CSocket ( CChannel* pNewChannel, const quint16 iPortNumber, const quint16 iQosNumber, const QString& strServerBindIP, bool bEnableIPv6 ) :
    pChannel ( pNewChannel ),
    bIsClient ( true ),
    bJitterBufferOK ( true ),
    bEnableIPv6 ( bEnableIPv6 )
{
    Init ( iPortNumber, iQosNumber, strServerBindIP );

    // client connections:
    QObject::connect ( this, &CSocket::ProtocolMessageReceived, pChannel, &CChannel::OnProtocolMessageReceived );

    QObject::connect ( this, &CSocket::ProtocolCLMessageReceived, pChannel, &CChannel::OnProtocolCLMessageReceived );

    QObject::connect ( this, static_cast<void ( CSocket::* )()> ( &CSocket::NewConnection ), pChannel, &CChannel::OnNewConnection );
}

CSocket::CSocket ( CServer* pNServP, const quint16 iPortNumber, const quint16 iQosNumber, const QString& strServerBindIP, bool bEnableIPv6 ) :
    pServer ( pNServP ),
    bIsClient ( false ),
    bJitterBufferOK ( true ),
    bEnableIPv6 ( bEnableIPv6 )
{
    Init ( iPortNumber, iQosNumber, strServerBindIP );

    // server connections:
    QObject::connect ( this, &CSocket::ProtocolMessageReceived, pServer, &CServer::OnProtocolMessageReceived );

    QObject::connect ( this, &CSocket::ProtocolCLMessageReceived, pServer, &CServer::OnProtocolCLMessageReceived );

    QObject::connect ( this,
                       static_cast<void ( CSocket::* ) ( int, int, CHostAddress )> ( &CSocket::NewConnection ),
                       pServer,
                       &CServer::OnNewConnection );

    QObject::connect ( this, &CSocket::ServerFull, pServer, &CServer::OnServerFull );
}

void CSocket::Init ( const quint16 iNewPortNumber, const quint16 iNewQosNumber, const QString& strNewServerBindIP )
{
    udpSocket = new QUdpSocket ( this );

    // default to any-address for either both IP protocols or just IPv4
    QHostAddress hostAddress = bEnableIPv6 ? QHostAddress::Any : QHostAddress::AnyIPv4;

    // first store parameters, in case reinit is required (mostly for iOS)
    iPortNumber     = iNewPortNumber;
    iQosNumber      = iNewQosNumber;
    strServerBindIP = strNewServerBindIP;

    if ( !bEnableIPv6 )
    {
        if ( !strServerBindIP.isEmpty() )
        {
            hostAddress = QHostAddress ( strServerBindIP );
        }
    }

    // allocate memory for network receive and send buffer in samples
    vecbyRecBuf.Init ( MAX_SIZE_BYTES_NETW_BUF );

    // initialize the listening socket
    bool bSuccess;

    if ( bIsClient )
    {
        if ( iPortNumber == 0 )
        {
            bSuccess = udpSocket->bind ( hostAddress, iPortNumber );
        }
        else
        {
            // If the port is not available, try "NUM_SOCKET_PORTS_TO_TRY" times
            // with incremented port numbers. Randomize the start port, in case a
            // faulty router gets stuck and confused by a particular port (like
            // the starting port). Might work around frustrating "cannot connect"
            // problems (#568)
            const quint16 startingPortNumber = iPortNumber + rand() % NUM_SOCKET_PORTS_TO_TRY;

            quint16 iClientPortIncrement = 0;
            do
            {
                bSuccess = udpSocket->bind ( hostAddress, startingPortNumber + iClientPortIncrement );
                iClientPortIncrement++;
            } while ( !bSuccess && ( iClientPortIncrement <= NUM_SOCKET_PORTS_TO_TRY ) );
        }
    }
    else
    {
        // for the server, only try the given port number and do not try out
        // other port numbers to bind since it is important that the server
        // gets the desired port number
        bSuccess = udpSocket->bind ( hostAddress, iPortNumber );
    }

    if ( !bSuccess )
    {
        // we cannot bind socket, throw error
        throw CGenErr ( "Cannot bind the socket (maybe "
                        "the software is already running).",
                        "Network Error" );
    }

    // Set QoS, now that Qt has set the type of socket
    udpSocket->setSocketOption ( QAbstractSocket::TypeOfServiceOption, iQosNumber );
}

void CSocket::Abort() { udpSocket->abort(); }

CSocket::~CSocket() { udpSocket->close(); }

void CSocket::SendPacket ( const CVector<uint8_t>& vecbySendBuf, const CHostAddress& HostAddr )
{
    int status = 0;

    QMutexLocker locker ( &Mutex );

    const int iVecSizeOut = vecbySendBuf.Size();

    if ( iVecSizeOut > 0 )
    {
        // send packet through network (we have to convert the constant unsigned
        // char vector in "const char*", for this we first convert the const
        // uint8_t vector in a read/write uint8_t vector and then do the cast to
        // const char *)

        for ( int tries = 0; tries < 2; tries++ ) // retry loop in case send fails on iOS
        {
            status =
                udpSocket->writeDatagram ( (const char*) &( (CVector<uint8_t>) vecbySendBuf )[0], iVecSizeOut, HostAddr.InetAddr, HostAddr.iPort );

            if ( status >= 0 )
            {
                break; // do not retry if success
            }

#ifdef Q_OS_IOS
            // qDebug("Socket send exception - mostly happens in iOS when returning from idle");
            Init ( iPortNumber, iQosNumber, strServerBindIP ); // reinit

            // loop back to retry
#endif
        }
    }
}

bool CSocket::GetAndResetbJitterBufferOKFlag()
{
    // check jitter buffer status
    if ( !bJitterBufferOK )
    {
        // reset flag and return "not OK" status
        bJitterBufferOK = true;
        return false;
    }

    // the buffer was OK, we do not have to reset anything and just return the
    // OK status
    return true;
}

void CSocket::OnDataReceived()
{
    /*
        The strategy of this function is that only the "put audio" function is
        called directly (i.e. the high thread priority is used) and all other less
        important things like protocol parsing and acting on protocol messages is
        done in the low priority thread. To get a thread transition, we have to
        use the signal/slot mechanism (i.e. we use messages for that).
    */

    const long iNumBytesRead =
        udpSocket->readDatagram ( (char*) &vecbyRecBuf[0], MAX_SIZE_BYTES_NETW_BUF, &RecHostAddr.InetAddr, &RecHostAddr.iPort );

    // check if an error occurred or no data could be read
    if ( iNumBytesRead <= 0 )
    {
        return;
    }

    // convert IPv6-mapped IPv4 address to native IPv4 for later
    if ( RecHostAddr.InetAddr.protocol() == QAbstractSocket::IPv6Protocol )
    {
        bool    ok;
        quint32 ip4 = RecHostAddr.InetAddr.toIPv4Address ( &ok );
        if ( ok )
        {
            RecHostAddr.InetAddr.setAddress ( ip4 );
        }
    }

    // check if this is a protocol message
    int              iRecCounter;
    int              iRecID;
    CVector<uint8_t> vecbyMesBodyData;

    if ( !CProtocol::ParseMessageFrame ( vecbyRecBuf, iNumBytesRead, vecbyMesBodyData, iRecCounter, iRecID ) )
    {
        // this is a protocol message, check the type of the message
        if ( CProtocol::IsConnectionLessMessageID ( iRecID ) )
        {
            //### TODO: BEGIN ###//
            // a copy of the vector is used -> avoid malloc in real-time routine
            emit ProtocolCLMessageReceived ( iRecID, vecbyMesBodyData, RecHostAddr );
            //### TODO: END ###//
        }
        else
        {
            //### TODO: BEGIN ###//
            // a copy of the vector is used -> avoid malloc in real-time routine
            emit ProtocolMessageReceived ( iRecCounter, iRecID, vecbyMesBodyData, RecHostAddr );
            //### TODO: END ###//
        }
    }
    else
    {
        // this is most probably a regular audio packet
        if ( bIsClient )
        {
            // client:

            switch ( pChannel->PutAudioData ( vecbyRecBuf, iNumBytesRead, RecHostAddr ) )
            {
            case PS_AUDIO_ERR:
            case PS_GEN_ERROR:
                bJitterBufferOK = false;
                break;

            case PS_NEW_CONNECTION:
                // inform other objects that new connection was established
                emit NewConnection();
                break;

            case PS_AUDIO_INVALID:
                // inform about received invalid packet by fireing an event
                emit InvalidPacketReceived ( RecHostAddr );
                break;

            default:
                // do nothing
                break;
            }
        }
        else
        {
            // server:

            int iCurChanID;

            if ( pServer->PutAudioData ( vecbyRecBuf, iNumBytesRead, RecHostAddr, iCurChanID ) )
            {
                // we have a new connection, emit a signal
                emit NewConnection ( iCurChanID, pServer->GetNumberOfConnectedClients(), RecHostAddr );

                // this was an audio packet, start server if it is in sleep mode
                if ( !pServer->IsRunning() )
                {
                    // (note that Qt will delete the event object when done)
                    QCoreApplication::postEvent ( pServer, new CCustomEvent ( MS_PACKET_RECEIVED, 0, 0 ) );
                }
            }

            // check if no channel is available
            if ( iCurChanID == INVALID_CHANNEL_ID )
            {
                // fire message for the state that no free channel is available
                emit ServerFull ( RecHostAddr );
            }
        }
    }
}
