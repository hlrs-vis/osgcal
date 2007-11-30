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
#include <sys/stat.h>

#include <osg/Notify>
#include <osg/Drawable>
#include <osgDB/FileNameUtils>

#include <osgCal/CoreModel>

using namespace osgCal;

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

bool
isFileExists( const std::string& f )
{
    struct stat st;

    return ( stat( f.c_str(), &st ) == 0 );
}

void
CoreModel::load( const std::string& cfgFileNameOriginal,
                 const MeshDisplaySettings* _ds ) throw (std::runtime_error)
{
    std::ref_ptr< MeshDisplaySettings > ds( _ds ? _ds : new MeshDisplaySettings );
    if ( calCoreModel )
    {
        // reloading is not supported
        throw std::runtime_error( "model already loaded" );
    }

//    _flags = SHOW_TBN;//USE_GL_FRONT_FACING | NO_SOFTWARE_MESHES | USE_DEPTH_FIRST_MESHES
//         | DONT_CALCULATE_VERTEX_IN_SHADER;
    
//     flags = _flags;
//     stateSetCache->hwMeshStateSetCache->flags = _flags;
//     stateSetCache->depthMeshStateSetCache->flags = _flags;

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
        calCoreModel = loadCoreModel( cfgFileName, scale, true/*ignoreMeshes*/ );
        loadMeshes( meshesCacheFileName( cfgFileName ),
                    calCoreModel, meshesData );
    }    

    // -- Preparing meshes and materials for fast Model creation --
    for ( MeshesVector::iterator
              meshData = meshesData.begin(),
              meshDataEnd = meshesData.end();
          meshData != meshDataEnd; ++meshData )
    {
        Mesh* m = new Mesh( this,
                            (*meshData).get(),
                            new Material( (*meshData)->coreMaterial, dir ),
                            new MeshDisplaySettings
            );
        // TODO: add per-core model coreMaterialCache

        meshes.push_back( m );

        osg::notify( osg::INFO )
            << "mesh              : " << m->data->name << std::endl
            << "maxBonesInfluence : " << m->data->maxBonesInfluence << std::endl
            << "trianglesCount    : " << m->data->getIndicesCount() / 3 << std::endl
            << "vertexCount       : " << m->data->vertexBuffer->size() << std::endl
            << "rigid             : " << m->data->rigid << std::endl
            << "rigidBoneId       : " << m->data->rigidBoneId << std::endl //<< std::endl
//            << "-- material: " << m->data->coreMaterial->getName() << " --" << std::endl
            << *m->material << std::endl;
    }

    // -- Collecting animation names --
    for ( int i = 0; i < calCoreModel->getCoreAnimationCount(); i++ )
    {
        animationNames.push_back( calCoreModel->getCoreAnimation( i )->getName() );
    }
}

bool
CoreModel::loadNoThrow( const std::string& cfgFileName,
                        std::string&       errorText,
                        int                flags ) throw ()
{
    try
    {
        load( cfgFileName, flags );
        return true;
    }
    catch ( std::runtime_error& e )
    {
        errorText = e.what();
        return false;
    }
}


CoreModel::MeshDisplaySettings::MeshDisplaySettings()
    : software( false )
    , showTBN( false )
    , fogMode( (osg::Fog::Mode)0 )
    , useDepthFirstMesh( false )
{
}

CoreModel::MeshStateSets::MeshStateSets( StateSetCache*  c,
                                         const MeshData* d,
                                         const Material* m,
                                         const MeshDisplaySettings* ds )
{
    Material* ncm = const_cast< Material* >( m ); // material const, refCount isn't

    if ( ds->software )
    {
        stateSet = c->swMeshStateSetCache->get( static_cast< SoftwareMaterial* >( ncm ) );
    }
    else
    {
        staticStateSet = c->hwMeshStateSetCache->get( ncm, 0, ds->useDepthFirstMesh );
        if ( d->rigid == false )
        {
            stateSet = c->hwMeshStateSetCache->get( ncm, d->maxBonesInfluence,
                                                    ds->useDepthFirstMesh );
        }

        if ( ds->useDepthFirstMesh )
        {
            staticDepthOnly = c->depthMeshStateSetCache->get( ncm, 0 );

            if ( d->rigid == false )
            {
                depthOnly = c->depthMeshStateSetCache->get( ncm, d->maxBonesInfluence );
            }
        }
    }
}

CoreModel::MeshDisplayLists::~MeshDisplayLists()
{
    OpenThreads::ScopedLock< OpenThreads::Mutex > lock( mutex ); 

    for( size_t i = 0; i < lists.size(); i++ )
    {
        if ( lists[i] != 0 )
        {
            osg::Drawable::deleteDisplayList( i, lists[i], 0/*getGLObjectSizeHint()*/ );
            lists[i] = 0;
        }
    }            
}

void
CoreModel::MeshDisplayLists::checkAllDisplayListsCompiled( MeshData* data ) const
{
    size_t numOfContexts = osg::DisplaySettings::instance()->getMaxNumberOfGraphicsContexts();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock( mutex ); 
    
    if ( lists.size() != numOfContexts )
    {
        return;
    }
    
    for ( size_t i = 0; i < numOfContexts; i++ )
    {
        if ( lists[ i ] == 0 )
        {
            return;
        }
    }

    // -- Free buffers that are no more needed --
    data->normalBuffer = 0;
    data->texCoordBuffer = 0;
    data->tangentAndHandednessBuffer = 0;
}

CoreModel::Mesh::Mesh( const CoreModel* model,
                       MeshData*        _data,
                       const Material*  _material,
                       const MeshDisplaySettings* _ds )
    : data( _data )
    , material( const_cast< Material* >( _material ) )
    , displaySettings( const_cast< MeshDisplaySettings* >( _ds ) )
    , displayLists( new MeshDisplayLists )
    , stateSets( new MeshStateSets( model->getStateSetCache(),
                                    _data,
                                    _material,
                                    _ds ) )
{
}

CoreModel::Mesh::Mesh( const CoreModel* model,
                       const Mesh* mesh,
                       const Material*  newMaterial,
                       const MeshDisplaySettings* newDs )
    : data( mesh->data.get() )
    , material( const_cast< Material* >( newMaterial ) )
    , displaySettings( const_cast< MeshDisplaySettings* >( newDs ) )
    , displayLists( mesh->displayLists.get() )
    , stateSets( new MeshStateSets( model->getStateSetCache(),
                                    mesh->data.get(),
                                    newMaterial,
                                    newDs ) )
{
}


// -- CoreModel loading --

CalCoreModel*
osgCal::loadCoreModel( const std::string& cfgFileName,
                       float& scale,
                       bool ignoreMeshes )
    throw (std::runtime_error)
{
    // -- Initial loading of model --
    scale = 1.0f;
    bool bScale = false;

    FILE* f = fopen( cfgFileName.c_str(), "r" );
    if( !f )
    {
        throw std::runtime_error( "Can't open " + cfgFileName );
    }

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
                scale	= atof( equal );
                continue;
            }

            if ( !strcmp( buffer, "skeleton" ) ) 
            {
                if( !calCoreModel->loadCoreSkeleton( fullpath ) )
                {
                    throw std::runtime_error( "Can't load skeleton: "
                                             + CalError::getLastErrorDescription() );
                }                
            } 
            else if ( !strcmp( buffer, "animation" ) )
            {
                int animationId = calCoreModel->loadCoreAnimation( fullpath );
                if( animationId < 0 )
                {
                    throw std::runtime_error( "Can't load animation " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreAnimation(animationId)->setName( nameToLoad );
            }
            else if ( !strcmp( buffer, "mesh" ) )
            {
                if ( ignoreMeshes )
                {
                    continue; // we don't need meshes since VBO data is already loaded from cache
                }

                int meshId = calCoreModel->loadCoreMesh( fullpath );
                if( meshId < 0 )
                {
                    throw std::runtime_error( "Can't load mesh " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreMesh( meshId )->setName( nameToLoad );

                // -- Remove zero influence vertices --
                // warning: this is a temporary workaround and subject to remove!
                // (this actually must be fixed in blender exporter)
                CalCoreMesh* cm = calCoreModel->getCoreMesh( meshId );

                for ( int i = 0; i < cm->getCoreSubmeshCount(); i++ )
                {
                    CalCoreSubmesh* sm = cm->getCoreSubmesh( i );

                    std::vector< CalCoreSubmesh::Vertex >& v = sm->getVectorVertex();

                    for ( size_t j = 0; j < v.size(); j++ )
                    {
                        std::vector< CalCoreSubmesh::Influence >& infl = v[j].vectorInfluence;

                        for ( size_t ii = 0; ii < infl.size(); ii++ )
                        {
                            if ( infl[ii].weight == 0 )
                            {
                                infl.erase( infl.begin() + ii );
                                --ii;
                            }
                        }
                    }
                }
            }
            else if ( !strcmp( buffer, "material" ) )  
            {
                int materialId = calCoreModel->loadCoreMaterial( fullpath );

                if( materialId < 0 ) 
                {
                    throw std::runtime_error( "Can't load material " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                } 
                else 
                {
                    calCoreModel->createCoreMaterialThread( materialId ); 
                    calCoreModel->setCoreMaterialId( materialId, 0, materialId );

                    CalCoreMaterial* material = calCoreModel->getCoreMaterial( materialId );
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
    fclose( f );

    return calCoreModel.release();
}
