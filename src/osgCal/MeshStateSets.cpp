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
#include <osgCal/MeshStateSets>

using namespace osgCal;

MeshStateSets::MeshStateSets( StateSetCache*  c,
                              const MeshData* d,
                              const Material* m,
                              const MeshDisplaySettings* ds )
{
    Material* ncm = const_cast< Material* >( m ); // material const, refCount isn't
    MeshDisplaySettings* ncds = const_cast< MeshDisplaySettings* >( ds ); // same

    if ( ds->software )
    {
        stateSet = c->swMeshStateSetCache->get( static_cast< SoftwareMaterial* >( ncm ) );
    }
    else
    {
        staticStateSet = c->hwMeshStateSetCache->get( ncm, 0, ncds );
        if ( d->rigid == false )
        {
            stateSet = c->hwMeshStateSetCache->get( ncm, d->maxBonesInfluence, ncds );
        }

        if ( ds->useDepthFirstMesh )
        {
            staticDepthOnly = c->depthMeshStateSetCache->get( ncm, 0 );

            if ( d->rigid == false )
            {
                depthOnly = c->depthMeshStateSetCache->get( ncm, d->maxBonesInfluence );
            }
        }
    }
}
