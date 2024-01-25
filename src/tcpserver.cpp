/******************************************************************************\
 * Copyright (c) 2024
 *
 * Author(s):
 *  Tony Mountifield
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

#include "global.h"
#include "tcpserver.h"

CTcpServer::CTcpServer ( QObject* parent, const QString& strServerBindIP, int iPort, bool bEnableIPv6 ) :
    QObject ( parent ),
    strServerBindIP ( strServerBindIP ),
    iPort ( iPort ),
    bEnableIPv6 ( bEnableIPv6 ),
    pTcpServer ( new QTcpServer ( this ) )
{
    connect ( pTcpServer, &QTcpServer::newConnection, this, &CTcpServer::OnNewConnection );
}

CTcpServer::~CTcpServer()
{
    if ( pTcpServer->isListening() )
    {
        qInfo() << "- stopping Jamulus-TCP server";
        pTcpServer->close();
    }
}

bool CTcpServer::Start()
{
    if ( iPort < 0 )
    {
        return false;
    }

    // default to any-address for either both IP protocols or just IPv4
    QHostAddress hostAddress = bEnableIPv6 ? QHostAddress::Any : QHostAddress::AnyIPv4;

    if ( !bEnableIPv6 )
    {
        if ( !strServerBindIP.isEmpty() )
        {
            hostAddress = QHostAddress ( strServerBindIP );
        }
    }

    if ( pTcpServer->listen ( hostAddress, iPort ) )
    {
        qInfo() << qUtf8Printable (
            QString ( "- Jamulus-TCP: Server started on %1:%2" ).arg ( pTcpServer->serverAddress().toString() ).arg ( pTcpServer->serverPort() ) );
        return true;
    }
    qInfo() << "- Jamulus-TCP: Unable to start server:" << pTcpServer->errorString();
    return false;
}

void CTcpServer::OnNewConnection()
{
    QTcpSocket* pSocket = pTcpServer->nextPendingConnection();
    if ( !pSocket )
    {
        return;
    }

    // express IPv4 address as IPv4
    QHostAddress peerAddress = pSocket->peerAddress();

    if ( peerAddress.protocol() == QAbstractSocket::IPv6Protocol )
    {
        bool    ok;
        quint32 ip4 = peerAddress.toIPv4Address ( &ok );
        if ( ok )
        {
            peerAddress.setAddress ( ip4 );
        }
    }

    qDebug() << "- Jamulus-TCP: received connection from:" << peerAddress.toString();

    connect ( pSocket, &QTcpSocket::disconnected, [this, pSocket, peerAddress]() {
        qDebug() << "- Jamulus-TCP: connection from:" << peerAddress.toString() << "closed";
        pSocket->deleteLater();
    } );

    connect ( pSocket, &QTcpSocket::readyRead, [this, pSocket]() {
        // handle received Jamulus protocol packet
    } );
}

#if 0
void CTcpServer::Send ( QTcpSocket* pSocket ) {
    // pSocket->write ( );
}
#endif
