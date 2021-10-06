/* -*- c++ -*-
    Copyright (C) 2003 <ryu@gpul.org>
    Copyright (C) 2006 Vladimir Shabanov <vshabanoff@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <memory>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <sstream>

#include <osg/Notify>
#include <osgDB/FileNameUtils>

#include <osgCal/MeshLoader>

#include <osgCal/CoreModel>

using namespace osgCal;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
#define FIELD_OFFSET(type, field)    ((long)&(((type *)0)->field))
////////////////////////////////////////////////////////////////////////////////
template <class K, class T> class DataCmp {
public:
  DataCmp( unsigned int offset ) : _offset(offset)
  {
  }
  bool operator()(const K & lhs, const K & rhs) const
  {
    return (T)*((T*)(((char*)&lhs)+_offset))
         > (T)*((T*)(((char*)&rhs)+_offset));
  }

  unsigned int _offset;
};
////////////////////////////////////////////////////////////////////////////////
CoreModel::CoreModel()
    : calCoreModel( 0 )
{
    stateSetCache = StateSetCache::instance();
//    stateSetCache = new StateSetCache;
}

CoreModel::CoreModel(const CoreModel&, const osg::CopyOp&)
    : Object() // to eliminate warning
{
    throw std::runtime_error( "CoreModel copying is not supported" );
}

#include <cal3d/coretrack.h>

CoreModel::~CoreModel()
{
    if ( calCoreModel )
    {
        // TODO: report CoreTrack memory leak problem to cal3d maintainers
        for ( int i = 0; i < calCoreModel->getCoreAnimationCount(); i++ )
        {
            CalCoreAnimation* a = calCoreModel->getCoreAnimation( i );
            std::list<CalCoreTrack *>& ct = a->getListCoreTrack();
            for ( std::list<CalCoreTrack *>::iterator
                      t = ct.begin(),
                      tEnd = ct.end();
                  t != tEnd; ++t )
            {
                (*t)->destroy();
                delete (*t);
            }
            ct.clear();
        }

        // cleanup of non-auto released resources
        delete calCoreModel;
    }
}

bool
isFileExists( const std::string& f )
{
    struct stat st;

    return ( stat( f.c_str(), &st ) == 0 );
}

void
CoreModel::load( const std::string& cfgFileNameOriginal,
                 MeshParametersSelector* _ps )
{
    if ( calCoreModel )
    {
        // reloading is not supported
        throw std::runtime_error( "model already loaded" );
    }

    osg::ref_ptr< MeshParametersSelector >
        ps( _ps ? _ps : DefaultMeshParametersSelector::instance() );

    std::string dir = osgDB::getFilePath( cfgFileNameOriginal );

    std::string cfgFileName;

    if ( dir == "" )
    {
        dir = ".";
        cfgFileName = "./" + cfgFileNameOriginal;
    }
    else
    {
        cfgFileName = cfgFileNameOriginal;
    }

    MeshesVector meshesData;

    if ( isFileExists( meshesCacheFileName( cfgFileName ) ) == false )
    {
        calCoreModel = loadCoreModel( cfgFileName, scale );
        loadMeshes( calCoreModel, meshesData );
    }
    else
    {
        // We don't check file dates, since after `svn up' they can be
        // in any order. So, yes, if cache file doesn't correspond to
        // model we can SIGSEGV.
        calCoreModel =
            loadCoreModel( cfgFileName, scale, true/*ignoreMeshes*/ );
        loadMeshes( meshesCacheFileName( cfgFileName ),
                    calCoreModel, meshesData );
    }

    // -- Preparing meshes and materials for fast Model creation --
    for ( MeshesVector::iterator
              meshData = meshesData.begin(),
              meshDataEnd = meshesData.end();
          meshData != meshDataEnd; ++meshData )
    {
        MeshData* md = (*meshData).get();
        CoreMesh* m = new CoreMesh( this,
                                    md,
                                    new Material( md->coreMaterial, dir ),
                                    ps->getParameters( md ) );
        // TODO: add per-core model coreMaterialCache

        meshes.push_back( m );

        osg::notify( osg::INFO )
            << "mesh              : " << m->data->name << std::endl
            << "maxBonesInfluence : " << m->data->maxBonesInfluence << std::endl
            << "trianglesCount    : " << (m->data->getIndicesCount() / 3)
                << std::endl
            << "vertexCount       : " << m->data->vertexBuffer->size()
                << std::endl
            << "rigid             : " << m->data->rigid << std::endl
            << "rigidBoneId       : " << m->data->rigidBoneId << std::endl
            << *m->material << std::endl;
    }

    // -- Collecting animation names --
    for ( int i = 0; i < calCoreModel->getCoreAnimationCount(); i++ )
    {
        animationNames.push_back(
            calCoreModel->getCoreAnimation( i )->getName() );
        animationDurations.push_back(
            calCoreModel->getCoreAnimation( i )->getDuration() );
    }
}

bool
CoreModel::loadNoThrow( const std::string& cfgFileName,
                        std::string&       errorText,
                        MeshParametersSelector* ps )
{
    try
    {
        load( cfgFileName, ps );
        return true;
    }
    catch ( std::runtime_error& e )
    {
        errorText = e.what();
        return false;
    }
}

void
CoreModel::releaseGLObjects( osg::State* state ) const
{
    // DL, StateSets, Shaders?
    for ( MeshVector::const_iterator
              coreMesh = meshes.begin(),
              coreMeshEnd = meshes.end();
          coreMesh != coreMeshEnd; ++coreMesh )
    {
        (*coreMesh)->releaseGLObjects( state );
        // removes mesh display lists & state sets
    }

    ShadersCache::instance()->releaseGLObjects( state );
    // ^ generally not needed since shaders are included in mesh state sets
}


// -- CoreModel loading --

struct FileCloser
{
        FILE* f;
        FileCloser( FILE* f )
            : f( f )
        {}
        ~FileCloser()
        {
            if ( f )
            {
                fclose( f );
            }
        }
};

CalCoreModel*
osgCal::loadCoreModel( const std::string& cfgFileName,
                       float& scale,
                       bool ignoreMeshes )
{
    // -- Initial loading of model --
    scale = 1.0f;
    bool bScale = false;

    FILE* f = fopen( cfgFileName.c_str(), "r" );
    if( !f )
    {
        throw std::runtime_error( "Can't open " + cfgFileName );
    }
    FileCloser closer( f );

    std::auto_ptr< CalCoreModel > calCoreModel( new CalCoreModel( "dummy" ) );

    // Extract path from fileName
    std::string dir = osgDB::getFilePath( cfgFileName );

    static const int LINE_BUFFER_SIZE = 4096;
    char buffer[LINE_BUFFER_SIZE];

    while ( fgets( buffer, LINE_BUFFER_SIZE,f ) )
    {
        // Ignore comments or empty lines
        if ( *buffer == '#' || *buffer == 0 )
            continue;

        char* equal = strchr( buffer, '=' );
        if ( equal )
        {
            // Terminates first token
            *equal++ = 0;
            // Removes ending newline ( CR & LF )
            {
                int last = strlen( equal ) - 1;
                if ( equal[last] == '\n' ) equal[last] = 0;
                if ( last > 0 && equal[last-1] == '\r' ) equal[last-1] = 0;
            }

            // extract file name. all animations, meshes and materials names
            // are taken from file name without extension
            std::string nameToLoad;
            char* point = strrchr( equal, '.' );
            if ( point )
            {
                nameToLoad = std::string( equal, point );
            }
            else
            {
                nameToLoad = equal;
            }

            std::string fullpath = dir + "/" + std::string( equal );

            // process .cfg parameters
            if ( !strcmp( buffer, "scale" ) )
            {
                bScale	= true;
                std::istringstream equal_stream(equal);
                equal_stream.imbue(std::locale::classic());
                equal_stream >> scale;
                continue;
            }

            if ( !strcmp( buffer, "skeleton" ) )
            {
                if( !calCoreModel->loadCoreSkeleton( fullpath ) )
                {
                    throw std::runtime_error(
                        "Can't load skeleton: "
                        + CalError::getLastErrorDescription() );
                }
            }
            else if ( !strcmp( buffer, "animation" ) )
            {
                int animationId = calCoreModel->loadCoreAnimation( fullpath );
                if( animationId < 0 )
                {
                    throw std::runtime_error(
                         "Can't load animation " + nameToLoad + ": "
                         + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreAnimation(animationId)
                    ->setName( nameToLoad );
            }
            else if ( !strcmp( buffer, "mesh" ) )
            {
                if ( ignoreMeshes )
                {
                     // we don't need meshes since VBO data is already loaded
                     // from cache
                    continue;
                }

                int meshId = calCoreModel->loadCoreMesh( fullpath );
                if( meshId < 0 )
                {
                    throw std::runtime_error(
                        "Can't load mesh " + nameToLoad + ": "
                        + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreMesh( meshId )->setName( nameToLoad );

                // -- Remove zero influence vertices --
                // warning: this is a temporary workaround and subject to
                // remove! (this actually must be fixed in blender exporter)
                CalCoreMesh* cm = calCoreModel->getCoreMesh( meshId );

                for ( int i = 0; i < cm->getCoreSubmeshCount(); i++ )
                {
                    CalCoreSubmesh* sm = cm->getCoreSubmesh( i );

                    std::vector< CalCoreSubmesh::Vertex >& v =
                        sm->getVectorVertex();

                    for ( size_t j = 0; j < v.size(); j++ )
                    {

                        std::vector< CalCoreSubmesh::Influence >& infl =
                             v[j].vectorInfluence;

                        std::vector< CalCoreSubmesh::Influence >::iterator it =
                            infl.begin();
                        for ( ;it != infl.end(); )
                        {
                            if ( it->weight <= 0.0001 ) it = infl.erase( it );
                            else ++it;
                        }

                        std::sort( infl.begin(), infl.end(),
                          DataCmp<CalCoreSubmesh::Influence,float>
                            (FIELD_OFFSET(CalCoreSubmesh::Influence,weight)) );
                    }
                }
            }
            else if ( !strcmp( buffer, "material" ) )
            {
                int materialId = calCoreModel->loadCoreMaterial( fullpath );

                if( materialId < 0 )
                {
                    throw std::runtime_error(
                        "Can't load material " + nameToLoad + ": "
                        + CalError::getLastErrorDescription() );
                }
                else
                {
                    calCoreModel->createCoreMaterialThread( materialId );
                    calCoreModel->setCoreMaterialId(
                        materialId, 0, materialId );

                    CalCoreMaterial* material =
                        calCoreModel->getCoreMaterial( materialId );
                    material->setName( nameToLoad );
                }
            }
        }
    }

    // scaling must be done after everything has been created
    if( bScale )
    {
        calCoreModel->scale( scale );
    }

    return calCoreModel.release();
}
