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

SubMeshSoftware::SubMeshSoftware( Model*     _model,
                                  int        meshIndex,
                                  bool       _meshIsStatic )
    : coreModel( _model->getCoreModel() )
    , model( _model )
    , calModel( _model->getCalModel() )
    , mesh( const_cast< CoreModel::Mesh* >( &_model->getCoreModel()->getMeshes()[ meshIndex ] ) )
    , meshIsStatic( _meshIsStatic )
{
    if ( mesh->maxBonesInfluence == 0 || meshIsStatic )
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
    setStateSet( mesh->stateSet.get() );

    create();
}

SubMeshSoftware::SubMeshSoftware(const SubMeshSoftware& submesh, const osg::CopyOp& copyop)
{
    throw std::runtime_error( "SubMeshSoftware copying is not supported" );
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
    setVertexArray( model->getVertexBuffer() );
    setNormalArray( model->getNormalBuffer() );
    setNormalBinding( osg::Geometry::BIND_PER_VERTEX );
    getNormalData().normalize = GL_TRUE;
    setTexCoordArray( 0, const_cast< TexCoordBuffer* >( coreModel->getTexCoordBuffer() ) );

    osg::DrawElementsUInt* de = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES,
                                                           mesh->getIndexesCount() );
    memcpy( &de->front(),
            (GLuint*)&coreModel->getIndexBuffer()->front()
            + mesh->getIndexInVbo(),
            mesh->getIndexesCount() * sizeof ( GLuint ) );
    addPrimitiveSet( de );

// draw arrays are MORE slow than draw elements (at least twice)
//     setVertexIndices( const_cast< IndexBuffer* >( coreModel->getIndexBuffer() ) );
//     setNormalIndices( const_cast< IndexBuffer* >( coreModel->getIndexBuffer() ) );
//     setTexCoordIndices( 0, const_cast< IndexBuffer* >( coreModel->getIndexBuffer() ) );
//     addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::TRIANGLES,
//                                           mesh->getIndexInVbo(),
//                                           mesh->getIndexesCount() ) );
    
    boundingBox = mesh->boundingBox;
}

namespace osg {
bool
operator == ( const osg::Matrix3& m1,
              const osg::Matrix3& m2 ); // defined in SubMeshHardware.cpp
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

void
SubMeshSoftware::update()
{
    if ( mesh->maxBonesInfluence == 0 || meshIsStatic )
    {
        return; // no bones - no update
    }
    // hmm. is it good to copy/paste? its nearly the same algorithm
    
    // -- Setup rotation matrices & translation vertices --
    CalHardwareModel* hardwareModel = coreModel->getCalHardwareModel();
    
    hardwareModel->selectHardwareMesh( mesh->hardwareMeshId );

    std::vector< std::pair< osg::Matrix3, osg::Vec3f > > rotationTranslationMatrices;

    for( int boneId = 0; boneId < hardwareModel->getBoneCount(); boneId++ )
    {
        CalQuaternion   rotationBoneSpace =
            hardwareModel->getRotationBoneSpace( boneId, calModel->getSkeleton() );
        CalVector       translationBoneSpace =
            hardwareModel->getTranslationBoneSpace( boneId, calModel->getSkeleton() );

        CalMatrix       rotationMatrix = rotationBoneSpace;
        GLfloat         rotation[9];
        GLfloat         translation[3];

        rotation[0] = rotationMatrix.dxdx;
        rotation[1] = rotationMatrix.dxdy;
        rotation[2] = rotationMatrix.dxdz;
        rotation[3] = rotationMatrix.dydx;
        rotation[4] = rotationMatrix.dydy;
        rotation[5] = rotationMatrix.dydz;
        rotation[6] = rotationMatrix.dzdx;
        rotation[7] = rotationMatrix.dzdy;
        rotation[8] = rotationMatrix.dzdz;

        translation[0] = translationBoneSpace.x;
        translation[1] = translationBoneSpace.y;
        translation[2] = translationBoneSpace.z;

        osg::Matrix3 r( rotation[0], rotation[3], rotation[6],
                        rotation[1], rotation[4], rotation[7], 
                        rotation[2], rotation[5], rotation[8] );
//         osg::Matrixf r( rotation[0], rotation[3], rotation[6], 0,
//                         rotation[1], rotation[4], rotation[7], 0, 
//                         rotation[2], rotation[5], rotation[8], 0,
//                         0          , 0          , 0          , 1 );
        osg::Vec3 v( translation[0],
                     translation[1],
                     translation[2] );
        
        rotationTranslationMatrices.push_back( std::make_pair( r, v ) );
    }

    rotationTranslationMatrices.resize( 31 );
    rotationTranslationMatrices[ 30 ] = // last always identity (see #68)
        std::make_pair( osg::Matrix3( 1, 0, 0,
                                      0, 1, 0,
                                      0, 0, 1 ),
                        osg::Vec3( 0, 0, 0 ) );
    
    // -- Check changes --
    if ( rotationTranslationMatrices == previousRotationTranslationMatrices )
    {
        return; // no changes
    }
    else
    {
        previousRotationTranslationMatrices = rotationTranslationMatrices;
    }

    // -- Scan indexes --
    boundingBox = osg::BoundingBox();
    
    const GLuint* beginIndex = &coreModel->getIndexBuffer()->front() + mesh->getIndexInVbo();
    const GLuint* endIndex   = beginIndex + mesh->getIndexesCount();

    VertexBuffer&               vb  = *model->getVertexBuffer();
    NormalBuffer&               nb  = *model->getNormalBuffer();
    Model::UpdateFlagBuffer&    ufb = *model->getUpdateFlagBuffer();
    const VertexBuffer&         svb = *coreModel->getVertexBuffer();
    const NormalBuffer&         snb = *coreModel->getNormalBuffer();
    const WeightBuffer&         wb  = *coreModel->getWeightBuffer();
    const MatrixIndexBuffer&    mib = *coreModel->getMatrixIndexBuffer();    

#define ITERATE( _f )                                               \
    for ( const GLuint* idx = beginIndex; idx < endIndex; ++idx )   \
    {                                                               \
        if ( ufb[ *idx ] )                                          \
        {                                                           \
            continue; /* skip already processed vertices */         \
        }                                                           \
        else                                                        \
        {                                                           \
            ufb[ *idx ] = GL_TRUE;                                  \
        }                                                           \
                                                                    \
        osg::Vec3f&        v  = vb [ *idx ];                        \
        osg::Vec3f&        n  = nb [ *idx ];                        \
        const osg::Vec3f&  sv = svb[ *idx ]; /* source vector */    \
        const osg::Vec3f&  sn = snb[ *idx ]; /* source normal */    \
        const osg::Vec4f&  w  = wb [ *idx ]; /* weights */          \
        const osg::Vec4s&  mi = mib[ *idx ]; /* bone indexes */     \
                                                                    \
        _f;                                                         \
                                                                    \
        boundingBox.expandBy( v );                                  \
    }

#define PROCESS_X( _process_y )                                                 \
    if ( w.x() )                                                                \
    {                                                                           \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi.x()].first;     \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi.x()].second;    \
        v = (mul3(rm, sv) + tv) * w.x();                                        \
        n = (mul3(rm, sn)) * w.x();                                             \
                                                                                \
        _process_y;                                                             \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        v = sv;                                                                 \
    }

#define PROCESS_Y( _process_z )                                                 \
    if ( w.y() )                                                                \
    {                                                                           \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi.y()].first;     \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi.y()].second;    \
        v += (mul3(rm, sv) + tv) * w.y();                                       \
        n += (mul3(rm, sn)) * w.y();                                            \
                                                                                \
        _process_z;                                                             \
    }                                                                           \

#define PROCESS_Z( _process_w )                                                 \
    if ( w.z() )                                                                \
    {                                                                           \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi.z()].first;     \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi.z()].second;    \
        v += (mul3(rm, sv) + tv) * w.z();                                       \
        n += (mul3(rm, sn)) * w.z();                                            \
                                                                                \
        _process_w;                                                             \
    }                                                                           \

#define PROCESS_W()                                                             \
    if ( w.w() )                                                                \
    {                                                                           \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi.w()].first;     \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi.w()].second;    \
        v += (mul3(rm, sv) + tv) * w.w();                                       \
        n += (mul3(rm, sn)) * w.w();                                            \
    }                                                                           \

#define STOP

    switch ( mesh->maxBonesInfluence )
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
