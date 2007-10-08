//----------------------------------------------------------------------------//
// MaxMaterial.cpp                                                            //
// Copyright (C) 2001, 2002 Bruno 'Beosil' Heidelberger                       //
//----------------------------------------------------------------------------//
// This program is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU General Public License as published by the Free //
// Software Foundation; either version 2 of the License, or (at your option)  //
// any later version.                                                         //
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// Includes                                                                   //
//----------------------------------------------------------------------------//

#include "StdAfx.h"
#include "Exporter.h"
#include "MaxMaterial.h"
#include "BaseInterface.h"

//----------------------------------------------------------------------------//
// Debug                                                                      //
//----------------------------------------------------------------------------//

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//----------------------------------------------------------------------------//
// Constructors                                                               //
//----------------------------------------------------------------------------//

CMaxMaterial::CMaxMaterial()
{
	m_pIStdMat = 0;
}

//----------------------------------------------------------------------------//
// Destructor                                                                 //
//----------------------------------------------------------------------------//

CMaxMaterial::~CMaxMaterial()
{
}

//----------------------------------------------------------------------------//
// Create a max material instance                                             //
//----------------------------------------------------------------------------//

std::string
className( Texmap* pTexMap )
{
	CStr className;
	pTexMap->GetClassNameA( className ); // Normal Bump

	return className.data();
}

// bool 
// bumpTexmap( Texmap* pTexMap )
// {
// 	return ( className( pTexMap ) == "Bump" );
// }
				
bool 
normalBumpTexmap( Texmap* pTexMap )
{
	return ( className( pTexMap ) == "Normal Bump" );
}
				
bool 
bitmapTexTexmap( Texmap* pTexMap )
{
	return ( className( pTexMap ) == "Bitmap" );
}

/**
 * Changes file extension to specified. Extension specified with point (e.g. ".dds")
 */
std::string
changeExtensionTo( const std::string& fn,
                   const std::string& newExt )
{
	std::string::size_type pos;
	pos = fn.find_last_of('.');
    
	if ( pos == std::string::npos )
    {
        return fn + newExt;
    }
    else
    {
        return fn.substr(0, pos) + newExt;
    }
}

/**
 * Get file name (w/o path) corresponding to BitmapTex
 */
std::string
BitmapTexFileName( Texmap* tm,
                   const std::string& type = std::string() )
{
    BitmapTex* bt = static_cast< BitmapTex* >( tm );
	// get the full filepath
	std::string strFilename;
	strFilename = changeExtensionTo( bt->GetMapName(), (type == "") ? ".dds" : "." + type + ".dds");

	// extract the filename
	std::string::size_type pos;
	pos = strFilename.find_last_of("\\/:");

	if(pos == std::string::npos) return strFilename;

	return strFilename.substr(pos + 1);
}

std::string
intToString( int i )
{
    char buf[ 1024 ];

    sprintf( buf, "%d", i );

    return std::string( buf );
}

std::string
floatToString( float f )
{
    char buf[ 1024 ];

    sprintf( buf, "%d.%06d", int(floor( f )), int(fmod( f, 1.0f ) * 1000000));
	// we don't use %f since in Russian locale we get (,) as digit separator.

    return std::string( buf );
}

float
maxGlossinessToPhongGlossiness( float g )
{
	//return 128*g;
    
	// 3DSMax Glossiness parameter makes light spot size linearly
	// decrease as glossiness grow from 0 to 100%.
    // The Phong cos(R,E)^g makes non-linear light spot decreasing with
	// linear grow of g parameter.
    // We need some function that map g_3dmax to g_phong so that light
	// spot size decrease linearly.
    // We can use angle between R and E (cos(R,E) or dot(R,E)) as
	// light spot diameter:
    //   cos(R,E)^g_phong = const = color_at_RE_diameter (~0.0)
    //   ln (cos(R,E)^g_phong) = ln const
    //   d = R,E; g_phong - f d; d <- (0..pi/2); g_3dmax <- [1..0]
    //   f d * ln cos d = ln const
    //   f d = ln const / ln cos d
    //   d_min = ~0; d_max = ~pi/2
    //   d = (1 - g_3dmax) * (d_max - d_min) + d_min

/*    const double d_min = 0.001;
    const double d_max = PI/2 - 0.3;
    const double light_spot_end_color = 0.0001;
    
    double d = (1.0 - g) * (d_max - d_min) + d_min;
    
    return log( light_spot_end_color ) / log( cos( d ) );*/
    
    const double light_spot_end_color = 0.1;// smaller value => smaller spots
    double d = 1.0 / pow( 2.0, g / 0.2 );// max decrease light spot exponentially
    
    return log( light_spot_end_color ) / log( cos( d ) );
}

bool CMaxMaterial::Create(StdMat *pIStdMat)
{
	// check if the internal node is valid
	if(pIStdMat == 0)
	{
		theExporter.SetLastError("Invalid handle.", __FILE__, __LINE__);
		return false;
	}

	m_pIStdMat = pIStdMat;

    mapFilenames.push_back( "Sides:" + std::string( m_pIStdMat->GetTwoSided() ? "2" : "1" ) );
    mapFilenames.push_back( "Opacity:" + floatToString(
                                m_pIStdMat->GetOpacity( 0 /*zero time*/) ) );
    mapFilenames.push_back( "Glossiness:" + floatToString(
                                maxGlossinessToPhongGlossiness( m_pIStdMat->GetShininess( 0 /*zero time*/) ) ) );
    mapFilenames.push_back( "SelfIllumination:" + floatToString(
                                m_pIStdMat->GetSelfIllum( 0 /*zero time*/) ) );
    switch ( m_pIStdMat->GetShading() )
    {
        case SHADE_PHONG:
            mapFilenames.push_back( "Shading:Phong" );
            break;

        case SHADE_BLINN:
            mapFilenames.push_back( "Shading:Blinn" );
            break;

    };    

	// loop through all maps of the material
	for( int materialMapId = 0; materialMapId < m_pIStdMat->NumSubTexmaps(); materialMapId++ )
	{
        Texmap* pTexMap = pIStdMat->GetSubTexmap( materialMapId );

        // check if the map is valid
        if((pTexMap == 0) || (!pIStdMat->MapEnabled(materialMapId)))
        {
            continue;
        }

		// check slots to handle
        if ( std::string( m_pIStdMat->GetSubTexmapSlotName(materialMapId) ) == "Diffuse Color"
             && bitmapTexTexmap( pTexMap ) )
        {
            mapFilenames.push_back( "DiffuseMap:" + BitmapTexFileName( pTexMap ) );
        }
        else if ( std::string( m_pIStdMat->GetSubTexmapSlotName(materialMapId) ) == "Bump" )
		{
            if ( bitmapTexTexmap( pTexMap ) )
            {
                mapFilenames.push_back( "BumpMap:" + BitmapTexFileName( pTexMap, "bump" ) );
                mapFilenames.push_back( "BumpMapAmount:" + floatToString(
                                            m_pIStdMat->GetTexmapAmt(materialMapId, 0) ) );
            }
            else if ( normalBumpTexmap( pTexMap ) )
            {
                if ( pTexMap->GetSubTexmap( 0 ) ) // normal map
                {
                    mapFilenames.push_back( "NormalsMap:" +
                                            BitmapTexFileName( pTexMap->GetSubTexmap(0) ) );
//                    mapFilenames.push_back( "NormalsMapAmount:" + floatToString(
//                                                ((StdMat*) pTexMap)->GetTexmapAmt(0, 0) ) );
					// ^ how to get amount from normal bump texmap? Texmap is not a StdMat
                }
                
                if ( pTexMap->GetSubTexmap( 1 ) ) // additional bump
                {
                    mapFilenames.push_back( "BumpMap:" +
                                            BitmapTexFileName( pTexMap->GetSubTexmap(1) ) );
                    //mapFilenames.push_back( "BumpMapAmount:" + floatToString(
//                                                ((StdMat*) pTexMap)->GetTexmapAmt(1, 0) ) );
                }
                //return std::string( pTexMap->GetName() ); // Map #22
                //return std::string( pTexMap->GetFullName() ); // Map #22  ( Normal Bump ) 
                //return std::string( m_pIStdMat->GetSubTexmapSlotName(materialMapId) ); // Bump
                //return "<none>";
                // hmm, do we need "Bump:Normal Bump:asdasdf.tga" "Diffuse:asdfasd.jpg" ?
            }
		}
    }

	return true;
}

//----------------------------------------------------------------------------//
// Get the ambient color of the material                                      //
//----------------------------------------------------------------------------//

void CMaxMaterial::GetAmbientColor(float *pColor)
{
	// get color from the internal standard material
	Color color;
	color = m_pIStdMat->GetAmbient(0);

	pColor[0] = color.r;
	pColor[1] = color.g;
	pColor[2] = color.b;
	pColor[3] = 0.0f;
}

//----------------------------------------------------------------------------//
// Get the diffuse color of the material                                      //
//----------------------------------------------------------------------------//

void CMaxMaterial::GetDiffuseColor(float *pColor)
{
	// get color from the internal standard material
	Color color;
	color = m_pIStdMat->GetDiffuse(0);

	pColor[0] = color.r;
	pColor[1] = color.g;
	pColor[2] = color.b;
	pColor[3] = m_pIStdMat->GetOpacity(0);
}

//----------------------------------------------------------------------------//
// Get the number of maps of the material                                     //
//----------------------------------------------------------------------------//


int CMaxMaterial::GetMapCount()
{
    return mapFilenames.size();
}

//----------------------------------------------------------------------------//
// Get the filename of the bitmap of the given map                            //
//----------------------------------------------------------------------------//

std::string CMaxMaterial::GetMapFilename(int mapId)
{
    return mapFilenames[ mapId ];
}

//----------------------------------------------------------------------------//
// Get the name of the material                                               //
//----------------------------------------------------------------------------//

std::string CMaxMaterial::GetName()
{
	// check if the internal material is valid
	if(m_pIStdMat == 0) return "<void>";

	const char* name = m_pIStdMat->GetName();
	return name;
}

//----------------------------------------------------------------------------//
// Get the shininess factor of the material                                   //
//----------------------------------------------------------------------------//

float CMaxMaterial::GetShininess()
{
	return m_pIStdMat->GetShinStr(0);
}

//----------------------------------------------------------------------------//
// Get the specular color of the material                                     //
//----------------------------------------------------------------------------//

void CMaxMaterial::GetSpecularColor(float *pColor)
{
	// get color from the internal standard material
	Color color;
	color = m_pIStdMat->GetSpecular(0);

	pColor[0] = color.r;
	pColor[1] = color.g;
	pColor[2] = color.b;
	pColor[3] = 0.0f;
}

//----------------------------------------------------------------------------//
