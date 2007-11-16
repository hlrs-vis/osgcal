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
#include <memory>

#include <osg/Notify>
#include <osg/Texture2D>
#include <osg/Image>
#include <osg/VertexProgram>
#include <osg/Drawable>
#include <osg/Vec4>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/ColorMask>
#include <osg/DisplaySettings>
#include <osg/io_utils>

#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <cal3d/coresubmesh.h>
#include <cal3d/coremesh.h>

#include <osgCal/CoreModel>

using namespace osgCal;

CoreModel::CoreModel()
    : calCoreModel( 0 )
{
    setThreadSafeRefUnref( true );
//     std::cout << "CoreModel::CoreModel()" << std::endl;
    texturesCache = new TexturesCache();
    swMeshStateSetCache = new SwMeshStateSetCache( new MaterialsCache(),
                                                   texturesCache );
    hwMeshStateSetCache = new HwMeshStateSetCache( swMeshStateSetCache,
                                                   texturesCache );
    depthMeshStateSetCache = new DepthMeshStateSetCache();

    // add initial reference since we are not keeping caches in osg::ref_ptr 
    // and unref them manually (before destroying skeletal shader to which they are refer)
    texturesCache->ref();
    swMeshStateSetCache->ref();
    hwMeshStateSetCache->ref();
    depthMeshStateSetCache->ref();

    // create and ref skeletal vertex shader
    if ( !shadersCache )
    {
        shadersCache = new ShaderCache();
    }

    shadersCache->ref();
}

CoreModel::CoreModel(const CoreModel&, const osg::CopyOp&)
    : Object() // to eliminate warning
{
    throw std::runtime_error( "CoreModel copying is not supported" );
}

CoreModel::~CoreModel()
{
    meshes.clear();
    
    // cleanup of non-auto released resources
    delete calCoreModel;

    depthMeshStateSetCache->unref();
    hwMeshStateSetCache->unref();
    swMeshStateSetCache->unref();
    texturesCache->unref();

    // delete shadersCache when no references left
    if ( shadersCache->referenceCount() == 1 )
    {
        // TODO: there is some error with access to already destroyed shader
        // when exiting multithreaded application with model w/o animations
        shadersCache->unref(); 
        shadersCache = NULL;
    }
//     std::cout << "CoreModel::~CoreModel()" << std::endl;
}

void
CoreModel::load( const std::string& cfgFileNameOriginal,
                 int _flags ) throw (std::runtime_error)
{
    if ( calCoreModel )
    {
        // reloading is not supported
        throw std::runtime_error( "model already loaded" );
    }

//    _flags = SHOW_TBN;//USE_GL_FRONT_FACING | NO_SOFTWARE_MESHES | USE_DEPTH_FIRST_MESHES
//         | DONT_CALCULATE_VERTEX_IN_SHADER;
    
    flags = _flags;
    hwMeshStateSetCache->flags = _flags;
    depthMeshStateSetCache->flags = _flags;

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
        // -- Setup some fields --
        Mesh* m = new Mesh;

        m->data = meshData->get();

        m->setThreadSafeRefUnref( true );
        m->data->setThreadSafeRefUnref( true );
        m->data->indexBuffer->setThreadSafeRefUnref( true );
        m->data->vertexBuffer->setThreadSafeRefUnref( true );
        m->data->normalBuffer->setThreadSafeRefUnref( true );

        // -- Setup state sets --
        m->hwStateDesc = HwStateDesc( m->data->coreMaterial, dir );
//         calCoreModel->getCoreMaterial( m->coreSubMesh->getCoreMaterialThreadId() );
//         coreMaterialId == coreMaterialThreadId - we made them equal on load phase
        
        m->stateSet = swMeshStateSetCache->get( static_cast< SwStateDesc >( m->hwStateDesc ) );

        m->staticHardwareStateSet = hwMeshStateSetCache->get( m->hwStateDesc );
        m->staticDepthStateSet = depthMeshStateSetCache->get( m->hwStateDesc );

        m->hwStateDesc.shaderFlags |= SHADER_FLAG_BONES( m->data->maxBonesInfluence );

        if ( m->data->rigid == false )
        {
            m->hardwareStateSet = hwMeshStateSetCache->get( m->hwStateDesc );
            m->depthStateSet = depthMeshStateSetCache->get( m->hwStateDesc );
        }

        // -- Done with mesh --
        meshes.push_back( m );

        osg::notify( osg::INFO )
            << "mesh              : " << m->data->name << std::endl
            << "maxBonesInfluence : " << m->data->maxBonesInfluence << std::endl
            << "trianglesCount    : " << m->data->getIndicesCount() / 3 << std::endl
            << "vertexCount       : " << m->data->vertexBuffer->size() << std::endl
            << "rigid             : " << m->data->rigid << std::endl
            << "rigidBoneId       : " << m->data->rigidBoneId << std::endl //<< std::endl
//            << "-- material: " << m->data->coreMaterial->getName() << " --" << std::endl
            << m->hwStateDesc << std::endl;
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

CoreModel::MeshDisplayLists::~MeshDisplayLists()
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock( displayListsMutex ); 

    for( size_t i = 0; i < displayLists.size(); i++ )
    {
        if ( displayLists[i] != 0 )
        {
            osg::Drawable::deleteDisplayList( i, displayLists[i], 0/*getGLObjectSizeHint()*/ );
            displayLists[i] = 0;
        }
    }            
}

void
CoreModel::MeshDisplayLists::checkAllDisplayListsCompiled() const
{
    size_t numOfContexts = osg::DisplaySettings::instance()->getMaxNumberOfGraphicsContexts();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock( displayListsMutex ); 
    
    if ( displayLists.size() != numOfContexts )
    {
        return;
    }
    
    for ( size_t i = 0; i < numOfContexts; i++ )
    {
        if ( displayLists[ i ] == 0 )
        {
            return;
        }
    }

    // -- Free buffers that are no more needed --
    data->normalBuffer = 0;
    data->texCoordBuffer = 0;
    data->tangentAndHandednessBuffer = 0;
}
