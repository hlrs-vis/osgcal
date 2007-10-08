//----------------------------------------------------------------------------//
// MaxMaterial.h                                                              //
// Copyright (C) 2001, 2002 Bruno 'Beosil' Heidelberger                       //
//----------------------------------------------------------------------------//
// This program is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU General Public License as published by the Free //
// Software Foundation; either version 2 of the License, or (at your option)  //
// any later version.                                                         //
//----------------------------------------------------------------------------//

#ifndef MAX_MATERIAL_H
#define MAX_MATERIAL_H

//----------------------------------------------------------------------------//
// Includes                                                                   //
//----------------------------------------------------------------------------//

#include "BaseMaterial.h"
#include <string>
#include <vector>

//----------------------------------------------------------------------------//
// Class declaration                                                          //
//----------------------------------------------------------------------------//

class CMaxMaterial : public CBaseMaterial
{
// member variables
protected:
	StdMat *m_pIStdMat;

// constructors/destructor
public:
	CMaxMaterial();
	virtual ~CMaxMaterial();

// member functions
public:
	virtual bool Create(StdMat *pIStdMat);
	virtual void GetAmbientColor(float *pColor);
	virtual void GetDiffuseColor(float *pColor);
	virtual int GetMapCount();
	virtual std::string GetMapFilename(int mapId);
	virtual std::string GetName();
	virtual void GetSpecularColor(float *pColor);
	virtual float GetShininess();

private:
    /**
     * Since the only way to pass material parameters to cal3d is
     * through string map name.
     *
     * So additional information is also here:
     * {"Opacity:100",
     *  "Sides:2", // for 2-sided
     *  "DiffuseMap:<filename>",
     *  "NormalsMap:<filename>",
     *  "BumpMap:<filename>",
     *  "BumpAmount:NN"
     * }
     */
    std::vector< std::string > mapFilenames;
};

#endif

//----------------------------------------------------------------------------//
