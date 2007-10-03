/*
    Copyright (C) 2007 Vladimir Shabanov <vshabanoff@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <sys/stat.h>
#include <osgCal/IOUtils>
#include <osgDB/FileNameUtils>

using namespace osgCal;

void
usage()
{
    puts( "Usage: osgCalPreparer [-force] <cal3d.cfg file name>" );
}

bool
isFileOlder( const std::string& f1,
             const std::string& f2 )
{
    struct stat stat1;
    struct stat stat2;

    if ( stat( f1.c_str(), &stat1 ) != 0 )
    {
        throw std::runtime_error( "isFileOlder: stat error for " + f1 );
    }

    if ( stat( f2.c_str(), &stat2 ) != 0 )
    {
        throw std::runtime_error( "isFileOlder: stat error for " + f2 );
    }

    return ( stat1.st_mtime < stat2.st_mtime );
}


int
main( int argc,
      const char** argv )
{
    if ( Cal::LIBRARY_VERSION != 1000 && Cal::LIBRARY_VERSION != 1100
         && Cal::LIBRARY_VERSION != 1200 )
    {
        perror( "error: osgCalPreparer tested only on cal3d version 0.10.0, 0.11.0 & 0.12.0" );
        return 2;
    }
    
    if ( argc < 2 )
    {
        usage();
        return 2;
    }

    bool forced = stricmp( argv[1], "-force" ) == 0;

    if ( forced && argc < 3 )
    {
        usage();
        return 2;
    }

#define BRACKET_ERROR( _action, _errorPrefix )  \
    try                                         \
    {                                           \
        _action;                                \
    }                                           \
    catch ( std::runtime_error& e )             \
    {                                           \
        printf( _errorPrefix, e.what() );       \
        return 2;                               \
    }
   
    std::string cfgFileName = argv[forced ? 2 : 1];

    std::string dir = osgDB::getFilePath( cfgFileName );

    if ( dir == "" )
    {
        dir = ".";
        cfgFileName = "./" + cfgFileName;
    }

    try // try check for cached vbos
    {
        if ( !forced
             &&
             ::isFileOlder( cfgFileName, VBOsCacheFileName( cfgFileName ) )
             &&
             ::isFileOlder( cfgFileName, HWModelCacheFileName( cfgFileName ) ) )
        {
            printf( "Skipping  %s  ...  ok\n", cfgFileName.c_str() );;
            return 0;
        }
        else
        {
            throw std::runtime_error( "cache is older than .cfg" );
        }
    }
    catch ( std::runtime_error& e )
    {
        printf( "Preparing %s  ...  ", cfgFileName.c_str() );;
        fflush( stdout );
        
        CalCoreModel* calCoreModel = 0;
        float scale;
        CalHardwareModel* calHardwareModel = 0;
        VBOs* bos = 0;
        std::vector< std::string > meshNames;

        BRACKET_ERROR( calCoreModel = loadCoreModel( cfgFileName, scale ),
                       "Can't load model:\n%s" );

        calHardwareModel = new CalHardwareModel( calCoreModel );
        BRACKET_ERROR( bos = loadVBOs( calHardwareModel ),
                       "Can't load vbos from hardware model:\n%s" );
        for(int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount();
            hardwareMeshId++)
        {
            meshNames.push_back(
                calCoreModel->
                getCoreMesh( calHardwareModel->getVectorHardwareMesh()[ hardwareMeshId ].meshId )->
                getName() );
        }

        BRACKET_ERROR( saveVBOs( bos, VBOsCacheFileName( cfgFileName ) ),
                       "Can't save vbos cache:\n%s" );
        BRACKET_ERROR( saveHardwareModel( calHardwareModel,
                                          calCoreModel,
                                          meshNames,
                                          HWModelCacheFileName( cfgFileName ) ),
                       "Can't save hardware model cache:\n%s" );

        delete bos;
        delete calHardwareModel;
        delete calCoreModel;

        puts( "ok" );
    }
    
    return 0;
}
