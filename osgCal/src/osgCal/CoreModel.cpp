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
#include <osg/io_utils>

#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <cal3d/coresubmesh.h>
#include <cal3d/coremesh.h>

#include <osgCal/CoreModel>


// TODO: it seems that Robert also added this in latest OSG
class VertexBufferObject : public osg::BufferObject
{
    public:

        virtual osg::Object* cloneType() const
        {
            throw std::runtime_error( "VertexBufferObject::cloneType unsupported");
        }
        virtual osg::Object* clone(const osg::CopyOp&) const
        {
            throw std::runtime_error( "VertexBufferObject::clone unsupported");
        }
       
        VertexBufferObject( GLenum      target,
                            osg::Array* a )
            : BufferObject()
            , array( a )
        {
            _target = target;
            _usage = GL_STATIC_DRAW_ARB;
            _totalSize = a->getTotalDataSize();
            //_target = GL_ARRAY_BUFFER_ARB;//GL_ELEMENT_ARRAY_BUFFER_ARB
        }

        virtual bool needsCompile( unsigned int contextID ) const
        {
            return ( buffer( contextID ) == 0 );
        }

        virtual void compileBuffer(osg::State& state) const
        {
            unsigned int contextID = state.getContextID();
            if (!needsCompile(contextID)) return;
    
            Extensions* extensions = getExtensions( contextID, true );

            GLuint& vbo = buffer( contextID );

            extensions->glGenBuffers(1, &vbo);
            extensions->glBindBuffer(_target, vbo);
            extensions->glBufferData(_target, _totalSize, array->getDataPointer(),
                                     _usage);
        }

    private:

        osg::ref_ptr< osg::Array > array;
};



// -- Shader cache --

#define OSGCAL_MAX_BONES_PER_MESH       30
#define OSGCAL_MAX_VERTEX_PER_MODEL     1000000

using namespace osgCal;

#define SHADER_FLAG_BONES(_nbones)      ((_nbones) * 128)
#define SHADER_FLAG_BUMP_MAPPING        64
#define SHADER_FLAG_FOG                 32
#define SHADER_FLAG_RGBA                16 // enable blending of RGBA textures
#define SHADER_FLAG_OPACITY             8
#define SHADER_FLAG_TEXTURING           4
#define SHADER_FLAG_NORMAL_MAPPING      2
#define SHADER_FLAG_SHINING             1

/**
 * Set of shaders with specific flags.
 */
class SkeletalShadersSet : public osg::Referenced
{
    public:

        ~SkeletalShadersSet()
        {
            osg::notify( osg::DEBUG_FP ) << "destroying SkeletalShadersSet... " << std::endl;
            vertexShaders.clear();
            fragmentShaders.clear();
            programs.clear();
            osg::notify( osg::DEBUG_FP ) << "SkeletalShadersSet destroyed" << std::endl;
        }

        osg::Program* get( int flags )
        {
            ProgramsMap::const_iterator pmi = programs.find( flags );

            if ( pmi != programs.end() )
            {
                return pmi->second.get();
            }
            else
            {
#define PARSE_FLAGS                                                     \
                int BONES_COUNT = flags / SHADER_FLAG_BONES(1);         \
                int FOG = ( SHADER_FLAG_FOG & flags ) ? 1 : 0;          \
                int RGBA = ( SHADER_FLAG_RGBA & flags ) ? 1 : 0;        \
                int OPACITY = ( SHADER_FLAG_OPACITY & flags ) ? 1 : 0;  \
                int TEXTURING = ( SHADER_FLAG_TEXTURING & flags ) ? 1 : 0; \
                int NORMAL_MAPPING = ( SHADER_FLAG_NORMAL_MAPPING & flags ) ? 1 : 0; \
                int BUMP_MAPPING = ( SHADER_FLAG_BUMP_MAPPING & flags ) ? 1 : 0; \
                int SHINING = ( SHADER_FLAG_SHINING & flags ) ? 1 : 0;

                PARSE_FLAGS;
                
                osg::Program* p = new osg::Program;

                char name[ 256 ];
                sprintf( name, "skeletal shader (%d bones%s%s%s%s%s%s%s)",
                         BONES_COUNT,
                         FOG ? ", fog" : "",
                         RGBA ? ", rgba" : "",
                         OPACITY ? ", opacity" : "",
                         TEXTURING ? ", texturing" : "",
                         NORMAL_MAPPING ? ", normal mapping" : "",
                         BUMP_MAPPING ? ", bump mapping" : "",
                         SHINING ? ", shining" : "" );
                            
                p->setName( name );

                p->addShader( getVertexShader( flags ) );
                p->addShader( getFragmentShader( flags ) );

                //p->addBindAttribLocation( "position", 0 );
                // Attribute location binding is needed for ATI.
                // ATI will draw nothing until one of the attributes
                // will bound to zero location (BTW, this behaviour
                // described in OpenGL spec. don't know why on nVidia
                // it works w/o binding).

                programs[ flags ] = p;
                return p;
            }            
        }
        
    private:

        osg::Shader* getVertexShader( int flags )
        {           
            flags &= ~SHADER_FLAG_RGBA & ~SHADER_FLAG_OPACITY;
                   // remove irrelevant flags that can lead to
                   // duplicate shaders in map
//             if ( flags &
//                  (SHADER_FLAG_BONES(1) | SHADER_FLAG_BONES(2)
//                   | SHADER_FLAG_BONES(3) | SHADER_FLAG_BONES(4)) )
//             {
//                 flags &= ~SHADER_FLAG_BONES(0)
//                     & ~SHADER_FLAG_BONES(1) & ~SHADER_FLAG_BONES(2)
//                     & ~SHADER_FLAG_BONES(3) & ~SHADER_FLAG_BONES(4);
//                 flags |= SHADER_FLAG_BONES(4);
//             }
//           BTW, not so much difference between always 4 bone and per-bones count shaders

            ShadersMap::const_iterator smi = vertexShaders.find( flags );

            if ( smi != vertexShaders.end() )
            {
                return smi->second.get();
            }
            else
            {                
                PARSE_FLAGS;
                (void)RGBA, (void)OPACITY; // remove unused variable warning

                std::string shaderText;

                #include "shaders/SkeletalVertexShader.h"

                osg::Shader* vs = new osg::Shader( osg::Shader::VERTEX,
                                                   shaderText.data() );
                vertexShaders[ flags ] = vs;
                return vs;
            }
        }

        osg::Shader* getFragmentShader( int flags )
        {
            flags &= ~SHADER_FLAG_BONES(0)
                   & ~SHADER_FLAG_BONES(1) & ~SHADER_FLAG_BONES(2)
                   & ~SHADER_FLAG_BONES(3) & ~SHADER_FLAG_BONES(4);
                   // remove irrelevant flags that can lead to
                   // duplicate shaders in map  

            ShadersMap::const_iterator smi = fragmentShaders.find( flags );

            if ( smi != fragmentShaders.end() )
            {
                return smi->second.get();
            }
            else
            {                
                PARSE_FLAGS;
                (void)BONES_COUNT; // remove unused variable warning

                std::string shaderText;

                #include "shaders/SkeletalFragmentShader.h"

                osg::Shader* fs = new osg::Shader( osg::Shader::FRAGMENT,
                                                   shaderText.data() );
                fragmentShaders[ flags ] = fs;
                return fs;
            }
        }

        typedef std::map< int, osg::ref_ptr< osg::Program > > ProgramsMap;
        ProgramsMap programs;

        typedef std::map< int, osg::ref_ptr< osg::Shader > > ShadersMap;
        ShadersMap vertexShaders;
        ShadersMap fragmentShaders;
};


/**
 * Global instance of skeletalVertexProgram.
 * There is only one shader instance which is created with first CoreModel
 * and destroyed after last CoreModel destroyed ( and hence all Models destroyed also
 * since they are referred to CoreModel ).
 */
static osg::ref_ptr< SkeletalShadersSet > skeletalShadersSet;


CoreModel::CoreModel()
    : calCoreModel( 0 )
    , calHardwareModel( 0 )
{
//     std::cout << "CoreModel::CoreModel()" << std::endl;
    texturesCache = new TexturesCache();
    swMeshStateSetCache = new SwMeshStateSetCache( new MaterialsCache(),
                                                   texturesCache );
    hwMeshStateSetCache = new HwMeshStateSetCache( swMeshStateSetCache,
                                                   texturesCache );

    // add initial reference since we are not keeping caches in osg::ref_ptr 
    // and unref them manually (before destroying skeletal shader to which they are refer)
    texturesCache->ref();
    swMeshStateSetCache->ref();
    hwMeshStateSetCache->ref();

    // create or add ref skeletal vertex shader
    if ( !skeletalShadersSet )
    {
        skeletalShadersSet = new SkeletalShadersSet();
    }
    else
    {
        skeletalShadersSet->ref();
    }
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
    delete calHardwareModel;
    delete calCoreModel;

    hwMeshStateSetCache->unref();
    swMeshStateSetCache->unref();
    texturesCache->unref();

    // unref skeletal vertex program and delete when no references left
    SkeletalShadersSet* sss = skeletalShadersSet.release();
    if ( sss->referenceCount() <= 0 )
    {
        sss->unref();
    }
    else
    {
        skeletalShadersSet = sss;
    }
//     std::cout << "CoreModel::~CoreModel()" << std::endl;
}

std::ostream&
operator << ( std::ostream& os,
              osgCal::HwStateDesc sd )
{
    os << "ambientColor     : " << sd.material.ambientColor << std::endl
       << "diffuseColor     : " << sd.material.diffuseColor << std::endl
       << "specularColor    : " << sd.material.specularColor << std::endl
       << "glossiness       : " << sd.material.glossiness << std::endl
       << "duffuseMap       : " << sd.diffuseMap << std::endl
       << "normalsMap       : " << sd.normalsMap << std::endl
//       << "normalsMapAmount : " << sd.normalsMapAmount << std::endl
       << "bumpMap          : " << sd.bumpMap << std::endl
       << "bumpMapAmount    : " << sd.bumpMapAmount << std::endl
       << "sides            : " << sd.sides << std::endl;
    return os;
}

void
CoreModel::load( const std::string& cfgFileNameOriginal ) throw (std::runtime_error)
{
    if ( calCoreModel )
    {
        // reloading is not supported
        throw std::runtime_error( "model already loaded" );
    }

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

    std::vector< std::string > meshNames;
    std::auto_ptr< VBOs > bos;

    if ( isFileExists( HWModelCacheFileName( cfgFileName ) ) == false
         || isFileExists( VBOsCacheFileName( cfgFileName ) ) == false )
    {
        // -- Load model and hw model from model --
        calCoreModel = loadCoreModel( cfgFileName, scale );

        calHardwareModel = new CalHardwareModel(calCoreModel);
        bos = std::auto_ptr< VBOs >( loadVBOs( calHardwareModel ) );
        //saveVBOs( bos, dir + "/vbos.cache" );
        //saveHardwareModel( calHardwareModel, dir + "/hwmodel.cache" );
        // ^ it's not loading task, cache preparation is export task
        // see `applications/preparer' for this.

        // -- Fill meshNames array --
        for(int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount();
            hardwareMeshId++)
        {
            meshNames.push_back(
                calCoreModel->
                getCoreMesh( calHardwareModel->getVectorHardwareMesh()[ hardwareMeshId ].meshId )->
                getName() );
        }
    }
    else
    {
        // -- Load cached hardware model --
        if ( Cal::LIBRARY_VERSION != 1000 && Cal::LIBRARY_VERSION != 1100 && Cal::LIBRARY_VERSION != 1200 )
        {
            throw std::runtime_error( "caching was only tested on cal3d 0.10.0, 0.11.0 and 0.12.0" );
        }
        
        calCoreModel = loadCoreModel( cfgFileName, scale, true/*ignoreMeshes*/ );

//         if ( isFileOlder( cfgFileName, VBOsCacheFileName( cfgFileName ) ) )
//         {
        // We don't check file dates, since after `svn up' they can be
        // in any order. So, yes, if cache file doesn't correspond to
        // model we can SIGSEGV.
        bos = std::auto_ptr< VBOs >( loadVBOs( VBOsCacheFileName( cfgFileName ) ) );
        //std::cout << "loaded from cache" << std::endl;
        //std::cout << "vertexCount = " << bos->vertexCount << std::endl;
        //std::cout << "faceCount = "   << bos->faceCount << std::endl;
        calHardwareModel = loadHardwareModel( calCoreModel,
                                              HWModelCacheFileName( cfgFileName ),
                                              meshNames );
//         }
//         else
//         {
//             throw std::runtime_error( "cache is older than .cfg" );
//         }
    }

    // -- Preparing meshes and materials for fast Model creation --
    for(int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount();
        hardwareMeshId++)
    {
        // -- Setup some fields --
        Mesh m;

        m.hardwareMeshId = hardwareMeshId;
        m.hardwareMesh   = &calHardwareModel->getVectorHardwareMesh()[ hardwareMeshId ];
        m.name           = meshNames[ hardwareMeshId ];

        if ( m.hardwareMesh->faceCount == 0 )
        {
            continue; // rare case, first reported by Ovidiu Sabou
                      // it not caused any bug on my machine,
                      // but Ovidiu had osgCalViewer crash
        }

        // -- Calculate maxBonesInfluence & rigidness --
        GLfloat* vertexBuffer = (GLfloat*) bos->vertexBuffer->getDataPointer();
        MatrixIndexBuffer::value_type::value_type*
            matrixIndexBuffer = (MatrixIndexBuffer::value_type::value_type*)
            bos->matrixIndexBuffer->getDataPointer();
        GLfloat* weightBuffer = (GLfloat*) bos->weightBuffer->getDataPointer();
        GLuint*  indexBuffer = (GLuint*) bos->indexBuffer->getDataPointer();

        GLuint* begin = &indexBuffer[ m.getIndexInVbo() ];
        GLuint* end   = begin + m.getIndexesCount();

        m.maxBonesInfluence = 0;
        m.rigid = true;
        
        for ( ; begin < end; ++begin )
        {
            float* v = &vertexBuffer[ *begin * 3 ];
            m.boundingBox.expandBy( v[0], v[1], v[2] );

            if ( !( matrixIndexBuffer[ *begin * 4 ] == 0
                    &&
                    weightBuffer[ *begin * 4 ] == 1.0f ) )
            {
                m.rigid = false;
            }

            if ( m.maxBonesInfluence < 1 && weightBuffer[ *begin * 4 + 0 ] > 0.0 )
                m.maxBonesInfluence = 1;
            if ( m.maxBonesInfluence < 2 && weightBuffer[ *begin * 4 + 1 ] > 0.0 )
                m.maxBonesInfluence = 2;
            if ( m.maxBonesInfluence < 3 && weightBuffer[ *begin * 4 + 2 ] > 0.0 )
                m.maxBonesInfluence = 3;
            if ( m.maxBonesInfluence < 4 && weightBuffer[ *begin * 4 + 3 ] > 0.0 )
                m.maxBonesInfluence = 4;

//             std::cout << matrixIndexBuffer[ *begin * 4 + 0 ] << '\t'
//                       << matrixIndexBuffer[ *begin * 4 + 1 ] << '\t'
//                       << matrixIndexBuffer[ *begin * 4 + 2 ] << '\t'
//                       << matrixIndexBuffer[ *begin * 4 + 3 ] << "\t\t"
//                       << weightBuffer[ *begin * 4 + 0 ] << '\t'
//                       << weightBuffer[ *begin * 4 + 1 ] << '\t'
//                       << weightBuffer[ *begin * 4 + 2 ] << '\t'
//                       << weightBuffer[ *begin * 4 + 3 ] << std::endl;
        }

        if ( m.rigid )
        {
            if ( m.getBonesCount() != 1 )
            {
                throw std::runtime_error( "must be one bone in this mesh" );
            }

            m.rigidBoneId = m.hardwareMesh->m_vectorBonesIndices[ 0 ];
        }

        if ( m.maxBonesInfluence == 0 ) // unrigged mesh
        {
            m.rigid = true; // mesh is rigid when all its vertices are
                            // rigged to one bone with weight = 1.0,
                            // or when no one vertex is rigged at all
            m.rigidBoneId = -1; // no bone
        }

        // -- Setup state sets --
        m.hwStateDesc = HwStateDesc( m.hardwareMesh->pCoreMaterial, dir );
//         calCoreModel->getCoreMaterial( m.coreSubMesh->getCoreMaterialThreadId() );
//         coreMaterialId == coreMaterialThreadId - we made them equal on load phase
        
        m.stateSet = swMeshStateSetCache->get( static_cast< SwStateDesc >( m.hwStateDesc ) );

        m.staticHardwareStateSet = hwMeshStateSetCache->get( m.hwStateDesc );

        m.hwStateDesc.shaderFlags |= SHADER_FLAG_BONES( m.maxBonesInfluence );
        m.hardwareStateSet = hwMeshStateSetCache->get( m.hwStateDesc );

        // -- Done with mesh --
        meshes.push_back( m );

        osg::notify( osg::INFO )
            << "mesh: " << m.name << std::endl
            << "material: " << m.hardwareMesh->pCoreMaterial->getName() << std::endl
            << m.hwStateDesc << std::endl
            << "  m.maxBonesInfluence       = " << m.maxBonesInfluence << std::endl
            << "  m.hardwareMesh->meshId    = " << m.hardwareMesh->meshId << std::endl
            << "  m.hardwareMesh->submeshId = " << m.hardwareMesh->submeshId << std::endl
            << "  m.indexInVbo              = " << m.getIndexInVbo() << std::endl
            << "  m.indexesCount            = " << m.getIndexesCount() << std::endl
            << "  m.rigid                   = " << m.rigid << std::endl;
    }

    // -- Check zero weight bones --
    {
        MatrixIndexBuffer::iterator mi = bos->matrixIndexBuffer->begin();
        WeightBuffer::iterator      w  = bos->weightBuffer->begin();

        while ( mi < bos->matrixIndexBuffer->end() )
        {
            if ( (*w)[0] <= 0.0 ) // no influences at all
            {
                (*w)[0] = 1.0;
                (*mi)[0] = 30;
                // last+1 bone in shader is always identity matrix.
                // we need this hack for meshes where some vertexes
                // are rigged and some are not (so we create
                // non-movable bone for them) (see #68)
            }
            
            ++mi;
            ++w;
        }
    }

    // -- Collecting animation names --
    for ( int i = 0; i < calCoreModel->getCoreAnimationCount(); i++ )
    {
        animationNames.push_back( calCoreModel->getCoreAnimation( i )->getName() );
    }
    
    // -- Save some buffers --
    vertexBuffer        = bos->vertexBuffer;
    weightBuffer        = bos->weightBuffer;
    matrixIndexBuffer   = bos->matrixIndexBuffer;
    indexBuffer         = bos->indexBuffer;
    normalBuffer        = bos->normalBuffer;
    binormalBuffer      = bos->binormalBuffer;
    tangentBuffer       = bos->tangentBuffer;
    texCoordBuffer      = bos->texCoordBuffer;

    // -- Create vertex buffer objects --
    vbos.resize( 8 );

    vbos[ BI_VERTEX ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                bos->vertexBuffer.get() );
    vbos[ BI_WEIGHT ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                bos->weightBuffer.get() );
    vbos[ BI_NORMAL ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                bos->normalBuffer.get() );
    vbos[ BI_TANGENT ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                 bos->tangentBuffer.get() );
    vbos[ BI_BINORMAL ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                  bos->binormalBuffer.get() );
    vbos[ BI_MATRIX_INDEX ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                      bos->matrixIndexBuffer.get() );
    vbos[ BI_TEX_COORD ] = new VertexBufferObject( GL_ARRAY_BUFFER_ARB,
                                                   bos->texCoordBuffer.get() );
    vbos[ BI_INDEX ] = new VertexBufferObject( GL_ELEMENT_ARRAY_BUFFER_ARB,
                                               bos->indexBuffer.get() );
}

bool
CoreModel::loadNoThrow( const std::string& cfgFileName,
                        std::string&       errorText ) throw ()
{
    try
    {
        load( cfgFileName );
        return true;
    }
    catch ( std::runtime_error& e )
    {
        errorText = e.what();
        return false;
    }
}


// -- MaterialDesc --

osg::Vec4
colorToVec4( const CalCoreMaterial::Color& color )
{
    return osg::Vec4( color.red / 255.f,
                      color.green / 255.f,
                      color.blue / 255.f,
                      color.alpha == 0 ? 1.f : (color.alpha / 255.f) );
}

MaterialDesc::MaterialDesc( CalCoreMaterial* m,
                            float glossiness,
                            float opacity )
    : ambientColor(  colorToVec4( m->getAmbientColor()  ) )
    , diffuseColor(  colorToVec4( m->getDiffuseColor()  ) )
    , specularColor( colorToVec4( m->getSpecularColor() ) * m->getShininess() ) 
    , glossiness( glossiness )
{
    ambientColor.a() = opacity;
    diffuseColor.a() = opacity;
    specularColor.a() = opacity;
}

#define lt( a, b, t ) (a < b ? true : ( b < a ? false : t ))
// // a < b || (!( b < a ) && t )
    
bool
osgCal::operator < ( const MaterialDesc& md1,
                     const MaterialDesc& md2 )
{
    bool r = lt( md1.ambientColor,
                 md2.ambientColor,
                 lt( md1.diffuseColor,
                     md2.diffuseColor,
                     lt( md1.specularColor,
                         md2.specularColor,
                         lt( md1.glossiness,
                             md2.glossiness, false ))));
    return r;
}

// -- SwStateDesc --

bool
osgCal::operator < ( const SwStateDesc& d1,
                     const SwStateDesc& d2 )
{
    bool r = lt( d1.material,
                 d2.material,
                 lt( d1.diffuseMap,
                     d2.diffuseMap,
                     lt( d1.sides,
                         d2.sides, false )));
    return r;
}

// -- HwStateDesc --

bool
osgCal::operator < ( const HwStateDesc& d1,
                     const HwStateDesc& d2 )
{
    bool r = lt( static_cast< SwStateDesc >( d1 ),
                 static_cast< SwStateDesc >( d2 ),
                 lt( d1.normalsMap,
                     d2.normalsMap,
                     lt( d1.bumpMap,
                         d2.bumpMap,
                         lt( d1.normalsMapAmount,
                             d2.normalsMapAmount,
                             lt( d1.bumpMapAmount,
                                 d2.bumpMapAmount,
                                 lt( d1.shaderFlags,
                                     d2.shaderFlags, false ))))));
    return r;
}

#undef lt

float
stringToFloat( const std::string& s )
{
    float r;
    sscanf( s.c_str(), "%f", &r );
    return r;
}

int
stringToInt( const std::string& s )
{
    int r;
    sscanf( s.c_str(), "%d", &r );
    return r;
}

HwStateDesc::HwStateDesc( CalCoreMaterial* m,
                          const std::string& dir )
    : normalsMapAmount( 0 )
    , bumpMapAmount( 0 )
    , shaderFlags( 0 )
{
//     m->getVectorMap().clear();
//     CalCoreMaterial::Color white = { 255, 255, 255, 255 };
//     m->setAmbientColor( white );
//     m->setDiffuseColor( white );
//     m->setSpecularColor( white );
//     m->setShininess( 0 );
    // ^ not much is changed w/o different states, so state changes
    // are not bottleneck
    
    float glossiness = 50;
    float opacity = 1.0; //m->getDiffuseColor().alpha / 255.0;
    // We can't set opacity to diffuse color's alpha because of some
    // test models (cally, paladin, skeleton) have alpha components
    // equal to zero (this is incorrect, but backward compatibility
    // is more important).

    // -- Scan maps (parameters) --
    for ( int i = 0; i < m->getMapCount(); i++ )
    {
        const std::string& map = m->getMapFilename( i );
        std::string prefix = getPrefix( map );
        std::string suffix = getSuffix( map );
        
        if ( prefix == "DiffuseMap:" || prefix == "" )
        {
            diffuseMap = dir + "/" + suffix;
            shaderFlags |= SHADER_FLAG_TEXTURING;
        }
        else if ( prefix == "NormalsMap:" )
        {
            normalsMap = dir + "/" + suffix;
            shaderFlags |= SHADER_FLAG_NORMAL_MAPPING;
        }
        else if ( prefix == "BumpMap:" )
        {
//             bumpMap = dir + "/" + suffix;
//             shaderFlags |= SHADER_FLAG_BUMP_MAPPING;
        }
        else if ( prefix == "BumpMapAmount:" )
        {
            bumpMapAmount = stringToFloat( suffix );
        }
        else if ( prefix == "Opacity:" )
        {
            opacity = stringToFloat( suffix );
        }
        else if ( prefix == "Glossiness:" )
        {
            glossiness = stringToFloat( suffix );
        }
        else if ( prefix == "Sides:" )
        {
            sides = stringToInt( suffix );
        }
        else if ( prefix == "SelfIllumination:" )
        {
            ;
        }
        else if ( prefix == "Shading:" )
        {
            ;
        }
    }

    // -- Setup material --
    material = MaterialDesc( m, glossiness, opacity );

    if ( diffuseMap != "" ) // set diffuse color to white when texturing
    {
        material.diffuseColor.r() = 1.0;
        material.diffuseColor.g() = 1.0;
        material.diffuseColor.b() = 1.0;
    }

    // -- Setup additional shader flags --
    if (    material.specularColor.r() != 0
         || material.specularColor.g() != 0
         || material.specularColor.b() != 0 )
    {
        shaderFlags |= SHADER_FLAG_SHINING;
    }

    if ( material.diffuseColor.a() < 1 )
    {
        shaderFlags |= SHADER_FLAG_OPACITY;
    }
}


// -- Caches --

template < typename Key, typename Value, typename Class >
Value*
getOrCreate( std::map< Key, osg::ref_ptr< Value > >& map,
             const Key&                              key,
             Class*                                  obj,
             Value*                        ( Class::*create )( const Key& ) )
{
    typename std::map< Key, osg::ref_ptr< Value > >::const_iterator i = map.find( key );
    if ( i != map.end() )
    {
        return i->second.get();
    }
    else
    {
        Value* v = (obj ->* create)( key ); // damn c++!
        map[ key ] = v;
        return v;
    }
}
                 

// -- Materials cache --

osg::Material*
MaterialsCache::get( const MaterialDesc& md )
{
    return getOrCreate( cache, md, this, &MaterialsCache::createMaterial );
}

osg::Material*
MaterialsCache::createMaterial( const MaterialDesc& desc )
{
    osg::Material* material = new osg::Material();

    material->setColorMode( osg::Material::AMBIENT_AND_DIFFUSE );
    material->setAmbient( osg::Material::FRONT_AND_BACK, desc.ambientColor );
    material->setDiffuse( osg::Material::FRONT_AND_BACK, desc.diffuseColor );
    material->setSpecular( osg::Material::FRONT_AND_BACK, desc.specularColor );
    material->setShininess( osg::Material::FRONT_AND_BACK,
                            desc.glossiness > 128.0 ? 128.0 : desc.glossiness );
    // TODO: test shininess in sw mode

    return material;
}


// -- Textures cache --

osg::Texture2D*
TexturesCache::get( const TextureDesc& td )
{
    return getOrCreate( cache, td, this, &TexturesCache::createTexture );
}

osg::Texture2D*
TexturesCache::createTexture( const TextureDesc& fileName )
    throw ( std::runtime_error )
{
//    std::cout << "load texture: " << fileName << std::endl;
    osg::Image* img = osgDB::readImageFile( fileName );

    if ( !img )
    {
        throw std::runtime_error( "Can't load " + fileName );
    }

    osg::Texture2D* texture = new osg::Texture2D;

    // these are default settings
//     texture->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
//     texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
//     texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

    texture->setImage( img );
    texture->setUnRefImageDataAfterApply( true );

    return texture;
}


// -- Software state set cache --

/**
 * Is state set contains texture with alpha channel (that needs to be
 * blended)
 */
static
bool
isRGBAStateSet( const osg::StateSet* stateSet )
{
    const osg::Texture2D* texture = static_cast< const osg::Texture2D* >(
        stateSet->getTextureAttribute( 0, osg::StateAttribute::TEXTURE ) );
    
    return ( texture &&
             (    texture->getInternalFormat() == 4
               || texture->getInternalFormat() == GL_RGBA ) );
}

static
bool
isTransparentStateSet( osg::StateSet* stateSet )
{
    return ( stateSet->getRenderingHint() & osg::StateSet::TRANSPARENT_BIN ) != 0;
}

static
void
setupTransparentStateSet( osg::StateSet* stateSet )
{
    if ( isTransparentStateSet( stateSet ) )
    {
        return; // already setup
    }

    stateSet->setRenderingHint( osg::StateSet::TRANSPARENT_BIN );

    // enable blending
    osg::BlendFunc* bf = new osg::BlendFunc;
    bf->setFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    stateSet->setAttributeAndModes( bf,
                                    osg::StateAttribute::ON |
                                    osg::StateAttribute::PROTECTED );
}

SwMeshStateSetCache::SwMeshStateSetCache( MaterialsCache* mc,
                                          TexturesCache* tc )
    : materialsCache( mc ? mc : new MaterialsCache() )
    , texturesCache( tc ? tc : new TexturesCache() )
{}
            
osg::StateSet*
SwMeshStateSetCache::get( const SwStateDesc& swsd )
{
    return getOrCreate( cache, swsd, this,
                        &SwMeshStateSetCache::createSwMeshStateSet );
}
osg::StateSet*
SwMeshStateSetCache::createSwMeshStateSet( const SwStateDesc& desc )
{
    osg::StateSet* stateSet = new osg::StateSet();

    // -- setup material --
    osg::Material* material = materialsCache->get( desc.material );
    stateSet->setAttributeAndModes(material, osg::StateAttribute::ON);    

    // -- setup diffuse map --
    if ( desc.diffuseMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.diffuseMap );

        stateSet->setTextureAttributeAndModes( 0, texture, osg::StateAttribute::ON );
    }

    // -- setup sidedness --
    switch ( desc.sides )
    {
        case 1:
            // one sided mesh -- force backface culling
            stateSet->setAttributeAndModes( new osg::CullFace(),
                                            osg::StateAttribute::ON |
                                            osg::StateAttribute::PROTECTED );
            // PROTECTED, since overriding of this state (for example by
            // pressing 'b') leads to incorrect display
            break;

        case 2:
            // two sided mesh -- no culling (for sw mesh)
            stateSet->setAttributeAndModes( new osg::CullFace(),
                                            osg::StateAttribute::OFF |
                                            osg::StateAttribute::PROTECTED );
            break;

        default:
            // undefined sides count -- use default OSG culling
            break;
    }

    // -- Check transparency modes --
    if ( isRGBAStateSet( stateSet ) || desc.material.diffuseColor.a() < 1 )
    {
        setupTransparentStateSet( stateSet );
        // and force backface culling
        stateSet->setAttributeAndModes( new osg::CullFace(),
                                        osg::StateAttribute::ON |
                                        osg::StateAttribute::PROTECTED );
        // TODO: actually for software meshes we need two pass render
        // not only backface culling. But we can't use two meshes
        // since it is one mesh (maybe add drawImplementation to
        // software mesh?)
    }
    else
    {
        stateSet->setRenderingHint( osg::StateSet::OPAQUE_BIN );
        // ^ this fixes some strange state sorting bug in OSG
        // otherwise we sometimes get materials & lightmodels (and
        // maybe something else) from another statesets.
        // At least we get lightmodel from other stateset for
        // transparent meshes.
    }
    return stateSet;
}


// -- Hardware state set cache --

osg::Uniform*
newFloatUniform( const std::string& name,
                 GLfloat value )
{
    osg::Uniform* u = new osg::Uniform( osg::Uniform::FLOAT, name );
    u->set( value );

    return u;
}

osg::Uniform*
newIntUniform( osg::Uniform::Type type,
               const std::string& name,
               int value )
{
    osg::Uniform* u = new osg::Uniform( type, name );
    u->set( value );

    return u;
}

HwMeshStateSetCache::HwMeshStateSetCache( SwMeshStateSetCache* swssc,
                                          TexturesCache* tc )
    : swMeshStateSetCache( swssc ? swssc : new SwMeshStateSetCache() )
    , texturesCache( tc ? tc : new TexturesCache() )
{}
            
osg::StateSet*
HwMeshStateSetCache::get( const HwStateDesc& swsd )
{
    return getOrCreate( cache, swsd, this,
                        &HwMeshStateSetCache::createHwMeshStateSet );
}

osg::StateSet*
HwMeshStateSetCache::createHwMeshStateSet( const HwStateDesc& desc )
{
    osg::StateSet* baseStateSet = swMeshStateSetCache->
        get( static_cast< SwStateDesc >( desc ) );

    osg::StateSet* stateSet = new osg::StateSet( *baseStateSet );
//    stateSet->merge( *baseStateSet );
    // 50 fps downs to 48 when merge used instead of new StateSet(*base)

    // -- Setup shader --
    int rgba = isRGBAStateSet( stateSet );

    stateSet->setAttributeAndModes( skeletalShadersSet->get(
                                        desc.shaderFlags +
                                        rgba * SHADER_FLAG_RGBA ),
                                    osg::StateAttribute::ON );

    stateSet->addUniform( newFloatUniform( "glossiness", desc.material.glossiness ) );

    // -- setup normals map --
    if ( desc.normalsMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.normalsMap );

        stateSet->setTextureAttributeAndModes( 1, texture, osg::StateAttribute::ON );
        stateSet->addUniform( newIntUniform( osg::Uniform::SAMPLER_2D, "normalMap", 1 ) );
    }

    // -- setup bump map --
    if ( desc.bumpMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.bumpMap );        

        stateSet->setTextureAttributeAndModes( 2, texture, osg::StateAttribute::ON );
        stateSet->addUniform( newIntUniform( osg::Uniform::SAMPLER_2D, "bumpMap", 2 ) );
        stateSet->addUniform( newFloatUniform( "bumpMapAmount", desc.bumpMapAmount ) );
    }

    // -- setup some uniforms --
    if ( desc.diffuseMap != "" )
    {
        stateSet->addUniform( newIntUniform( osg::Uniform::SAMPLER_2D, "decalMap", 0 ) );
    }

    // -- setup sidedness --
    switch ( desc.sides )
    {
        case 1:
            // one sided mesh -- force backface culling
            // do nothing since it already forced for software mesh
            break;

        case 2:
            // two sided mesh -- enable culling
            // (we use two pass render for double sided meshes,
            //  or single pass when gl_FrontFacing used, but in only
            //  works on GeForce >= 6.x and not works on ATI)
            stateSet->setAttributeAndModes( new osg::CullFace(),
                                            osg::StateAttribute::ON |
                                            osg::StateAttribute::PROTECTED );
            break;

        default:
            // undefined sides count -- use default OSG culling
            break;
    }
    
    return stateSet;
}
