/***************************************************************************
 *   Copyright (C) 2008 by Heiko Koehn                                     *
 *   KoehnHeiko@googlemail.com                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <map>
#include <QDir>
#include <iostream>
#include <fstream>
#include <QStringList>

#include "HostPool.h"
#include "Host.h"
#include <QMutex>
#include <QMutexLocker>

bool HostPool::deleteHost(QString hostname)
{
    QMutexLocker locker(& mPoolLock);
	
    // make sure this is really a new host
    if( mHostPool.find( hostname ) == mHostPool.end() )
    {
        return false;
    }
    else
    {
        delete mHostPool[hostname];
        mHostPool.remove( hostname );
    }
    return true;
}

bool HostPool::renameHost(QString hostname)
{
    QMutexLocker locker(& mPoolLock);
	
    // make sure this is really a new host
    if( mHostPool.find( hostname ) == mHostPool.end() )
    {
        return false;
    }
    else
    {
        //Host * pNewHost = getHost( hostname ); // see why it doesn't work
        Host * pNewHost = mHostPool[hostname];
        mHostPool.remove( hostname );
        mHostPool.insert(pNewHost->getName(), pNewHost);
    }

    return true;

}

bool HostPool::addNewHost( QString hostname, QString port, QString login, QString pass )
{
    QMutexLocker locker(&mPoolLock);	
    
    // make sure this is really a new host
    if( mHostPool.find( hostname ) != mHostPool.end() )
    {
        return false;
    }
    
    int portnumber = 23;
    if(port!="")
    {
        portnumber = port.toInt();
    }
    int id = createNewHostID();
    Host * pNewHost = new Host( portnumber, hostname, login, pass, id );
    
    if( pNewHost == 0 ) //enough memory?
    {
        return false;
    }
    
    mHostPool[hostname] = pNewHost;
    return true;
}

int HostPool::createNewHostID()
{
    return (mHostPool.size() + 1);    
}

QStringList HostPool::getHostList()
{
    QMutexLocker locker(& mPoolLock);
   
    QStringList strlist;
    QList<QString> hostList = mHostPool.keys();
    qDebug()<<"hostList ="<<hostList<<"size="<<hostList.size()<<"keys="<<mHostPool.keys()<<"isEmpty="<<mHostPool.isEmpty()<<"dump="<<mHostPool;
    if( hostList.size() > 0 ) strlist << hostList;
    return strlist;
}



QList<QString> HostPool::getHostNameList()
{
    QMutexLocker locker(& mPoolLock);
    
    return mHostPool.keys();
    /*
    QList<QString> strlist;
    
    typedef QMap<QString, Host*>::iterator IT;
    for( IT it=mHostPool.begin(); it!=mHostPool.end(); it++ )
    {
        strlist.push_back(it->first);
        qDebug() << "host=" << it->first;
    }
    return strlist;*/
}

void HostPool::orderShutDown()
{
    QMutexLocker locker(& mPoolLock);
    
    QList<Host*> hostList = mHostPool.values();
    for( int i=0; i<hostList.size(); i++ )
    {
        hostList[i]->orderShutDown();
    }
    /*
    typedef QMap<QString, Host*>::iterator I;
    for(I it=mHostPool.begin(); it!=mHostPool.end(); it++ )
    {
        (it->second)->orderShutDown();
    } */
}

Host * HostPool::getHost( QString hostname )
{
    QMutexLocker locker(& mPoolLock);
    if( mHostPool.find( hostname ) != mHostPool.end() )
    {
        // host exists
        return mHostPool[hostname];
    }
    else 
    {
        return 0;
    }
}

Host * HostPool::getHostFromHostID( int id )
{
    QMutexLocker locker( & mPoolLock );
    QMapIterator<QString, Host *> it(mHostPool);
    while( it.hasNext() ) 
    {
        it.next();
        Host * pHost = it.value();
        if( pHost->getHostID() == id )
        {
            return pHost;
        }
    }
    qDebug()<<"ERROR: didnt find requested id in hostpool";
    return 0;
}

Host * HostPool::getFirstHost()
{
    QMutexLocker locker(& mPoolLock);     
    Host * pHost = mHostPool.begin().value();
    return pHost;
}

Host * HostPool::getNextHost( QString LastHost )
{
    QMutexLocker locker(& mPoolLock);
    if( mHostPool.find( LastHost ) != mHostPool.end() )
    {
        //ok host exists get next one
        QMap<QString, Host*>::iterator it;
        if( ++it != mHostPool.end() )
        {  
            Host * pHost = mHostPool[it.key()];
            return pHost;
        }
    }
    else 
    { 
        return 0;
    }		 
    Host * pHost = mHostPool[mHostPool.begin().key()];
    return pHost;
}

bool HostPool::serialize( QString directory )
{
    QMutexLocker locker(& mPoolLock);
    QString filename = directory + "/HostPool.dat";
    QDir dir;
    if( ! dir.exists( directory ) )
    {
        dir.mkpath( directory );    
    }
    QFile file( filename );
    file.open(QIODevice::WriteOnly);
    QDataStream ofs(&file); 
    
    ofs << mHostPool.size();
    typedef QMap<QString, Host*>::iterator IT;
    for( IT it=mHostPool.begin(); it!=mHostPool.end(); it++ )
    {
        ofs << it.key(); //host pool selbst serialisen
        QString host_directory = directory + it.key();
        if( ! it.value()->serialize() ) //jeder hostname wird ein directory f�r Hostinfo, JobPool und DirData
        {
            file.close();
            qDebug() << "ERROR: can't serialize " << it.key() << endl;
            return false;
        }
    }
    file.close();   
    return true;
}

bool HostPool::restore( QString directory )
{
    QString filename = directory + "/HostPool.dat";
    QFile file( filename );
    file.open(QIODevice::ReadOnly);
    QDataStream ifs(&file); 
    int hostCount;
    ifs >> hostCount;
    for( int i=0; i<hostCount; i++ )
    {
        QString hostname;
        ifs >> hostname;
        qDebug() << "restoring " << hostname;
        QString nothing = "";
        Host * pH = new Host( 0, hostname, nothing, nothing, createNewHostID() );				
        mHostPool[hostname] = pH;
        if( ! pH->restore( directory + hostname, -1 ) )
        {
            return false;
        }
        if( hostname.size() < 1 )
        {
            mHostPool.remove( QString("") );    
        }
    }
    
    file.close();
    return true;
}

Host * HostPool::importHost( QString path )
{
    QString nothing = "";
    Host * pH = new Host( 0, nothing, nothing, nothing, createNewHostID() );    
    
    if( ! pH->importHost( path ) )
    {
        return 0;
    }
    mHostPool[pH->getName()] = pH;
    return pH;
}


