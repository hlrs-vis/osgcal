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
//#include <GL/gl.h>
//#include <GL/glu.h>

//#include <osg/VertexProgram>
//#include <osg/GL2Extensions>
#include <osg/CullFace>

#include <osgCal/HardwareMesh>

using namespace osgCal;



HardwareMesh::HardwareMesh( ModelData*             _modelData,
                            const CoreModel::Mesh* _mesh,
                            bool                   useDepthFirstMesh )
    : Mesh( _modelData, _mesh )
{   
    setUseDisplayList( false );
    setSupportsDisplayList( false );
    // ^ no display lists since we create them manually
    
    setUseVertexBufferObjects( false ); // false is default

    setStateSet( mesh->stateSets->staticHardware[ useDepthFirstMesh ].get() );
    // Initially we use static (not skinning) state set. It will
    // changed to skinning (in update() method when some animation
    // starts.

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
    if ( useDepthFirstMesh
         &&
         !(mesh->stateSets->staticHardware[0].get()->getRenderingHint()
           & osg::StateSet::TRANSPARENT_BIN) )
    {
        depthMesh = new DepthMesh( this ); 
    }

    setUserData( getStateSet() /*any referenced*/ );
    // ^ make this node not redundant and not suitable for merging for osgUtil::Optimizer
    // (with merging & flattening turned on some color artefacts arise)
    // TODO: how to completely disable FLATTEN_STATIC_TRANSFORMS on models?
    // it copies vertex buffer (which is per model) for each submesh
    // which takes too much memory
}

osg::Object*
HardwareMesh::cloneType() const
{
    throw std::runtime_error( "cloneType() is not implemented" );
}

osg::Object*
HardwareMesh::clone( const osg::CopyOp& ) const
{
    throw std::runtime_error( "clone() is not implemented" );
}

void
HardwareMesh::drawImplementation( osg::RenderInfo& renderInfo ) const
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
        throw std::runtime_error( "HardwareMesh::drawImplementation(): can't get program (shader compilation failed?" );
    }

    return stateProgram->getPCP( state.getContextID() );
}

void
HardwareMesh::drawImplementation( osg::RenderInfo&     renderInfo,
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
                                           mesh->data->getBonesCount(), GL_FALSE,
                                           rotationMatrices );
        if ( translationVectorsAttrib >= 0 )
        {
            gl2extensions->glUniform3fv( translationVectorsAttrib,
                                         mesh->data->getBonesCount(),
                                         translationVectors );
        }

        const GLfloat translation[3] = {0,0,0};
        const GLfloat rotation[9] = {1,0,0, 0,1,0, 0,0,1};
        gl2extensions->glUniformMatrix3fv( rotationMatricesAttrib + 30, 1, GL_FALSE, rotation );
        if ( translationVectorsAttrib >= 0 )
        {
            gl2extensions->glUniform3fv( translationVectorsAttrib + 30, 1, translation );
        }
    }

    // -- Create display list if not yet exists --
    unsigned int contextID = renderInfo.getContextID();

    mesh->displayLists->mutex.lock();
    GLuint& dl = mesh->displayLists->lists[ contextID ];

    if( dl != 0 )
    {
        mesh->displayLists->mutex.unlock();
    }
    else
    {
        dl = generateDisplayList( contextID, getGLObjectSizeHint() );

        innerDrawImplementation( renderInfo, dl );
        mesh->displayLists->mutex.unlock();

        mesh->displayLists->checkAllDisplayListsCompiled( mesh->data.get() );
    }

    // -- Call display list --
    // get mesh material to restore glColor after glDrawElements call
    const osg::Material* material = static_cast< const osg::Material* >
        ( state.getLastAppliedAttribute( osg::StateAttribute::MATERIAL ) );

    bool transparent = stateSet->getRenderingHint() & osg::StateSet::TRANSPARENT_BIN;

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
}

void
HardwareMesh::compileGLObjects(osg::RenderInfo& renderInfo) const
{
    Geometry::compileGLObjects( renderInfo );

    unsigned int contextID = renderInfo.getContextID();

    mesh->displayLists->mutex.lock();
    
    GLuint& dl = mesh->displayLists->lists[ contextID ];

    if( dl == 0 )
    {
        dl = generateDisplayList( contextID, getGLObjectSizeHint() );

        innerDrawImplementation( renderInfo, dl );
        mesh->displayLists->mutex.unlock();

        mesh->displayLists->checkAllDisplayListsCompiled( mesh->data.get() );
    }
    else
    {        
        mesh->displayLists->mutex.unlock();
    }
}

void
HardwareMesh::accept( osgUtil::GLObjectsVisitor* glv )
{
    glv->apply( *mesh->stateSets->staticHardware[depthMesh.valid()].get() );

    if ( !mesh->data->rigid )
    {
        glv->apply( *mesh->stateSets->hardware[depthMesh.valid()].get() );
    }

    if ( depthMesh.valid() )
    {
        glv->apply( *mesh->stateSets->staticDepthOnly.get() );

        if ( !mesh->data->rigid )
        {
            glv->apply( *mesh->stateSets->depthOnly.get() );
        }
    }
}

void
HardwareMesh::innerDrawImplementation( osg::RenderInfo&     renderInfo,
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
HardwareMesh::update()
{   
    // -- Setup rotation matrices & translation vertices --
    typedef std::pair< osg::Matrix3, osg::Vec3f > RTPair;
    float rotationTranslationMatricesData[ 31 * sizeof (RTPair) / sizeof ( float ) ];
    // we make data to not init matrices & vertex since we always set
    // them to correct data
    RTPair* rotationTranslationMatrices = (RTPair*)(void*)&rotationTranslationMatricesData;

    deformed = false;
    bool changed = false;

    for( int boneIndex = 0; boneIndex < mesh->data->getBonesCount(); boneIndex++ )
    {
        int boneId = mesh->data->getBoneId( boneIndex );
        const ModelData::BoneParams& bp = modelData->getBoneParams( boneId );

        deformed |= bp.deformed;
        changed  |= bp.changed;

        RTPair& rt = rotationTranslationMatrices[ boneIndex ];

        rt.first  = bp.rotation;
        rt.second = bp.translation;
    }

    // -- Check for deformation state and select state set type --
//    std::cout << "deformed = " << deformed << std::endl;
    if ( deformed )
    {
        setStateSet( mesh->stateSets->hardware[depthMesh.valid()].get() );
    }
    else
    {
        setStateSet( mesh->stateSets->staticHardware[depthMesh.valid()].get() );
        // for undeformed meshes we use static state set which not
        // perform vertex, normal, binormal and tangent deformations
        // in vertex shader
    }

    // -- Update depthMesh --
    if ( depthMesh.valid() )
    {
        depthMesh->update( deformed, changed );
    }

    // -- Check changes --
    if ( !changed )
    {
//        std::cout << "didn't changed" << std::endl;
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
    if ( mi->x() != 30 )                                                \
        /* we have no zero weight vertices they all bound to 30th bone */ \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->x()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->x()].second; \
        *v = (mul3(rm, *sv) + tv) * w->x();                             \
                                                                        \
        _process_y;                                                     \
    }                                                                   \
    else                                                                \
    {                                                                   \
        *v = *sv;                                                       \
    }                                                                   

    // Strange, but multiplying each matrix on source vector works
    // faster than accumulating matrix and multiply at the end (as in
    // shader)
    //
    // Not strange:
    // mul3            9*  6+
    // mul3 + tv       9*  9+
    // (mul3 + tv)*w  12*  9+
    // v += ..        12* 12+ (-3 for first)
    // x4 48* 45+
    //
    // +=rm,+=tv      12* 12+ (-12 for first)
    // (mul3 + tv)*w  12*  9+
    // x4 60* 45+
    // accumulation of matrix is more expensive than multiplicating
    //
    //
    // And it seems that memory access (even for accumulating matix)
    // is really expensive (commenting of branches in accumulating code
    // doesn't change speed at all)

#define PROCESS_Y( _process_z )                                         \
    if ( w->y() )                                                   \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->y()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->y()].second; \
        *v += (mul3(rm, *sv) + tv) * w->y();                            \
                                                                        \
        _process_z;                                                     \
    }                                                                   

#define PROCESS_Z( _process_w )                                         \
    if ( w->z() )                                                       \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->z()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->z()].second; \
        *v += (mul3(rm, *sv) + tv) * w->z();                            \
                                                                        \
        _process_w;                                                     \
    }

#define PROCESS_W()                                                     \
    if ( w->w() )                                                       \
    {                                                                   \
        const osg::Matrix3& rm = rotationTranslationMatrices[mi->w()].first; \
        const osg::Vec3f&   tv = rotationTranslationMatrices[mi->w()].second; \
        *v += (mul3(rm, *sv) + tv) * w->w();                            \
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

DepthMesh::DepthMesh( HardwareMesh* hw )
    : hwMesh( hw )
{
    //setThreadSafeRefUnref( true );

    setUseDisplayList( false );
    setSupportsDisplayList( false );
    setUseVertexBufferObjects( false ); // false is default
    setStateSet( hwMesh->getCoreModelMesh()->stateSets->staticDepthOnly.get() );
    dirtyBound();

    setUserData( getStateSet() /*any referenced*/ );
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
DepthMesh::update( bool deformed, bool changed )
{
    if ( deformed )
    {
        setStateSet( hwMesh->getCoreModelMesh()->stateSets->depthOnly.get() );
    }
    else
    {
        setStateSet( hwMesh->getCoreModelMesh()->stateSets->staticDepthOnly.get() );
    }

    if ( changed )
    {
        dirtyBound();
    }
}

osg::BoundingBox
DepthMesh::computeBound() const
{
    return hwMesh->computeBound();
}
