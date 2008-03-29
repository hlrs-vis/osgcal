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

#include <osgCal/MeshDisplayLists>

using namespace osgCal;


MeshDisplayLists::~MeshDisplayLists()
{
    releaseGLObjects( 0 );
}

void
MeshDisplayLists::checkAllDisplayListsCompiled( MeshData* data ) const
{
    size_t numOfContexts = osg::DisplaySettings::instance()->getMaxNumberOfGraphicsContexts();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock( mutex ); 
    
    if ( lists.size() != numOfContexts )
    {
        return;
    }
    
    for ( size_t i = 0; i < numOfContexts; i++ )
    {
        if ( lists[ i ] == 0 )
        {
            return;
        }
    }

    // -- Free buffers that are no more needed --
    data->normalBuffer = 0;
    data->texCoordBuffer = 0;
    data->tangentAndHandednessBuffer = 0;
}

void
MeshDisplayLists::releaseGLObjects( osg::State* state ) const
{
    OpenThreads::ScopedLock< OpenThreads::Mutex > lock( mutex ); 

    if ( state )
    {
        size_t  id = state->getContextID();
        GLuint& dl = lists[ id ];

        if ( dl != 0 )
        {
            osg::Drawable::deleteDisplayList( id, dl, 0/*getGLObjectSizeHint()*/ );
            dl = 0;
        }
    }
    else
    {
        for( size_t id = 0; id < lists.size(); id++ )
        {
            GLuint& dl = lists[ id ];

            if ( dl != 0 )
            {
                osg::Drawable::deleteDisplayList( id, dl, 0/*getGLObjectSizeHint()*/ );
                dl = 0;
            }
        }            
    }
}
