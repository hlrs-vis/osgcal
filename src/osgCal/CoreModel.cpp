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

#define SHADER_FLAG_BONES(_nbones)      ((_nbones) * 64)
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

        osg::Program* get( int flags )
        {
            ShadersMap::const_iterator shm = shaders.find( flags );

            if ( shm != shaders.end() )
            {
                return shm->second.get();
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
                int SHINING = ( SHADER_FLAG_SHINING & flags ) ? 1 : 0;

                PARSE_FLAGS;
                
                osg::Program* s = new osg::Program;

                char name[ 256 ];
                sprintf( name, "skeletal shader (%d bones%s%s%s%s%s%s)",
                         BONES_COUNT,
                         FOG ? ", fog" : "",
                         RGBA ? ", rgba" : "",
                         OPACITY ? ", opacity" : "",
                         TEXTURING ? ", texturing" : "",
                         NORMAL_MAPPING ? ", normal mapping" : "",
                         SHINING ? ", shining" : "" );
                            
                s->setName( name );

                s->addShader( new osg::Shader( osg::Shader::VERTEX,
                                               getVertexShaderText( flags ).data() ) );
                s->addShader( new osg::Shader( osg::Shader::FRAGMENT,
                                               getFragmentShaderText( flags ).data() ) );
                s->addBindAttribLocation( "position", 0 );

                shaders[ flags ] = s;
                return s;
            }            
        }
        
    private:

        std::string getVertexShaderText( int flags )
        {
            PARSE_FLAGS;
            (void)RGBA, (void)OPACITY; // remove unused variable warning

            std::string shaderText;

            #include "shaders/SkeletalVertexShader.h"

            return shaderText;
        }

        std::string getFragmentShaderText( int flags )
        {
            PARSE_FLAGS;
            (void)BONES_COUNT; // remove unused variable warning

            std::string shaderText;

            #include "shaders/SkeletalFragmentShader.h"

            return shaderText;
        }

        typedef std::map< int, osg::ref_ptr< osg::Program > > ShadersMap;
        ShadersMap shaders;
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
       << "shininess        : " << sd.material.shininess << std::endl
       << "duffuseMap       : " << sd.diffuseMap << std::endl
       << "normalsMap       : " << sd.normalsMap << std::endl
//       << "normalsMapAmount : " << sd.normalsMapAmount << std::endl
//       << "bumpMap          : " << sd.bumpMap << std::endl
//       << "bumpMapAmount    : " << sd.bumpMapAmount << std::endl
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

    calCoreModel = loadCoreModel( cfgFileName, scale );

    // -- Preparing hardware model --
    calHardwareModel = 0;
    VBOs* bos = 0;

    try // try load cached vbos
    {
        if ( Cal::LIBRARY_VERSION != 1000 )
        {
            throw std::runtime_error( "caching was only tested on cal3d 0.10.0" );
        }
//         if ( isFileOlder( cfgFileName, VBOsCacheFileName( cfgFileName ) ) )
//         {
        // We don't check file dates, since after `svn up' they can be
        // in any order. So, yes, if cache file doesn't correspond to
        // model we can SIGSEGV.
        bos = loadVBOs( VBOsCacheFileName( cfgFileName ) );
        //std::cout << "loaded from cache" << std::endl;
        //std::cout << "vertexCount = " << bos->vertexCount << std::endl;
        //std::cout << "faceCount = "   << bos->faceCount << std::endl;
        calHardwareModel = loadHardwareModel( calCoreModel,
                                              HWModelCacheFileName( cfgFileName ) );
//         }
//         else
//         {
//             throw std::runtime_error( "cache is older than .cfg" );
//         }
    }
    catch ( std::runtime_error& e )
    {
        delete bos;
        delete calHardwareModel;
        calHardwareModel = new CalHardwareModel(calCoreModel);
        bos = loadVBOs( calHardwareModel );
        //saveVBOs( bos, dir + "/vbos.cache" );
        //saveHardwareModel( calHardwareModel, dir + "/hwmodel.cache" );
        // ^ it's not loading task, cache preparation is export task
        // see `applications/preparer' for this.
    }

    // -- Preparing meshes and materials for fast Model creation --
    for(int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount();
        hardwareMeshId++)
    {
        calHardwareModel->selectHardwareMesh(hardwareMeshId);

        // -- Setup some fields --
        Mesh m;

        m.hardwareMeshId = hardwareMeshId;
        m.hardwareMesh   = &calHardwareModel->getVectorHardwareMesh()[ hardwareMeshId ];
        m.coreMesh       = calCoreModel->getCoreMesh( m.hardwareMesh->meshId );
        m.coreSubMesh    = m.coreMesh->getCoreSubmesh( m.hardwareMesh->submeshId );
        m.name           = m.coreMesh->getName();

        // -- Calculate maxBonesInfluence & rigidness --
        GLfloat* vertexBuffer = (GLfloat*) bos->vertexBuffer->getDataPointer();
        GLshort* matrixIndexBuffer = (GLshort*) bos->matrixIndexBuffer->getDataPointer();
        GLfloat* weightBuffer = (GLfloat*) bos->weightBuffer->getDataPointer();
        GLuint*  indexBuffer = (GLuint*) bos->indexBuffer->getDataPointer();

        GLuint* begin = &indexBuffer[ m.getIndexInVbo() ];
        GLuint* end   = begin + m.getIndexesCount();

        m.maxBonesInfluence = 0;
        m.rigid = true;
        bool unrigged = true; // all weights equal zero
        
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

            if ( weightBuffer[ *begin * 4 ] > 0.0 )
            {
                unrigged = false;
            }
                 
            if ( m.maxBonesInfluence < 1 && weightBuffer[ *begin * 4 + 0 ] > 0.0 )
                m.maxBonesInfluence = 1;
            if ( m.maxBonesInfluence < 2 && weightBuffer[ *begin * 4 + 1 ] > 0.0 )
                m.maxBonesInfluence = 2;
            if ( m.maxBonesInfluence < 3 && weightBuffer[ *begin * 4 + 2 ] > 0.0 )
                m.maxBonesInfluence = 3;
            if ( m.maxBonesInfluence < 4 && weightBuffer[ *begin * 4 + 3 ] > 0.0 )
                m.maxBonesInfluence = 4;
        }

        if ( unrigged )
        {
            m.rigid = true; // mesh is rigid when all its vertices are
                            // rigged to one bone with weight = 1.0,
                            // or when no one vertex is rigged at all
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

        std::cout << "mesh: " << m.name << std::endl
                  << "material:\n" << m.hwStateDesc << std::endl
                  << "  m.maxBonesInfluence       = " << m.maxBonesInfluence << std::endl
                  << "  m.hardwareMesh->meshId    = " << m.hardwareMesh->meshId << std::endl
                  << "  m.hardwareMesh->submeshId = " << m.hardwareMesh->submeshId << std::endl
                  << "  m.indexInVbo              = " << m.getIndexInVbo() << std::endl
                  << "  m.indexesCount            = " << m.getIndexesCount() << std::endl;
    }

    // -- Check zero weight bones --
    {
        MatrixIndexBuffer::iterator mi = bos->matrixIndexBuffer->begin();
        WeightBuffer::iterator      w  = bos->weightBuffer->begin();

        while ( mi < bos->matrixIndexBuffer->end() )
        {
            if ( w->x() <= 0.0 ) // no influences at all
            {
                w->x() = 1.0;
                mi->x() = 30;
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

    // -- Cleanup --
    delete bos;
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
    , shininess( glossiness )
{
    ambientColor.a() = opacity;
    diffuseColor.a() = opacity;
    specularColor.a() = opacity;
}


// -- HwStateDesc --

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
    float glossiness = 50;
    float opacity = 1.0;

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
            //bumpMap = dir + "/" + suffix;
            //shaderFlags |= SHADER_FLAG_BUMP_MAPPING;
        }
        else if ( prefix == "BumpAmount:" )
        {
            bumpMapAmount = stringToFloat( suffix );
        }
        else if ( prefix == "Opacity:" )
        {
            opacity = stringToFloat( suffix );
        }
        else if ( prefix == "Glossiness:" )
        {
            glossiness = stringToFloat( suffix ) * 128;
            // TODO: this must be done in exporter
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


// -- Materials cache --

osg::Material*
MaterialsCache::createMaterial( const MaterialDesc& desc )
{
    osg::Material* material = new osg::Material();

    material->setColorMode( osg::Material::AMBIENT_AND_DIFFUSE );
    material->setAmbient( osg::Material::FRONT_AND_BACK, desc.ambientColor );
    material->setDiffuse( osg::Material::FRONT_AND_BACK, desc.diffuseColor );
    material->setSpecular( osg::Material::FRONT_AND_BACK, desc.specularColor );
    material->setShininess( osg::Material::FRONT_AND_BACK, desc.shininess );
    // TODO: test shininess in sw mode

    return material;
}


// -- Textures cache --

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

    // -- setup normals map --
    if ( desc.normalsMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.normalsMap );

        stateSet->setTextureAttributeAndModes( 1, texture, osg::StateAttribute::ON );
        stateSet->addUniform( newIntUniform( osg::Uniform::SAMPLER_2D, "normalMap", 1 ) );
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
