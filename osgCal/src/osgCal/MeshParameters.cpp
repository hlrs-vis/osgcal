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
#include <osgCal/MeshParameters>

using namespace osgCal;

MeshParameters::MeshParameters()
    : software( false )
    , showTBN( false )
    , fogMode( (osg::Fog::Mode)0 )
    , useDepthFirstMesh( false )
    , noSoftwareVertexUpdate( false )
{
}

static osg::ref_ptr< MeshParameters > defaultMeshParameters( new MeshParameters );

const MeshParameters*
MeshParameters::defaults()
{
    return defaultMeshParameters.get();
}

MeshParameters* 
DefaultMeshParametersSelector::getParameters( const MeshData* )
{
    return defaultMeshParameters.get();
}

static osg::ref_ptr< DefaultMeshParametersSelector >
defaultMeshParametersSelector( new DefaultMeshParametersSelector );

DefaultMeshParametersSelector*
DefaultMeshParametersSelector::instance()
{
    return defaultMeshParametersSelector.get();
}
