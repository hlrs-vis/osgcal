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

#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>

#include <osgCal/Material>

using namespace osgCal;

std::ostream&
operator << ( std::ostream& os,
              const osgCal::HwStateDesc& sd )
{
    os << "ambientColor      : " << sd.material.ambientColor << std::endl
       << "diffuseColor      : " << sd.material.diffuseColor << std::endl
       << "specularColor     : " << sd.material.specularColor << std::endl
       << "glossiness        : " << sd.material.glossiness << std::endl
       << "duffuseMap        : " << sd.diffuseMap << std::endl
       << "normalsMap        : " << sd.normalsMap << std::endl
//       << "normalsMapAmount  : " << sd.normalsMapAmount << std::endl
       << "bumpMap           : " << sd.bumpMap << std::endl
       << "bumpMapAmount     : " << sd.bumpMapAmount << std::endl
       << "sides             : " << sd.sides << std::endl;
    return os;
}

// -- MaterialDesc --

static
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
                                 false
                                 /*lt( d1.shaderFlags,
                                   d2.shaderFlags, false )*/)))));
    return r;
}

#undef lt

static
float
stringToFloat( const std::string& s )
{
    float r;
    sscanf( s.c_str(), "%f", &r );
    return r;
}

static
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
//     , shaderFlags( 0 )
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
//            shaderFlags |= SHADER_FLAG_TEXTURING;
        }
        else if ( prefix == "NormalsMap:" )
        {
            normalsMap = dir + "/" + suffix;
//            shaderFlags |= SHADER_FLAG_NORMAL_MAPPING;
        }
        else if ( prefix == "BumpMap:" )
        {
            bumpMap = dir + "/" + suffix;
//            shaderFlags |= SHADER_FLAG_BUMP_MAPPING;
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
            if ( sides == 2 )
            {
//                shaderFlags |= SHADER_FLAG_TWO_SIDED;
            }
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
//        shaderFlags |= SHADER_FLAG_SHINING;
    }

    if ( material.diffuseColor.a() < 1 )
    {
//        shaderFlags |= SHADER_FLAG_OPACITY;
//        shaderFlags |= SHADER_FLAG_TWO_SIDED;
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
        v->setThreadSafeRefUnref( true );
        obj->setThreadSafeRefUnref( true );
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
    if ( isRGBAStateSet( stateSet ) || desc.material.diffuseColor.a() < 1 )
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

    return u;
}

HwMeshStateSetCache::HwMeshStateSetCache( SwMeshStateSetCache* swssc,
                                          TexturesCache* tc )
    : flags( 0 )
    , swMeshStateSetCache( swssc ? swssc : new SwMeshStateSetCache() )
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
    int transparent = stateSet->getRenderingHint() & osg::StateSet::TRANSPARENT_BIN;

    stateSet->setAttributeAndModes( shadersCache->get(
                                        desc.shaderFlags
                                        +
                                        ( flags & CoreModel::FOG_LINEAR ) / CoreModel::FOG_EXP
                                        * SHADER_FLAG_FOG_MODE_EXP
                                        +
                                        rgba * (SHADER_FLAG_RGBA | SHADER_FLAG_TWO_SIDED)
                                        ),
                                    osg::StateAttribute::ON );

    stateSet->addUniform( newFloatUniform( "glossiness", desc.material.glossiness ) );

    // -- setup normals map --
    if ( desc.normalsMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.normalsMap );

        stateSet->setTextureAttributeAndModes( 1, texture, osg::StateAttribute::ON );
        static osg::ref_ptr< osg::Uniform > normalMap =
            newIntUniform( osg::Uniform::SAMPLER_2D, "normalMap", 1 );
        stateSet->addUniform( normalMap.get() );
    }

    // -- setup bump map --
    if ( desc.bumpMap != "" )
    {
        osg::Texture2D* texture = texturesCache->get( desc.bumpMap );        

        stateSet->setTextureAttributeAndModes( 2, texture, osg::StateAttribute::ON );
        static osg::ref_ptr< osg::Uniform > bumpMap =
            newIntUniform( osg::Uniform::SAMPLER_2D, "bumpMap", 2 );
        stateSet->addUniform( bumpMap.get() );
        stateSet->addUniform( newFloatUniform( "bumpMapAmount", desc.bumpMapAmount ) );
    }

    // -- setup some uniforms --
    if ( desc.diffuseMap != "" )
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
DepthMeshStateSetCache::get( const HwStateDesc& desc )
{
    int bonesCount = desc.shaderFlags / SHADER_FLAG_BONES(1);
    return getOrCreate( cache, std::make_pair( bonesCount, desc.sides ), this,
                        &DepthMeshStateSetCache::createDepthMeshStateSet );
}

osg::StateSet*
DepthMeshStateSetCache::createDepthMeshStateSet( const std::pair< int, int >& boneAndSidesCount )
{
    osg::StateSet* stateSet = new osg::StateSet();

    stateSet->setAttributeAndModes( shadersCache->get(
                                        boneAndSidesCount.first * SHADER_FLAG_BONES(1)
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
