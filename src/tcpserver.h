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

#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>
#include <memory>

/* Classes ********************************************************************/
class CTcpServer : public QObject
{
    Q_OBJECT

public:
    CTcpServer ( QObject* parent, const QString& strServerBindIP, int iPort, bool bEnableIPv6 );
    virtual ~CTcpServer();

    bool Start();

private:
    const QString strServerBindIP;
    const int     iPort;
    const bool    bEnableIPv6;
    QTcpServer*   pTcpServer;

protected slots:
    void OnNewConnection();
};
