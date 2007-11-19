/* -*- c++ -*-
    Copyright (C) 2003 <ryu@gpul.org>
    Copyright (C) 2006 Vladimir Shabanov <vshabanoff@gmail.com>

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
#include <cal3d/cal3d.h>

#include <osg/Notify>
#include <osgCal/SubMeshSoftware>

#include <iostream>

using namespace osgCal;

SubMeshSoftware::SubMeshSoftware( CoreModel*             _coreModel,
                                  ModelData*             _modelData,
                                  const CoreModel::Mesh* _mesh )
    : coreModel( _coreModel )
    , modelData( _modelData )
    , mesh( _mesh )
{
    //setThreadSafeRefUnref( true );

    if ( mesh->data->rigid )
    {
        setUseDisplayList( true );
        setSupportsDisplayList( true ); 
    }
    else
    {
        setUseDisplayList( false );
        setSupportsDisplayList( false );
    }

    setUseVertexBufferObjects( false ); // false is default
    setStateSet( mesh->stateSets->software.get() );

    create();

    setUserData( getStateSet() /*any referenced*/ );
    // ^ make this node not redundant and not suitable for merging for osgUtil::Optimizer
}

osg::Object*
SubMeshSoftware::cloneType() const
{
    throw std::runtime_error( "cloneType() is not implemented" );
}

osg::Object*
SubMeshSoftware::clone( const osg::CopyOp& ) const
{
    throw std::runtime_error( "clone() is not implemented" );
}

SubMeshSoftware::~SubMeshSoftware()
{
}

void
SubMeshSoftware::create()
{
    if ( !mesh->data->normalBuffer.valid() )
    {
        throw std::runtime_error( "no normal buffer exists for software mesh, "
                                  "seems that you've used hardware meshes before "
                                  "(which free unneded buffers after display list created), "
                                  "never use software and hardware meshes for single model, "
                                  "software meshes are for testing purpouses only" );
    }
    
    if ( mesh->data->rigid )
    {
        setVertexArray( mesh->data->vertexBuffer.get() );
        setNormalArray( mesh->data->normalBuffer.get() );
    }
    else
    {
        setVertexArray( (VertexBuffer*)mesh->data->vertexBuffer->clone( osg::CopyOp::DEEP_COPY_ALL ) );
        setNormalArray( (NormalBuffer*)mesh->data->normalBuffer->clone( osg::CopyOp::DEEP_COPY_ALL ) );
        getNormalData().normalize = GL_TRUE;
    }
    setNormalBinding( osg::Geometry::BIND_PER_VERTEX );
    setTexCoordArray( 0, const_cast< TexCoordBuffer* >( mesh->data->texCoordBuffer.get() ) );

    addPrimitiveSet( mesh->data->indexBuffer.get() ); // DrawElementsUInt

    boundingBox = mesh->data->boundingBox;
}

static
inline
osg::Vec3f
convert( const osg::Vec3f& v )
{
    return v;
}

static
inline
osg::Vec3f
convert( const osg::Vec3b& v )
{
    return osg::Vec3f( v.x() / 127.0, v.y() / 127.0, v.z() / 127.0 );
}

static
inline
osg::Vec3f
mul3( const osg::Matrix3& m,
      const osg::Vec3f& v )
{
    return osg::Vec3f( m(0,0)*v.x() + m(1,0)*v.y() + m(2,0)*v.z(),
                       m(0,1)*v.x() + m(1,1)*v.y() + m(2,1)*v.z(),
                       m(0,2)*v.x() + m(1,2)*v.y() + m(2,2)*v.z() );
}

inline
osg::Vec3f
mul3( const osg::Matrix3& m,
      const osg::Vec3b& v )
{
    return mul3( m, convert( v ) );
}

void
SubMeshSoftware::update()
{
    // hmm. is it good to copy/paste? its nearly the same algorithm
    
    // -- Setup rotation matrices & translation vertices --
    typedef std::pair< osg::Matrix3, osg::Vec3f > RTPair;
    float rotationTranslationMatricesData[ 31 * sizeof (RTPair) / sizeof ( float ) ];
    // we make data to not init matrices & vertex since we always set
    // them to correct data
    RTPair* rotationTranslationMatrices = (RTPair*)(void*)&rotationTranslationMatricesData;

    bool changed = false;

    for( int boneIndex = 0; boneIndex < mesh->data->getBonesCount(); boneIndex++ )
    {
        int boneId = mesh->data->getBoneId( boneIndex );
        const ModelData::BoneParams& bp = modelData->getBoneParams( boneId );

        changed  |= bp.changed;

        RTPair& rt = rotationTranslationMatrices[ boneIndex ];

        rt.first  = bp.rotation;
        rt.second = bp.translation;
    }
   
    // -- Check changes --
    if ( !changed )
    {
        return; // no changes
    }

    rotationTranslationMatrices[ 30 ] = // last always identity (see #68)
        std::make_pair( osg::Matrix3( 1, 0, 0,
                                      0, 1, 0,
                                      0, 0, 1 ),
                        osg::Vec3( 0, 0, 0 ) );

    // -- Scan indexes --
    boundingBox = osg::BoundingBox();
    
    VertexBuffer&               vb  = *(VertexBuffer*)getVertexArray();
    NormalBuffer&               nb  = *(NormalBuffer*)getNormalArray();
    const VertexBuffer&         svb = *mesh->data->vertexBuffer.get();
    const NormalBuffer&         snb = *mesh->data->normalBuffer.get();
    const WeightBuffer&         wb  = *mesh->data->weightBuffer.get();
    const MatrixIndexBuffer&    mib = *mesh->data->matrixIndexBuffer.get();

    osg::Vec3f*        v  = &vb.front()  ; /* dest vector */   
    osg::Vec3f*        n  = &nb.front()  ; /* dest normal */   
    const osg::Vec3f*  sv = &svb.front() ; /* source vector */   
    const NormalBuffer::value_type*
                       sn = &snb.front() ; /* source normal */   
    const osg::Vec4f*  w  = &wb.front()  ; /* weights */         
    const MatrixIndexBuffer::value_type*
                       mi = &mib.front() ; /* bone indexes */		

    osg::Vec3f*        vEnd = v + vb.size(); /* dest vector end */   
    
#define ITERATE( _f )                           \
    while ( v < vEnd )                          \
    {                                           \
        _f;                                     \
                                                \
        boundingBox.expandBy( *v );             \
        ++v;                                    \
        ++n;                                    \
        ++sv;                                   \
        ++sn;                                   \
        ++w;                                    \
        ++mi;                                   \
    }

    #define x() r()
    #define y() g()
    #define z() b()
    #define w() a()

    // 'if's get ~15% speedup here
    
#define PROCESS_X( _process_y )                                         \
    if ( mi->x() != 30 )                                                \
        /* we have no zero weight vertices they all bound to 30th bone */ \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->x()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->x()].second; \
        *v = (mul3(rm, *sv) + tv) * w->x();                             \
        *n = (mul3(rm, *sn)) * w->x();                                  \
                                                                        \
        _process_y;                                                     \
    }                                                                   \
    else                                                                \
    {                                                                   \
        *v = *sv;                                                       \
        *n = convert( *sn );                                            \
    }                                                                   
                                                                        
#define PROCESS_Y( _process_z )                                         \
    if ( w->y() )                                                   \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->y()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->y()].second; \
        *v += (mul3(rm, *sv) + tv) * w->y();                            \
        *n += (mul3(rm, *sn)) * w->y();                                 \
                                                                        \
        _process_z;                                                     \
    }                                                                   

#define PROCESS_Z( _process_w )                                         \
    if ( w->z() )                                                       \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->z()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->z()].second; \
        *v += (mul3(rm, *sv) + tv) * w->z();                            \
        *n += (mul3(rm, *sn)) * w->z();                                 \
                                                                        \
        _process_w;                                                     \
    }

#define PROCESS_W()                                                     \
    if ( w->w() )                                                       \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->w()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->w()].second; \
        *v += (mul3(rm, *sv) + tv) * w->w();                            \
        *n += (mul3(rm, *sn)) * w->w();                                 \
    }

#define STOP

    switch ( mesh->data->maxBonesInfluence )
    {
        case 1:
            ITERATE( PROCESS_X( STOP ) );
            break;

        case 2:
            ITERATE( PROCESS_X( PROCESS_Y ( STOP ) ) );
            break;

        case 3:
            ITERATE( PROCESS_X( PROCESS_Y ( PROCESS_Z( STOP ) ) ) );
            break;

        case 4:
            ITERATE( PROCESS_X( PROCESS_Y ( PROCESS_Z( PROCESS_W() ) ) ) );
            break;

        default:
            throw std::runtime_error( "maxBonesInfluence > 4 ???" );            
    }

    dirtyBound();

    dirtyDisplayList();
}
