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
#include <osgCal/DepthMesh>
#include <osgCal/HardwareMesh>

using namespace osgCal;

DepthMesh::DepthMesh( HardwareMesh* hw )
    : hwMesh( hw )
{
    //setThreadSafeRefUnref( true );

    setUseDisplayList( false );
    setSupportsDisplayList( false );
    setUseVertexBufferObjects( false ); // false is default
    if ( hwMesh->getCoreMesh()->data->rigid )
    {
        setStateSet( hwMesh->getCoreMesh()->stateSets->staticDepthOnly.get() );
    }
    else
    {
        setStateSet( hwMesh->getCoreMesh()->stateSets->depthOnly.get() );        
    }
    dirtyBound();

    setUserData( const_cast< MeshParameters* >
                 ( MeshParameters::defaults() ) /*any referenced*/ );
    // ^ make this node not redundant and not suitable for merging for osgUtil::Optimizer
}

void
DepthMesh::drawImplementation(osg::RenderInfo& renderInfo) const
{
    hwMesh->drawImplementation( renderInfo, getStateSet() );
}

void
DepthMesh::compileGLObjects(osg::RenderInfo& renderInfo) const
{
    hwMesh->compileGLObjects( renderInfo );
}

void
DepthMesh::update( bool /*deformed*/, bool changed )
{
//     if ( deformed )
//     {
//         setStateSet( hwMesh->getCoreMesh()->stateSets->depthOnly.get() );
//     }
//     else
//     {
//         setStateSet( hwMesh->getCoreMesh()->stateSets->staticDepthOnly.get() );
//     }

    if ( changed )
    {
        dirtyBound();
    }
}

osg::BoundingBox
DepthMesh::computeBoundingBox() const
{
    return hwMesh->computeBoundingBox();
}
