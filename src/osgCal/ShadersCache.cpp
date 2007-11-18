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
#include <osg/Notify>
#include <osg/observer_ptr>

#include <osgCal/ShadersCache>

using namespace osgCal;

int
osgCal::materialShaderFlags( const Material& material )
{
    int flags = 0;

    if ( !material.diffuseMap.empty() )
    {
        flags |= SHADER_FLAG_TEXTURING;
    }
    if ( !material.normalsMap.empty() )
    {
        flags |= SHADER_FLAG_NORMAL_MAPPING;
    }
    if ( !material.bumpMap.empty() )
    {
        flags |= SHADER_FLAG_BUMP_MAPPING;
    }
    if ( material.sides == 2 )
    {
        flags |= SHADER_FLAG_TWO_SIDED;
    }

    if (    material.specularColor.r() != 0
            || material.specularColor.g() != 0
            || material.specularColor.b() != 0 )
    {
        flags |= SHADER_FLAG_SHINING;
    }

    if ( material.diffuseColor.a() < 1 )
    {
        flags |= SHADER_FLAG_OPACITY;
        flags |= SHADER_FLAG_TWO_SIDED;
    }

    return flags;
}


ShadersCache::~ShadersCache()
{
    osg::notify( osg::DEBUG_FP ) << "destroying ShadersCache... " << std::endl;
    vertexShaders.clear();
    fragmentShaders.clear();
    programs.clear();
    osg::notify( osg::DEBUG_FP ) << "ShadersCache destroyed" << std::endl;
}

osg::Program*
ShadersCache::get( int flags )
{
//     if ( flags &
//          (SHADER_FLAG_BONES(1) | SHADER_FLAG_BONES(2)
//           | SHADER_FLAG_BONES(3) | SHADER_FLAG_BONES(4)) )
//     {
//         flags &= ~SHADER_FLAG_BONES(0)
//             & ~SHADER_FLAG_BONES(1) & ~SHADER_FLAG_BONES(2)
//             & ~SHADER_FLAG_BONES(3) & ~SHADER_FLAG_BONES(4);
//         flags |= SHADER_FLAG_BONES(4);
//     }
//     BTW, not so much difference between always 4 bone and per-bones count shaders

    if ( flags & SHADER_FLAG_DEPTH_ONLY )
    {
        flags &= DEPTH_ONLY_MASK; 
    }

    ProgramsMap::const_iterator pmi = programs.find( flags );

    if ( pmi != programs.end() )
    {
        return pmi->second.get();
    }
    else
    {
#define PARSE_FLAGS                                                     \
        int BONES_COUNT = flags / SHADER_FLAG_BONES(1);                 \
        int RGBA = ( SHADER_FLAG_RGBA & flags ) ? 1 : 0;                \
        int FOG_MODE = ( SHADER_FLAG_FOG_MODE_MASK & flags );           \
        int FOG = FOG_MODE != 0;                                        \
        int OPACITY = ( SHADER_FLAG_OPACITY & flags ) ? 1 : 0;          \
        int TEXTURING = ( SHADER_FLAG_TEXTURING & flags ) ? 1 : 0;      \
        int NORMAL_MAPPING = ( SHADER_FLAG_NORMAL_MAPPING & flags ) ? 1 : 0; \
        int BUMP_MAPPING = ( SHADER_FLAG_BUMP_MAPPING & flags ) ? 1 : 0; \
        int SHINING = ( SHADER_FLAG_SHINING & flags ) ? 1 : 0;          \
        int DEPTH_ONLY = ( SHADER_FLAG_DEPTH_ONLY & flags ) ? 1 : 0;    \
        int TWO_SIDED = ( SHADER_FLAG_TWO_SIDED & flags ) ? 1 : 0
        
        PARSE_FLAGS;
        (void)FOG; // remove unused variable warning
                
        osg::Program* p = new osg::Program;

        char name[ 256 ];
        sprintf( name, "skeletal shader (%d bones%s%s%s%s%s%s%s%s%s)",
                 BONES_COUNT,
                 DEPTH_ONLY ? ", depth_only" : "",
                 (FOG_MODE == SHADER_FLAG_FOG_MODE_EXP ? ", fog_exp"
                  : (FOG_MODE == SHADER_FLAG_FOG_MODE_EXP2 ? ", fog_exp2"
                     : (FOG_MODE == SHADER_FLAG_FOG_MODE_LINEAR ? ", fog_linear" : ""))),
                 RGBA ? ", rgba" : "",
                 OPACITY ? ", opacity" : "",
                 TEXTURING ? ", texturing" : "",
                 NORMAL_MAPPING ? ", normal mapping" : "",
                 BUMP_MAPPING ? ", bump mapping" : "",
                 SHINING ? ", shining" : "",
                 TWO_SIDED ? ", two-sided" : ""
            );

        //p->setThreadSafeRefUnref( true );
        p->setName( name );

        p->addShader( getVertexShader( flags ) );
        p->addShader( getFragmentShader( flags ) );

        //p->addBindAttribLocation( "position", 0 );
        // Attribute location binding is needed for ATI.
        // ATI will draw nothing until one of the attributes
        // will bound to zero location (BTW, this behaviour
        // described in OpenGL spec. don't know why on nVidia
        // it works w/o binding).

        programs[ flags ] = p;
        return p;
    }            
}
        

osg::Shader*
ShadersCache::getVertexShader( int flags )
{           
    flags &= ~SHADER_FLAG_RGBA
        & ~SHADER_FLAG_OPACITY
//        & ~SHADER_FLAG_TWO_SIDED
        & ~SHADER_FLAG_SHINING;
    // remove irrelevant flags that can lead to
    // duplicate shaders in map
    if ( flags & SHADER_FLAG_FOG_MODE_MASK )
    {
        flags |= SHADER_FLAG_FOG_MODE_MASK;
        // ^ vertex shader only need to know that FOG is needed
        // fog mode is irrelevant
    }

    ShadersMap::const_iterator smi = vertexShaders.find( flags );

    if ( smi != vertexShaders.end() )
    {
        return smi->second.get();
    }
    else
    {                
        PARSE_FLAGS;
        (void)RGBA, (void)OPACITY, (void)SHINING, (void)FOG_MODE;
        // remove unused variable warning

        std::string shaderText;

        if ( DEPTH_ONLY )
        {
            #include "shaders/SkeletalDepthOnly_vert.h"
        }
        else
        {                    
            #include "shaders/Skeletal_vert.h"
        }

        osg::Shader* vs = new osg::Shader( osg::Shader::VERTEX,
                                           shaderText.data() );
        //vs->setThreadSafeRefUnref( true );
        vertexShaders[ flags ] = vs;
        return vs;
    }
}

osg::Shader*
ShadersCache::getFragmentShader( int flags )
{
    flags &= ~SHADER_FLAG_BONES(0)
        & ~SHADER_FLAG_BONES(1) & ~SHADER_FLAG_BONES(2)
        & ~SHADER_FLAG_BONES(3) & ~SHADER_FLAG_BONES(4);
    // remove irrelevant flags that can lead to
    // duplicate shaders in map  

    ShadersMap::const_iterator smi = fragmentShaders.find( flags );

    if ( smi != fragmentShaders.end() )
    {
        return smi->second.get();
    }
    else
    {                
        PARSE_FLAGS;
        (void)BONES_COUNT; // remove unused variable warning

        std::string shaderText;

        if ( DEPTH_ONLY )
        {
            #include "shaders/SkeletalDepthOnly_frag.h"
        }
        else
        {                    
            #include "shaders/Skeletal_frag.h"
        }

        osg::Shader* fs = new osg::Shader( osg::Shader::FRAGMENT,
                                           shaderText.data() );
        //fs->setThreadSafeRefUnref( true );
        fragmentShaders[ flags ] = fs;
        return fs;
    }
}


/**
 * Global instance of ShadersCache.
 * There is only one shader instance which is created with first CoreModel
 * and destroyed after last CoreModel destroyed (and hence all Models destroyed also
 * since they are referring to CoreModel).
 */
static osg::observer_ptr< ShadersCache >  shadersCache;

ShadersCache*
ShadersCache::instance()
{
    if ( !shadersCache.valid() )
    {
        shadersCache = new ShadersCache;
    }

    return shadersCache.get();
}
