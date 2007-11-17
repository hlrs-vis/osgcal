/* -*- c++ -*-
   Copyright (C) 2007 Vladimir Shabanov <vshabanoff@gmail.com>

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
//#include <memory>

#include <osg/DeleteHandler>
#include <osg/Texture2D>
#include <osg/Image>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/ColorMask>

#include <osgDB/ReadFile>

#include <osgCal/StateSetCache>
#include <osgCal/CoreModel>

// -- Shader cache --

using namespace osgCal;

int
osgCal::materialShaderFlags( const Material& material )
{
    int flags = 0;

    if ( !material.diffuseMap.empty() )
    {
        flags |= SHADER_FLAG_TEXTURING;
    }
    if ( !material.normalsMap.empty() )
    {
        flags |= SHADER_FLAG_NORMAL_MAPPING;
    }
    if ( !material.bumpMap.empty() )
    {
        flags |= SHADER_FLAG_BUMP_MAPPING;
    }
    if ( material.sides == 2 )
    {
        flags |= SHADER_FLAG_TWO_SIDED;
    }

    if (    material.specularColor.r() != 0
            || material.specularColor.g() != 0
            || material.specularColor.b() != 0 )
    {
        flags |= SHADER_FLAG_SHINING;
    }

    if ( material.diffuseColor.a() < 1 )
    {
        flags |= SHADER_FLAG_OPACITY;
        flags |= SHADER_FLAG_TWO_SIDED;
    }

    return flags;
}


ShadersCache::~ShadersCache()
{
    osg::notify( osg::DEBUG_FP ) << "destroying ShadersCache... " << std::endl;
    vertexShaders.clear();
    fragmentShaders.clear();
    programs.clear();
    osg::notify( osg::DEBUG_FP ) << "ShadersCache destroyed" << std::endl;
}

osg::Program*
ShadersCache::get( int flags )
{
//     if ( flags &
//          (SHADER_FLAG_BONES(1) | SHADER_FLAG_BONES(2)
//           | SHADER_FLAG_BONES(3) | SHADER_FLAG_BONES(4)) )
//     {
//         flags &= ~SHADER_FLAG_BONES(0)
//             & ~SHADER_FLAG_BONES(1) & ~SHADER_FLAG_BONES(2)
//             & ~SHADER_FLAG_BONES(3) & ~SHADER_FLAG_BONES(4);
//         flags |= SHADER_FLAG_BONES(4);
//     }
//     BTW, not so much difference between always 4 bone and per-bones count shaders

    if ( flags & SHADER_FLAG_DEPTH_ONLY )
    {
        flags &= DEPTH_ONLY_MASK; 
    }

    ProgramsMap::const_iterator pmi = programs.find( flags );

    if ( pmi != programs.end() )
    {
        return pmi->second.get();
    }
    else
    {
#define PARSE_FLAGS                                                     \
        int BONES_COUNT = flags / SHADER_FLAG_BONES(1);                 \
        int RGBA = ( SHADER_FLAG_RGBA & flags ) ? 1 : 0;                \
        int FOG_MODE = ( SHADER_FLAG_FOG_MODE_MASK & flags );           \
        int FOG = FOG_MODE != 0;                                        \
        int OPACITY = ( SHADER_FLAG_OPACITY & flags ) ? 1 : 0;          \
        int TEXTURING = ( SHADER_FLAG_TEXTURING & flags ) ? 1 : 0;      \
        int NORMAL_MAPPING = ( SHADER_FLAG_NORMAL_MAPPING & flags ) ? 1 : 0; \
        int BUMP_MAPPING = ( SHADER_FLAG_BUMP_MAPPING & flags ) ? 1 : 0; \
        int SHINING = ( SHADER_FLAG_SHINING & flags ) ? 1 : 0;          \
        int DEPTH_ONLY = ( SHADER_FLAG_DEPTH_ONLY & flags ) ? 1 : 0;    \
        int TWO_SIDED = ( SHADER_FLAG_TWO_SIDED & flags ) ? 1 : 0
        
        PARSE_FLAGS;
        (void)FOG; // remove unused variable warning
                
        osg::Program* p = new osg::Program;

        char name[ 256 ];
        sprintf( name, "skeletal shader (%d bones%s%s%s%s%s%s%s%s%s)",
                 BONES_COUNT,
                 DEPTH_ONLY ? ", depth_only" : "",
                 (FOG_MODE == SHADER_FLAG_FOG_MODE_EXP ? ", fog_exp"
                  : (FOG_MODE == SHADER_FLAG_FOG_MODE_EXP2 ? ", fog_exp2"
                     : (FOG_MODE == SHADER_FLAG_FOG_MODE_LINEAR ? ", fog_linear" : ""))),
                 RGBA ? ", rgba" : "",
                 OPACITY ? ", opacity" : "",
                 TEXTURING ? ", texturing" : "",
                 NORMAL_MAPPING ? ", normal mapping" : "",
                 BUMP_MAPPING ? ", bump mapping" : "",
                 SHINING ? ", shining" : "",
                 TWO_SIDED ? ", two-sided" : ""
            );

        p->setThreadSafeRefUnref( true );
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
        

osg::Shader*
ShadersCache::getVertexShader( int flags )
{           
    flags &= ~SHADER_FLAG_RGBA
        & ~SHADER_FLAG_OPACITY
//        & ~SHADER_FLAG_TWO_SIDED
        & ~SHADER_FLAG_SHINING;
    // remove irrelevant flags that can lead to
    // duplicate shaders in map
    if ( flags & SHADER_FLAG_FOG_MODE_MASK )
    {
        flags |= SHADER_FLAG_FOG_MODE_MASK;
        // ^ vertex shader only need to know that FOG is needed
        // fog mode is irrelevant
    }

    ShadersMap::const_iterator smi = vertexShaders.find( flags );

    if ( smi != vertexShaders.end() )
    {
        return smi->second.get();
    }
    else
    {                
        PARSE_FLAGS;
        (void)RGBA, (void)OPACITY, (void)SHINING, (void)FOG_MODE;
        // remove unused variable warning

        std::string shaderText;

        if ( DEPTH_ONLY )
        {
            #include "shaders/SkeletalDepthOnly_vert.h"
        }
        else
        {                    
            #include "shaders/Skeletal_vert.h"
        }

        osg::Shader* vs = new osg::Shader( osg::Shader::VERTEX,
                                           shaderText.data() );
        vs->setThreadSafeRefUnref( true );
        vertexShaders[ flags ] = vs;
        return vs;
    }
}

osg::Shader*
ShadersCache::getFragmentShader( int flags )
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

        if ( DEPTH_ONLY )
        {
            #include "shaders/SkeletalDepthOnly_frag.h"
        }
        else
        {                    
            #include "shaders/Skeletal_frag.h"
        }

        osg::Shader* fs = new osg::Shader( osg::Shader::FRAGMENT,
                                           shaderText.data() );
        fs->setThreadSafeRefUnref( true );
        fragmentShaders[ flags ] = fs;
        return fs;
    }
}


/**
 * Global instance of ShadersCache.
 * There is only one shader instance which is created with first CoreModel
 * and destroyed after last CoreModel destroyed (and hence all Models destroyed also
 * since they are referring to CoreModel).
 */
static OpenThreads::Mutex shadersCacheMutex;
static osg::Referenced* shadersCache = NULL;

class SingletonDeleteHandler : public osg::DeleteHandler
{
    public:

        OpenThreads::Mutex& mutex;
        osg::Referenced**   r;

        SingletonDeleteHandler( OpenThreads::Mutex& m,
                                osg::Referenced**   _r )                                
            : mutex( m )
            , r( _r )
        {}
        
        virtual void requestDelete(const osg::Referenced* object)
        {
            mutex.lock();
            if ( (*r)->referenceCount() == 0 )
            {
                *r = NULL; // clear global variable
                mutex.unlock();
                osg::DeleteHandler::requestDelete( object );
            }
            else // someone referenced shared 'r' just before deletion
            {
                mutex.unlock();
                // do nothing
            }
        }
        
};

/**
 * MUST be used and referenced inside shadersCacheMutex.lock/unlock
 */
static
void
allocShadersCache()
{
    if ( shadersCache == NULL )
    {
        shadersCache = new ShadersCache();
        shadersCache->setThreadSafeRefUnref( true );
        shadersCache->setDeleteHandler( new SingletonDeleteHandler( shadersCacheMutex,
                                                                    &shadersCache ) );
    }
}

// -- StateSetCache --

StateSetCache::StateSetCache()
{
    setThreadSafeRefUnref( true );

    TexturesCache* texturesCache = new TexturesCache();
    swMeshStateSetCache = new SwMeshStateSetCache( new MaterialsCache(),
                                                   texturesCache );
    shadersCacheMutex.lock();
    allocShadersCache(); // alloc and reference inside mutex
    hwMeshStateSetCache = new HwMeshStateSetCache( swMeshStateSetCache.get(),
                                                   texturesCache,
                                                   static_cast< ShadersCache* >( shadersCache ) );
    depthMeshStateSetCache = new DepthMeshStateSetCache( static_cast< ShadersCache* >( shadersCache ) );
    shadersCacheMutex.unlock();
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
        v->setThreadSafeRefUnref( true );
        obj->setThreadSafeRefUnref( true );
        map[ key ] = v;
        return v;
    }
}
                 

// -- Materials cache --

osg::Material*
MaterialsCache::get( const OsgMaterial& md )
{
    return getOrCreate( cache, md, this, &MaterialsCache::createMaterial );
}

osg::Material*
MaterialsCache::createMaterial( const OsgMaterial& desc )
{
    osg::Material* material = new osg::Material();

    material->setColorMode( osg::Material::AMBIENT_AND_DIFFUSE );
    material->setAmbient( osg::Material::FRONT_AND_BACK, desc.ambientColor );
    material->setDiffuse( osg::Material::FRONT_AND_BACK, desc.diffuseColor );
    material->setSpecular( osg::Material::FRONT_AND_BACK, desc.specularColor );
    material->setShininess( osg::Material::FRONT_AND_BACK,
                            desc.glossiness > 128.0 ? 128.0 : desc.glossiness );

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
    img->setThreadSafeRefUnref( true );

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
    img->setDataVariance( osg::Object::STATIC ); // unnecessary
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

/**
 * Some global state attributes. Just to not allocate new duplicated
 * attributes for each state set that use them. We get nearly no
 * performance nor memory benefit from this.
 */
struct osgCalStateAttributes
{
        osg::ref_ptr< osg::BlendFunc > blending;
        osg::ref_ptr< osg::Depth >     depthFuncLessWriteMaskTrue;
        osg::ref_ptr< osg::Depth >     depthFuncLessWriteMaskFalse;
        osg::ref_ptr< osg::Depth >     depthFuncLequalWriteMaskFalse;
        osg::ref_ptr< osg::CullFace >  backFaceCulling;
        osg::ref_ptr< osg::ColorMask > noColorWrites;

        osgCalStateAttributes()
        {
            blending = new osg::BlendFunc;
            blending->setFunction( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            blending->setThreadSafeRefUnref( true );

            depthFuncLessWriteMaskTrue = new osg::Depth( osg::Depth::LESS, 0.0, 1.0, true );
            depthFuncLessWriteMaskTrue->setThreadSafeRefUnref( true );
            depthFuncLessWriteMaskFalse = new osg::Depth( osg::Depth::LESS, 0.0, 1.0, false );
            depthFuncLessWriteMaskFalse->setThreadSafeRefUnref( true );
            depthFuncLequalWriteMaskFalse = new osg::Depth( osg::Depth::LEQUAL, 0.0, 1.0, false );
            depthFuncLequalWriteMaskFalse->setThreadSafeRefUnref( true );

            backFaceCulling = new osg::CullFace( osg::CullFace::BACK );
            backFaceCulling->setThreadSafeRefUnref( true );

            noColorWrites = new osg::ColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
            noColorWrites->setThreadSafeRefUnref( true );
        }
};

namespace osgCal
{
    static osgCalStateAttributes stateAttributes;
}
using osgCal::stateAttributes;

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
    stateSet->setAttributeAndModes( stateAttributes.blending.get(),
                                    osg::StateAttribute::ON );

    // turn off depth writes
//     stateSet->setAttributeAndModes( stateAttributes.depthFuncLequalWriteMaskFalse.get(),
//                                     osg::StateAttribute::ON );
    // ^ incorrect in cases when transparency is used as alpha test
}

SwMeshStateSetCache::SwMeshStateSetCache( MaterialsCache* mc,
                                          TexturesCache* tc )
    : materialsCache( mc )
    , texturesCache( tc )
{}
            
osg::StateSet*
SwMeshStateSetCache::get( const SoftwareMaterial& swsd )
{
    return getOrCreate( cache, swsd, this,
                        &SwMeshStateSetCache::createSwMeshStateSet );
}
osg::StateSet*
SwMeshStateSetCache::createSwMeshStateSet( const SoftwareMaterial& desc )
{
    osg::StateSet* stateSet = new osg::StateSet();

    // -- setup material --
    osg::Material* material = materialsCache->get( *static_cast< const OsgMaterial* >( &desc ) );
    stateSet->setAttributeAndModes( material, osg::StateAttribute::ON );    

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
            stateSet->setAttributeAndModes( stateAttributes.backFaceCulling.get(),
                                            osg::StateAttribute::ON |
                                            osg::StateAttribute::PROTECTED );
            // PROTECTED, since overriding of this state (for example by
            // pressing 'b') leads to incorrect display
            break;

        case 2:
            // two sided mesh -- force no culling (for sw mesh)
            stateSet->setAttributeAndModes( stateAttributes.backFaceCulling.get(),
                                            osg::StateAttribute::OFF |
                                            osg::StateAttribute::PROTECTED );
            break;

        default:
            // undefined sides count -- use default OSG culling
            break;
    }

    // -- Check transparency modes --
    if ( isRGBAStateSet( stateSet ) || desc.diffuseColor.a() < 1 )
    {
        setupTransparentStateSet( stateSet );
        // and force backface culling
        stateSet->setAttributeAndModes( stateAttributes.backFaceCulling.get(),
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

static
osg::Uniform*
newFloatUniform( const std::string& name,
                 GLfloat value )
{
    osg::Uniform* u = new osg::Uniform( osg::Uniform::FLOAT, name );
    u->set( value );
    u->setThreadSafeRefUnref( true );

    return u;
}

static
osg::Uniform*
newIntUniform( osg::Uniform::Type type,
               const std::string& name,
               int value )
{
    osg::Uniform* u = new osg::Uniform( type, name );
    u->set( value );
    u->setThreadSafeRefUnref( true );

    return u;
}

HwMeshStateSetCache::HwMeshStateSetCache( SwMeshStateSetCache* swssc,
                                          TexturesCache* tc,
                                          ShadersCache* sc )
    : flags( 0 )
    , swMeshStateSetCache( swssc )
    , texturesCache( tc )
    , shadersCache( sc )
{}
            
osg::StateSet*
HwMeshStateSetCache::get( const Material& swsd,
                          int bonesCount )
{
    return getOrCreate( cache, std::make_pair( swsd, bonesCount ), this,
                        &HwMeshStateSetCache::createHwMeshStateSet );
}

osg::StateSet*
HwMeshStateSetCache::createHwMeshStateSet( const std::pair< Material, int >& matAndBones )
{
    const Material& material = matAndBones.first;
    int bonesCount           = matAndBones.second;
    
    osg::StateSet* baseStateSet = swMeshStateSetCache->
        get( *static_cast< const SoftwareMaterial* >( &material ) );

    osg::StateSet* stateSet = new osg::StateSet( *baseStateSet );
//    stateSet->merge( *baseStateSet );
    // 50 fps downs to 48 when merge used instead of new StateSet(*base)

    // -- Setup shader --
    int rgba = isRGBAStateSet( stateSet );
    int transparent = stateSet->getRenderingHint() & osg::StateSet::TRANSPARENT_BIN;

    stateSet->setAttributeAndModes( shadersCache->get(
                                        materialShaderFlags( material )
                                        +
                                        SHADER_FLAG_BONES( bonesCount )
                                        +
                                        ( flags & CoreModel::FOG_LINEAR ) / CoreModel::FOG_EXP
                                        * SHADER_FLAG_FOG_MODE_EXP
                                        +
                                        rgba * (SHADER_FLAG_RGBA | SHADER_FLAG_TWO_SIDED)
                                        ),
                                    osg::StateAttribute::ON );

    stateSet->addUniform( newFloatUniform( "glossiness", material.glossiness ) );

    // -- setup normals map --
    if ( material.normalsMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( material.normalsMap );

        stateSet->setTextureAttributeAndModes( 1, texture, osg::StateAttribute::ON );
        static osg::ref_ptr< osg::Uniform > normalMap =
            newIntUniform( osg::Uniform::SAMPLER_2D, "normalMap", 1 );
        stateSet->addUniform( normalMap.get() );
    }

    // -- setup bump map --
    if ( material.bumpMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( material.bumpMap );        

        stateSet->setTextureAttributeAndModes( 2, texture, osg::StateAttribute::ON );
        static osg::ref_ptr< osg::Uniform > bumpMap =
            newIntUniform( osg::Uniform::SAMPLER_2D, "bumpMap", 2 );
        stateSet->addUniform( bumpMap.get() );
        stateSet->addUniform( newFloatUniform( "bumpMapAmount", material.bumpMapAmount ) );
    }

    // -- setup some uniforms --
    if ( material.diffuseMap != "" )
    {
        static osg::ref_ptr< osg::Uniform > decalMap =
            newIntUniform( osg::Uniform::SAMPLER_2D, "decalMap", 0 );
        stateSet->addUniform( decalMap.get() );
    }

    // -- Depth first mode setup --
    if ( flags & CoreModel::USE_DEPTH_FIRST_MESHES
         &&
         !transparent )
    {
        stateSet->setAttributeAndModes( stateAttributes.depthFuncLequalWriteMaskFalse.get(),
                                        osg::StateAttribute::ON |
                                        osg::StateAttribute::PROTECTED );
        // ^ need LEQUAL instead of LESS since depth values are
        // already written, also there is no need to write depth
        // values
        // TODO: depth first meshes are incompatible with transparent
        // ones, since they need to be drawn last (TRANSPARENT_BIN) in
        // back to fron order.
    }
    
//     static osg::ref_ptr< osg::Uniform > clipPlanesUsed =
//         new osg::Uniform( "clipPlanesUsed", false );
//     stateSet->addUniform( clipPlanesUsed.get() );

    return stateSet;
}

osg::StateSet*
DepthMeshStateSetCache::get( const Material& m,
                             int bonesCount )
{
    return getOrCreate( cache, std::make_pair( bonesCount, m.sides ), this,
                        &DepthMeshStateSetCache::createDepthMeshStateSet );
}

osg::StateSet*
DepthMeshStateSetCache::createDepthMeshStateSet( const std::pair< int, int >& boneAndSidesCount )
{
    osg::StateSet* stateSet = new osg::StateSet();

    stateSet->setAttributeAndModes( shadersCache->get(
                                        SHADER_FLAG_BONES( boneAndSidesCount.first )
                                        +
                                        SHADER_FLAG_DEPTH_ONLY ),
                                    osg::StateAttribute::ON );
    // -- setup sidedness --
    switch ( boneAndSidesCount.second )
    {
        case 1:
            // one sided mesh -- force backface culling
            stateSet->setAttributeAndModes( stateAttributes.backFaceCulling.get(),
                                            osg::StateAttribute::ON |
                                            osg::StateAttribute::PROTECTED );
            // PROTECTED, since overriding of this state (for example by
            // pressing 'b') leads to incorrect display
            break;

        case 2:
            // two sided mesh -- no culling (for sw mesh)
            stateSet->setAttributeAndModes( stateAttributes.backFaceCulling.get(),
                                            osg::StateAttribute::OFF |
                                            osg::StateAttribute::PROTECTED );
            break;

        default:
            // undefined sides count -- use default OSG culling
            break;
    }

    stateSet->setAttributeAndModes( stateAttributes.depthFuncLessWriteMaskTrue.get(),
                                    osg::StateAttribute::ON |
                                    osg::StateAttribute::PROTECTED );
    stateSet->setAttributeAndModes( stateAttributes.noColorWrites.get(),
                                    osg::StateAttribute::ON |
                                    osg::StateAttribute::PROTECTED );

    stateSet->setRenderBinDetails( -10, "RenderBin" ); // we render depth meshes before any other meshes

    return stateSet;
}
