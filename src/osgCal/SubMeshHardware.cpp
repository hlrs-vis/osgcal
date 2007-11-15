/* -*- c++ -*-
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
//#define GL_GLEXT_PROTOTYPES <- for glDrawRangeElements
#include <GL/gl.h>
#include <GL/glu.h>

#include <osg/VertexProgram>
#include <osg/GL2Extensions>
#include <osg/CullFace>

#include <osgCal/SubMeshHardware>

using namespace osgCal;



SubMeshHardware::SubMeshHardware( CoreModel*             _coreModel,
                                  ModelData*             _modelData,
                                  const CoreModel::Mesh* _mesh )
    : coreModel( _coreModel )
    , modelData( _modelData )
    , mesh( _mesh )
    , deformed( false )
{   
    setThreadSafeRefUnref( true );

    setUseDisplayList( false );
    setSupportsDisplayList( false );
    // ^ no display lists since we create them manually
    
    setUseVertexBufferObjects( false ); // false is default

    setStateSet( mesh->staticHardwareStateSet.get() );
    // Initially we use static (not skinning) state set. It will
    // changed to skinning (in update() method when some animation
    // starts.

    create();

// its too expensive to create different stateset for each submesh
// which (stateset) differs only in two uniforms
// its better to set them in drawImplementation
//     setStateSet( new osg::StateSet( *mesh->hardwareStateSet.get(), osg::CopyOp::SHALLOW_COPY ) );
//     getStateSet()->addUniform( new osg::Uniform( osg::Uniform::FLOAT_VEC3, "translationVectors",
//                                                  30 /*OSGCAL_MAX_BONES_PER_MESH*/ ) );
//     getStateSet()->addUniform( new osg::Uniform( osg::Uniform::FLOAT_MAT3, "rotationMatrices",
//                                                  30 /*OSGCAL_MAX_BONES_PER_MESH*/ ) );

    setUserData( getStateSet() /*any referenced*/ );
    // ^ make this node not redundant and not suitable for merging for osgUtil::Optimizer
    // (with merging & flattening turned on some color artefacts arise)
    // TODO: how to completely disable FLATTEN_STATIC_TRANSFORMS on models?
    // it copies vertex buffer (which is per model) for each submesh
    // which takes too much memory
}

SubMeshHardware::~SubMeshHardware()
{
    releaseGLObjects(); // destroy our display lists
}

osg::Object*
SubMeshHardware::cloneType() const
{
    throw std::runtime_error( "cloneType() is not implemented" );
}

osg::Object*
SubMeshHardware::clone( const osg::CopyOp& ) const
{
    throw std::runtime_error( "clone() is not implemented" );
}

void
SubMeshHardware::drawImplementation( osg::RenderInfo& renderInfo ) const
{
    drawImplementation( renderInfo, getStateSet() );
}

static
const osg::Program::PerContextProgram*
getProgram( osg::State& state,
            const osg::StateSet* stateSet )
{
    const osg::Program* stateProgram =
        static_cast< const osg::Program* >
        ( stateSet->getAttribute( osg::StateAttribute::PROGRAM ) );
//        ( state.getLastAppliedAttribute( osg::StateAttribute::PROGRAM ) ); <- don't work in display lists

    if ( stateProgram == 0 )
    {
        throw std::runtime_error( "SubMeshHardware::drawImplementation(): can't get program (shader compilation failed?" );
    }

    return stateProgram->getPCP( state.getContextID() );
}

void
SubMeshHardware::drawImplementation( osg::RenderInfo&     renderInfo,
                                     const osg::StateSet* stateSet ) const
{
    osg::State& state = *renderInfo.getState();

    // -- Setup rotation/translation uniforms --
    if ( deformed )
    {
        const osg::Program::PerContextProgram* program = getProgram( state, stateSet );
        const osg::GL2Extensions* gl2extensions = osg::GL2Extensions::Get( state.getContextID(), true );

        // -- Calculate and bind rotation/translation uniforms --
        GLint rotationMatricesAttrib = program->getUniformLocation( "rotationMatrices" );
        if ( rotationMatricesAttrib < 0 )
        {
            rotationMatricesAttrib = program->getUniformLocation( "rotationMatrices[0]" );
            // Why the hell on ATI it has uniforms for each
            // elements? (nVidia has only one uniform for the whole array)
        }
    
        GLint translationVectorsAttrib = program->getUniformLocation( "translationVectors" );
        if ( translationVectorsAttrib < 0 )
        {
            translationVectorsAttrib = program->getUniformLocation( "translationVectors[0]" );
        }

        if ( rotationMatricesAttrib < 0 && translationVectorsAttrib < 0 )
        {
            throw std::runtime_error( "no rotation/translation uniforms in deformed mesh?" );
        }

        GLfloat rotationMatrices[9*31];
        GLfloat translationVectors[3*31];
        

        for( int boneIndex = 0; boneIndex < mesh->data->getBonesCount(); boneIndex++ )
        {
            modelData->getBoneRotationTranslation( mesh->data->getBoneId( boneIndex ),
                                                   &rotationMatrices[boneIndex*9],
                                                   &translationVectors[boneIndex*3] );
        }
    
        gl2extensions->glUniformMatrix3fv( rotationMatricesAttrib,
                                           mesh->data->getBonesCount(), GL_TRUE, rotationMatrices );
        if ( translationVectorsAttrib >= 0 )
        {
            gl2extensions->glUniform3fv( translationVectorsAttrib,
                                         mesh->data->getBonesCount(), translationVectors );
        }

        GLfloat translation[3] = {0,0,0};
        GLfloat rotation[9] = {1,0,0, 0,1,0, 0,0,1};
        gl2extensions->glUniformMatrix3fv( rotationMatricesAttrib + 30, 1, GL_FALSE, rotation );
        if ( translationVectorsAttrib >= 0 )
        {
            gl2extensions->glUniform3fv( translationVectorsAttrib + 30, 1, translation );
        }
    }

    // -- Create display list if not yet exists --
    unsigned int contextID = renderInfo.getContextID();

    mesh->displayListsMutex.lock();
    GLuint& dl = mesh->displayLists[ contextID ];

    if( dl != 0 )
    {
        mesh->displayListsMutex.unlock();
    }
    else
    {
        dl = generateDisplayList( contextID, getGLObjectSizeHint() );

        innerDrawImplementation( renderInfo, dl );
        mesh->displayListsMutex.unlock();

        mesh->checkAllDisplayListsCompiled();
    }

    // -- Call display list --
    // get mesh material to restore glColor after glDrawElements call
    const osg::Material* material = static_cast< const osg::Material* >
        ( state.getLastAppliedAttribute( osg::StateAttribute::MATERIAL ) );

    bool transparent = stateSet->getRenderingHint() & osg::StateSet::TRANSPARENT_BIN;
    bool twoSided = transparent || mesh->hwStateDesc.sides == 2;

    if ( twoSided )
    {
        glEnable( GL_VERTEX_PROGRAM_TWO_SIDE_ARB );
    }

    if ( transparent )
    {
        glCullFace( GL_FRONT ); // first draw only back faces
        glCallList( dl );
        if ( material ) glColor4fv( material->getDiffuse( osg::Material::FRONT ).ptr() );
        glCullFace( GL_BACK ); // then draw only front faces
        glCallList( dl );
        if ( material ) glColor4fv( material->getDiffuse( osg::Material::FRONT ).ptr() );
    }
    else
    {
        glCallList( dl );        
        if ( material ) glColor4fv( material->getDiffuse( osg::Material::FRONT ).ptr() );
    }

    if ( twoSided )
    {
        glDisable( GL_VERTEX_PROGRAM_TWO_SIDE_ARB );
        // TODO: not good to enable/disable it many times
    }
}

void
SubMeshHardware::compileGLObjects(osg::RenderInfo& renderInfo) const
{
    Geometry::compileGLObjects( renderInfo );

    unsigned int contextID = renderInfo.getContextID();

    mesh->displayListsMutex.lock();
    
    GLuint& dl = mesh->displayLists[ contextID ];

    if( dl == 0 )
    {
        dl = generateDisplayList( contextID, getGLObjectSizeHint() );

        innerDrawImplementation( renderInfo, dl );
        mesh->displayListsMutex.unlock();

        mesh->checkAllDisplayListsCompiled();
    }
    else
    {        
        mesh->displayListsMutex.unlock();
    }
}

void
SubMeshHardware::innerDrawImplementation( osg::RenderInfo&     renderInfo,
                                          GLuint               displayList ) const
{   
#define glError()                                                       \
    {                                                                   \
        GLenum err = glGetError();                                      \
        while (err != GL_NO_ERROR) {                                    \
            fprintf(stderr, "glError: %s caught at %s:%u\n",            \
                    (char *)gluErrorString(err), __FILE__, __LINE__);   \
            err = glGetError();                                         \
        }                                                               \
    }

    osg::State& state = *renderInfo.getState();
    
    state.disableAllVertexArrays();

    // -- Setup vertex arrays --
#ifdef OSG_CAL_BYTE_BUFFERS
    #define NORMAL_TYPE         GL_BYTE
#else
    #define NORMAL_TYPE         GL_FLOAT
#endif
    state.setNormalPointer( NORMAL_TYPE, 0,
                            mesh->data->normalBuffer->getDataPointer() );

    if ( mesh->data->texCoordBuffer.valid() )
    {
        state.setTexCoordPointer( 0, 2, GL_FLOAT, 0,
                                  mesh->data->texCoordBuffer->getDataPointer() );
    }

    if ( mesh->data->tangentAndHandednessBuffer.valid() )
    {
//         state.setTexCoordPointer( 1, 4, GL_HALF_FLOAT_ARB/*GL_SHORT*//*NORMAL_TYPE*/, 0,
//                                   mesh->data->tangentAndHandednessBuffer->getDataPointer() );
        state.setTexCoordPointer( 1, 4, NORMAL_TYPE, 0,
                                  mesh->data->tangentAndHandednessBuffer->getDataPointer() );
    }
    
    GLushort* weightBuffer = NULL;

    if ( mesh->data->weightBuffer.valid() )
    {
//         weightBuffer = new GLushort[ mesh->data->weightBuffer->size() * 4 ];

//         GLushort* w = weightBuffer;
//         const GLfloat* wsrc = (const GLfloat*)mesh->data->weightBuffer->getDataPointer();
//         const GLfloat* wend = wsrc + mesh->data->weightBuffer->size() * 4;
//         while ( wsrc < wend )
//         {
//             *w++ = floatToHalf( *wsrc++ );
//         }
        
//         state.setTexCoordPointer( 2, mesh->data->maxBonesInfluence, GL_HALF_FLOAT_ARB, 4*2,
//                                   weightBuffer );
        state.setTexCoordPointer( 2, mesh->data->maxBonesInfluence, GL_FLOAT, 4*4,
                                  mesh->data->weightBuffer->getDataPointer() );
    }

    GLshort* matrixIndexBuffer = NULL;

    if ( mesh->data->matrixIndexBuffer.valid() )
    {
        matrixIndexBuffer = new GLshort[ mesh->data->matrixIndexBuffer->size() * 4 ];

        GLshort* mi = matrixIndexBuffer;
        const GLbyte* m   = (const GLbyte*)mesh->data->matrixIndexBuffer->getDataPointer();
        const GLbyte* end = m + mesh->data->matrixIndexBuffer->size() * 4;
        while ( m < end )
        {
            *mi++ = *m++;
        }
        
        state.setTexCoordPointer( 3, mesh->data->maxBonesInfluence, GL_SHORT, 4*2,
                                  matrixIndexBuffer );
//         state.setColorPointer( 4, GL_UNSIGNED_BYTE, 0,
//                                mesh->data->matrixIndexBuffer->getDataPointer() );
        // GL_UNSIGNED_BYTE only supported in ColorPointer not the TexCoord
        // but with color we need to multiply color by 255.0 in shader
        // and get slighlty less performance (~1%). So we create
        // GLshort data here. Hope the driver will convert it to bytes
        // when compiling display list.
    }

    state.setVertexPointer( 3, GL_FLOAT, 0,
                            mesh->data->vertexBuffer->getDataPointer() );

    // -- Draw our indexed triangles --
    if ( displayList != 0 )
        glNewList( displayList, GL_COMPILE );

    mesh->data->indexBuffer->draw( state, false );
//     // no visible speedup when using glDrawRangeElements
//     glDrawRangeElements(
//         GL_TRIANGLES,
//         0,
//         mesh->data->vertexBuffer->size(),
//         mesh->data->indexBuffer->size(),
//         GL_UNSIGNED_INT,                                                
//         (GLuint *)mesh->data->indexBuffer->getDataPointer() );          

    if ( displayList != 0 )
        glEndList();
    
    //glError();
    state.disableAllVertexArrays();

    delete[] matrixIndexBuffer;
    delete[] weightBuffer;
}

void
SubMeshHardware::create()
{
    if ( mesh->data->rigid )
    {
        setVertexArray( mesh->data->vertexBuffer.get() );
    }
    else
    {
        setVertexArray( (VertexBuffer*)mesh->data->vertexBuffer->clone( osg::CopyOp::DEEP_COPY_ALL ) );
    }

    addPrimitiveSet( mesh->data->indexBuffer.get() ); // DrawElementsUInt

    boundingBox = mesh->data->boundingBox;

    // create depth submesh for non-transparent meshes
    if ( (coreModel->getFlags() & CoreModel::USE_DEPTH_FIRST_MESHES)
         &&
         !(mesh->staticHardwareStateSet.get()->getRenderingHint()
           & osg::StateSet::TRANSPARENT_BIN) )
    {
        depthSubMesh = new SubMeshDepth( this ); 
    }
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
namespace osg {
bool
operator == ( const osg::Matrix3& m1,
              const osg::Matrix3& m2 )
{
    for ( int i = 0; i < 9; i++ )
    {
        if ( m1.ptr()[ i ] != m2.ptr()[ i ] )
        {
            return false; 
        }
    }
    
    return true;
}
}

inline
float
square( float x )
{
    return x*x;
}

float
rtDistance( const std::vector< std::pair< osg::Matrix3, osg::Vec3f > >& v1,
            const std::vector< std::pair< osg::Matrix3, osg::Vec3f > >& v2,
            int count )
{
    float r = 0;
    for ( int i = 0; i < count; i++ )
    {
        for ( int j = 0; j < 9; j++ )
        {
            r += square( v1[i].first[j] - v2[i].first[j] );
        }
        r += ( v1[i].second - v2[i].second ).length2();
    }
    return r;
}

void
SubMeshHardware::update()
{   
    if ( mesh->data->rigid )
    {
        return; // no bones - no update
    }
    
    // -- Setup rotation matrices & translation vertices --
    std::vector< std::pair< osg::Matrix3, osg::Vec3f > > rotationTranslationMatrices;

    deformed = false;

    for( int boneIndex = 0; boneIndex < mesh->data->getBonesCount(); boneIndex++ )
    {
        int boneId = mesh->data->getBoneId( boneIndex );

        rotationTranslationMatrices.push_back( modelData->getBoneRotationTranslation( boneId ) );
        osg::Vec3 translation = modelData->getBoneTranslation( boneId );
        osg::Quat rotation    = modelData->getBoneRotation( boneId );

        if ( // cal3d reports nonzero translations for non-animated models
            // and non zero quaternions (seems like some FP round-off error). 
            // So we must check for deformations using some epsilon value.
            // Problem:
            //   * It is cal3d that must return correct values, no epsilons
            // But nevertheless we use this to reduce CPU load.
                 
            translation.length() > boundingBox.radius() * 1e-5 // usually 1e-6 .. 1e-7
            ||
            osg::Vec3d( rotation.x(),
                        rotation.y(),
                        rotation.z() ).length() > 1e-6 // usually 1e-7 .. 1e-8
            )
        {
            deformed = true;
//             std::cout << "quaternion: "
//                       << rotation.x << ' '
//                       << rotation.y << ' '
//                       << rotation.z << ' '
//                       << rotation.w << std::endl
//                       << "translation: "
//                       << translation.x << ' ' << translation.y << ' ' << translation.z
//                       << "; len = " << translation.length() << std::endl
//                       << "len / bbox.radius = " << translation.length() / boundingBox.radius()
//                       << std::endl;
        }
    }

    rotationTranslationMatrices.resize( 31 );
    rotationTranslationMatrices[ 30 ] = // last always identity (see #68)
        std::make_pair( osg::Matrix3( 1, 0, 0,
                                      0, 1, 0,
                                      0, 0, 1 ),
                        osg::Vec3( 0, 0, 0 ) );

    // -- Check for deformation state and select state set type --
//    std::cout << "deformed = " << deformed << std::endl;
    if ( deformed )
    {
        setStateSet( mesh->hardwareStateSet.get() );
    }
    else
    {
        setStateSet( mesh->staticHardwareStateSet.get() );
        // for undeformed meshes we use static state set which not
        // perform vertex, normal, binormal and tangent deformations
        // in vertex shader
    }

    // -- Update depthSubMesh --
    if ( depthSubMesh.valid() )
    {
        depthSubMesh->update( deformed );
    }

    // -- Check changes --    
    previousRotationTranslationMatrices.resize( 31 );
//     std::cout << "distance = "
//               << rtDistance( rotationTranslationMatrices,
//                              previousRotationTranslationMatrices )
//               << std::endl;
//    if ( rotationTranslationMatrices == previousRotationTranslationMatrices )
    if ( rtDistance( rotationTranslationMatrices,
                     previousRotationTranslationMatrices,
                     mesh->data->getBonesCount() ) < 1e-7 ) // usually 1e-8..1e-10
    {
//        std::cout << "didn't changed" << std::endl;
        return; // no changes
    }
    else
    {
        previousRotationTranslationMatrices = rotationTranslationMatrices;
    }

    // -- Scan indexes --
    boundingBox = osg::BoundingBox();
    
    VertexBuffer&               vb  = *(VertexBuffer*)getVertexArray();
    const VertexBuffer&         svb = *mesh->data->vertexBuffer.get();
    const WeightBuffer&         wb  = *mesh->data->weightBuffer.get();
    const MatrixIndexBuffer&    mib = *mesh->data->matrixIndexBuffer.get();
   
    osg::Vec3f*        v  = &vb.front();      /* dest vector */   
    const osg::Vec3f*  sv = &svb.front();     /* source vector */   
    const osg::Vec4f*  w  = &wb.front();      /* weights */         
    const MatrixIndexBuffer::value_type*
                       mi = &mib.front();     /* bone indexes */
    osg::Vec3f*        vEnd = v + vb.size();  /* dest vector end */   
    
#define ITERATE( _f )                           \
    while ( v < vEnd )                          \
    {                                           \
        _f;                                     \
                                                \
        boundingBox.expandBy( *v );             \
        ++v;                                    \
        ++sv;                                   \
        ++w;                                    \
        ++mi;                                   \
    }

    #define x() r()
    #define y() g()
    #define z() b()
    #define w() a()
    
#define PROCESS_X( _process_y )                                         \
    /*if ( mi->x() != 30 )*/                                            \
        /* we have no zero weight vertices they all bound to 30th bone */ \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->x()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->x()].second; \
        *v = (mul3(rm, *sv) + tv) * w->x();                             \
                                                                        \
        _process_y;                                                     \
    }                                                                   
//     else
//     {
//         *v = *sv;
//     }

    // Strange, but multiplying each matrix on source vector works
    // faster than accumulating matrix and multiply at the end (as in
    // shader)
    // TODO: ^ retest it with non-indexed update
    //
    // And more strange, removing of  branches gives 5-10% speedup.
    // Seems that instruction prediction is really bad thing.
    //
    // And it seems that memory access (even for accumulating matix)
    // is really expensive (commenting of branches in accumulating code
    // doesn't change speed at all)

#define PROCESS_Y( _process_z )                                         \
    /*if ( w->y() )*/                                                   \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->y()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->y()].second; \
        *v += (mul3(rm, *sv) + tv) * w->y();                             \
                                                                        \
        _process_z;                                                     \
    }

#define PROCESS_Z( _process_w )                                         \
    /*if ( w->z() )*/                                                   \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->z()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->z()].second; \
        *v += (mul3(rm, *sv) + tv) * w->z();                               \
                                                                        \
        _process_w;                                                     \
    }

#define PROCESS_W()                                                     \
    /*if ( w->w() )*/                                                   \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->w()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->w()].second; \
        *v += (mul3(rm, *sv) + tv) * w->w();                               \
    }

#define STOP

    switch ( mesh->data->maxBonesInfluence )
    {
        case 1:
	  //#pragma omp parallel for -
	  // w/o      - 1.5s
	  // 1thread  - 1.7s
	  // 2threads - 2.1s => no win here
            ITERATE( PROCESS_X( STOP ) );
            break;

        case 2:
	  //#pragma omp parallel for						
            ITERATE( PROCESS_X( PROCESS_Y ( STOP ) ) );
            break;

        case 3:
	  //#pragma omp parallel for					       
            ITERATE( PROCESS_X( PROCESS_Y ( PROCESS_Z( STOP ) ) ) );
            break;

        case 4:
	  //#pragma omp parallel for						
            ITERATE( PROCESS_X( PROCESS_Y ( PROCESS_Z( PROCESS_W() ) ) ) );
            break;

        default:
            throw std::runtime_error( "maxBonesInfluence > 4 ???" );            
    }

    dirtyBound();
}


// -- Depth submesh --

SubMeshDepth::SubMeshDepth( SubMeshHardware* hw )
    : hwMesh( hw )
{
    setThreadSafeRefUnref( true );

    setUseDisplayList( false );
    setSupportsDisplayList( false );
    setUseVertexBufferObjects( false ); // false is default
    setStateSet( hwMesh->getCoreModelMesh()->staticDepthStateSet.get() );
    dirtyBound();

    setUserData( getStateSet() /*any referenced*/ );
    // ^ make this node not redundant and not suitable for merging for osgUtil::Optimizer
}

void
SubMeshDepth::drawImplementation(osg::RenderInfo& renderInfo) const
{
    hwMesh->drawImplementation( renderInfo, getStateSet() );
}

void
SubMeshDepth::compileGLObjects(osg::RenderInfo& renderInfo) const
{
    hwMesh->compileGLObjects( renderInfo );
}

void
SubMeshDepth::update( bool deformed )
{
    if ( deformed )
    {
        setStateSet( hwMesh->getCoreModelMesh()->depthStateSet.get() );
    }
    else
    {
        setStateSet( hwMesh->getCoreModelMesh()->staticDepthStateSet.get() );
    }

    dirtyBound();
}

osg::BoundingBox
SubMeshDepth::computeBound() const
{
    return hwMesh->computeBound();
}
