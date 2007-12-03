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
#include <osgCal/Mesh>

using namespace osgCal;


Mesh::Mesh( ModelData*      _modelData,
            const CoreMesh* _mesh )
    : modelData( _modelData )
    , mesh( _mesh )
    , boundingBox( _mesh->data->boundingBox )
    , deformed( false )
    , depthMesh( 0 )
{   
    setName( mesh->data->name ); // for debug only, TODO: subject to remove    
    setDataVariance( mesh->data->rigid ? STATIC : DYNAMIC );
    // ^ No drawing during updates. Otherwise there will be a
    // crash in multithreaded osgCalViewer modes
    // (first reported by Jan Ciger)
}

void
Mesh::changeDisplaySettings( const MeshDisplaySettings* newDs )
{    
    const MeshDisplaySettings* prevDs = mesh->displaySettings.get();

    if ( prevDs->software != newDs->software )
    {
        throw std::runtime_error( "Mesh::changeDisplaySettings: software/hardware switching is not supported" );
    }
    
    mesh = new CoreMesh( modelData->getCoreModel(),
                         mesh.get(),
                         mesh->material.get(),
                         newDs );
    onDisplaySettingsChanged( prevDs );
}

void
Mesh::onDisplaySettingsChanged( const MeshDisplaySettings* previousDs )
{
    (void)previousDs;
}
