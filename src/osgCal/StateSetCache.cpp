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
#include <osg/observer_ptr>
#include <osg/Texture2D>
#include <osg/Image>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/ColorMask>

#include <osgDB/ReadFile>

#include <osgCal/StateSetCache>
#include <osgCal/CoreModel>

using namespace osgCal;

// -- StateSetCache --

StateSetCache::StateSetCache()
{
    TexturesCache* texturesCache = new TexturesCache();
    swMeshStateSetCache = new SwMeshStateSetCache( new MaterialsCache(),
                                                   texturesCache );

    hwMeshStateSetCache = new HwMeshStateSetCache( swMeshStateSetCache.get(),
                                                   texturesCache,
                                                   ShadersCache::instance() );
    depthMeshStateSetCache = new DepthMeshStateSetCache( ShadersCache::instance() );
}

static osg::observer_ptr< StateSetCache >  stateSetCache;

StateSetCache*
StateSetCache::instance()
{
    if ( !stateSetCache.valid() )
    {
        stateSetCache = new StateSetCache;
    }

    return stateSetCache.get();
}



// -- Caches --

/**
 * Observer which removes objects from cache when they are no more
 * used.
 */
template < typename Map >
class CachedObjectObserver : public osg::Observer
{
    public:
        CachedObjectObserver( Map* _map,
                              const typename Map::key_type& _key,
                              osg::Referenced* _container )
            : map( _map )
            , key( _key )
            , container( _container )
        {}
        
        virtual void objectDeleted( void* )
        {
            //std::cout << "erasing: "/* << key*/ << std::endl;
            map->erase( key );
            delete this;
        }

    private:
        Map* map;
        typename Map::key_type key;
        osg::ref_ptr< osg::Referenced > container;
};


template < typename Key, typename Value, typename Class >
Value*
getOrCreate( std::map< Key, Value* >& map,
             const Key&               key,
             Class*                   obj,
             Value*        ( Class::*create )( const Key& ) )
{
    typename std::map< Key, Value* >::const_iterator i = map.find( key );
    if ( i != map.end() )
    {
        return i->second;
    }
    else
    {
        Value* v = (obj ->* create)( key ); // damn c++!
        map[ key ] = v;
        v->addObserver( new CachedObjectObserver< std::map< Key, Value* > >( &map, key, obj ) );
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
    //img->setThreadSafeRefUnref( true );

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
            //blending->setThreadSafeRefUnref( true );

            depthFuncLessWriteMaskTrue = new osg::Depth( osg::Depth::LESS, 0.0, 1.0, true );
            //depthFuncLessWriteMaskTrue->setThreadSafeRefUnref( true );
            depthFuncLessWriteMaskFalse = new osg::Depth( osg::Depth::LESS, 0.0, 1.0, false );
            //depthFuncLessWriteMaskFalse->setThreadSafeRefUnref( true );
            depthFuncLequalWriteMaskFalse = new osg::Depth( osg::Depth::LEQUAL, 0.0, 1.0, false );
            //depthFuncLequalWriteMaskFalse->setThreadSafeRefUnref( true );

            backFaceCulling = new osg::CullFace( osg::CullFace::BACK );
            //backFaceCulling->setThreadSafeRefUnref( true );

            noColorWrites = new osg::ColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
            //noColorWrites->setThreadSafeRefUnref( true );
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
            stateSet->setMode( GL_VERTEX_PROGRAM_TWO_SIDE_ARB,
                               osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON );
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
        stateSet->setMode( GL_VERTEX_PROGRAM_TWO_SIDE_ARB,
                           osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON );
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
    //u->setThreadSafeRefUnref( true );

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
    //u->setThreadSafeRefUnref( true );

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
                          int bonesCount,
                          bool useDepthFirstMesh )
{
    return getOrCreate( cache,
                        std::make_pair( swsd,
                                        std::make_pair( bonesCount, useDepthFirstMesh ) ),
                        this,
                        &HwMeshStateSetCache::createHwMeshStateSet );
}

osg::StateSet*
HwMeshStateSetCache::createHwMeshStateSet( const std::pair< Material,
                                           std::pair< int, bool > >& matAndBones )
{
    const Material& material = matAndBones.first;
    int bonesCount           = matAndBones.second.first;
    bool useDepthFirstMesh   = matAndBones.second.second;
    
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
    if ( useDepthFirstMesh && !transparent )
    {
        stateSet->setAttributeAndModes( stateAttributes.depthFuncLequalWriteMaskFalse.get(),
                                        osg::StateAttribute::ON |
                                        osg::StateAttribute::PROTECTED );
        // ^ need LEQUAL instead of LESS since depth values are
        // already written, also there is no need to write depth
        // values
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
