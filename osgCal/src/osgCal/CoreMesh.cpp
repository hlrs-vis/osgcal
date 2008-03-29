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
#include <osgCal/CoreMesh>
#include <osgCal/CoreModel>

using namespace osgCal;

CoreMesh::CoreMesh( const CoreModel* model,
                    MeshData*        _data,
                    const Material*  _material,
                    const MeshParameters* _p )
    : data( _data )
    , material( const_cast< Material* >( _material ) )
    , parameters( const_cast< MeshParameters* >( _p ) )
    , displayLists( new MeshDisplayLists )
    , stateSets( new MeshStateSets( model->getStateSetCache(),
                                    _data,
                                    _material,
                                    _p ) )
{
}

CoreMesh::CoreMesh( const CoreModel* model,
                    const CoreMesh*  mesh,
                    const Material*  newMaterial,
                    const MeshParameters* newP )
    : data( mesh->data.get() )
    , material( const_cast< Material* >( newMaterial ) )
    , parameters( const_cast< MeshParameters* >( newP ) )
    , displayLists( mesh->displayLists.get() )
    , stateSets( new MeshStateSets( model->getStateSetCache(),
                                    mesh->data.get(),
                                    newMaterial,
                                    newP ) )
{
}

void
CoreMesh::releaseGLObjects( osg::State* state ) const
{
    displayLists->releaseGLObjects( state );
    stateSets->releaseGLObjects( state );    
}
