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

#include <osg/io_utils>

#include <osgDB/FileNameUtils>

#include <osgCal/Material>

using namespace osgCal;

std::ostream&
osgCal::operator << ( std::ostream& os,
                      const osgCal::Material& sd )
{
    os << "ambientColor      : " << sd.ambientColor << std::endl
       << "diffuseColor      : " << sd.diffuseColor << std::endl
       << "specularColor     : " << sd.specularColor << std::endl
       << "glossiness        : " << sd.glossiness << std::endl
       << "duffuseMap        : " << sd.diffuseMap << std::endl
       << "normalsMap        : " << sd.normalsMap << std::endl
//       << "normalsMapAmount  : " << sd.normalsMapAmount << std::endl
       << "bumpMap           : " << sd.bumpMap << std::endl
       << "bumpMapAmount     : " << sd.bumpMapAmount << std::endl
       << "sides             : " << sd.sides << std::endl;
    return os;
}

// -- OsgMaterial --

static
osg::Vec4
colorToVec4( const CalCoreMaterial::Color& color )
{
    return osg::Vec4( color.red / 255.f,
                      color.green / 255.f,
                      color.blue / 255.f,
                      color.alpha == 0 ? 1.f : (color.alpha / 255.f) );
}

void
OsgMaterial::setupOsgMaterial( CalCoreMaterial* m,
                               float _glossiness,
                               float opacity )
{
    ambientColor = colorToVec4( m->getAmbientColor() );
    diffuseColor =  colorToVec4( m->getDiffuseColor() );
    specularColor = colorToVec4( m->getSpecularColor() ) * m->getShininess();
    glossiness = _glossiness;
    ambientColor.a() = opacity;
    diffuseColor.a() = opacity;
    specularColor.a() = opacity;
}

#define lt( a, b, t ) (a < b ? true : ( b < a ? false : t ))
// // a < b || (!( b < a ) && t )
    
bool
osgCal::operator < ( const OsgMaterial& om1,
                     const OsgMaterial& om2 )
{
    bool r = lt( om1.ambientColor,
                 om2.ambientColor,
                 lt( om1.diffuseColor,
                     om2.diffuseColor,
                     lt( om1.specularColor,
                         om2.specularColor,
                         lt( om1.glossiness,
                             om2.glossiness, false ))));
    return r;
}

// -- SoftwareMaterial --

bool
osgCal::operator < ( const SoftwareMaterial& d1,
                     const SoftwareMaterial& d2 )
{
    bool r = lt( *static_cast< const OsgMaterial* >( &d1 ),
                 *static_cast< const OsgMaterial* >( &d2 ),
                 lt( d1.diffuseMap,
                     d2.diffuseMap,
                     lt( d1.sides,
                         d2.sides, false )));
    return r;
}

// -- HwStateDesc --

bool
osgCal::operator < ( const Material& d1,
                     const Material& d2 )
{
    bool r = lt( *static_cast< const SoftwareMaterial* >( &d1 ),
                 *static_cast< const SoftwareMaterial* >( &d2 ),
                 lt( d1.normalsMap,
                     d2.normalsMap,
                     lt( d1.bumpMap,
                         d2.bumpMap,
                         lt( d1.normalsMapAmount,
                             d2.normalsMapAmount,
                             lt( d1.bumpMapAmount,
                                 d2.bumpMapAmount,
                                 false)))));
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

/**
 * Return "prefix" from "prefix:foo:bar/baz" or empty string
 * when no ':' found.
 */
static
std::string
getPrefix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_first_of(":");

    if(pos == std::string::npos) return std::string();

    return str.substr(0, pos + 1);
}

/**
 * Return "suffix" from "foo:bar/baz:suffix" or whole string
 * when no ':' found.
 */
static
std::string
getSuffix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_last_of(":");

    if(pos == std::string::npos) return str;

    return str.substr(pos + 1);
}

// static
// bool
// endsWith( const std::string& str,
//           const std::string& suffix )
// {
//     std::string::size_type suffl = suffix.length();;
//     std::string::size_type strl = str.length();;

//     return ( strl >= suffl && str.substr( strl - suffl ) == suffix );
// }

// static
// std::string
// getAfter( const std::string& prefix,
//           const std::string& str )
// {
//     return str.substr(prefix.size());
// }

// static
// bool
// prefixEquals( const std::string& str,
//               const std::string& prefix )
// {
//     if ( str.length() < prefix.length() )
//     {
//         return false;
//     }
//     else
//     {
//         return str.substr( 0, prefix.length() ) == prefix;
//     }
// }

inline
std::string
concatPaths( const std::string& dir,
             const std::string& file )
{
    return osgDB::getRealPath( osgDB::concatPaths( dir, file ) );
}

Material::Material( CalCoreMaterial* m,
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
    // are not the bottleneck
    
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
            diffuseMap = concatPaths( dir, suffix );
        }
        else if ( prefix == "NormalsMap:" )
        {
            normalsMap = concatPaths( dir, suffix );
        }
        else if ( prefix == "BumpMap:" )
        {
            bumpMap = concatPaths( dir, suffix );
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
    setupOsgMaterial( m, glossiness, opacity );

    if ( diffuseMap != "" ) // set diffuse color to white when texturing
    {
        diffuseColor.r() = 1.0;
        diffuseColor.g() = 1.0;
        diffuseColor.b() = 1.0;
    }
}
