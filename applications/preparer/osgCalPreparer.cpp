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
#include <osgCal/MeshLoader>
#include <osgCal/CoreModel>
#include <osgDB/FileNameUtils>

using namespace osgCal;

void
usage()
{
    puts( "Usage: osgCalPreparer <cal3d.cfg file name>" );
}

int
main( int argc,
      const char** argv )
{
    if ( argc < 2 )
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
   
    std::string cfgFileName = argv[ 1 ];

    std::string dir = osgDB::getFilePath( cfgFileName );

    if ( dir == "" )
    {
        dir = ".";
        cfgFileName = "./" + cfgFileName;
    }

    printf( "Preparing %s  ...  ", cfgFileName.c_str() );;
    fflush( stdout );
        
    CalCoreModel* calCoreModel = 0;
    float scale;
    osgCal::MeshesVector meshesData;

    BRACKET_ERROR( calCoreModel = loadCoreModel( cfgFileName, scale ),
                   "Can't load model:\n%s" );
    BRACKET_ERROR( loadMeshes( calCoreModel, meshesData ),
                   "Can't load meshes from core model:\n%s" );
    BRACKET_ERROR( saveMeshes( calCoreModel,
                               meshesData,
                               meshesCacheFileName( cfgFileName ) ),
                   "Can't save meshes cache:\n%s" );

    delete calCoreModel;

    puts( "ok" );
    
    return 0;
}
