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
#include <memory>
#include <osg/io_utils>

#include <osgCal/MeshLoader>


namespace osgCal
{

// -- Mesh data --

static
osg::BoundingBox
calculateBoundingBox( const VertexBuffer* vb )
{
    osg::BoundingBox bb;

    for ( VertexBuffer::const_iterator v = vb->begin(), vEnd = vb->end();
          v != vEnd; ++v )
    {
        bb.expandBy( *v );
    }

    return bb;
}

static
void
checkRigidness( osgCal::MeshData* m,
                int unriggedBoneIndex )
{
    // -- Calculate maxBonesInfluence & rigidness --
    m->maxBonesInfluence = 0;
    m->rigid = true;

    MatrixIndexBuffer::const_iterator mi   = m->matrixIndexBuffer->begin();
    WeightBuffer::const_iterator      w    = m->weightBuffer->begin();
    WeightBuffer::const_iterator      wEnd = m->weightBuffer->end();
        
    for ( ; w != wEnd; ++w, ++mi )
    {
        if ( !( mi->r() == 0
                &&
                w->x() == 1.0f ) )
        {
            m->rigid = false;
        }

        if ( m->maxBonesInfluence < 1 && w->x() > 0.0 )
            m->maxBonesInfluence = 1;
        if ( m->maxBonesInfluence < 2 && w->y() > 0.0 )
            m->maxBonesInfluence = 2;
        if ( m->maxBonesInfluence < 3 && w->z() > 0.0 )
            m->maxBonesInfluence = 3;
        if ( m->maxBonesInfluence < 4 && w->w() > 0.0 )
            m->maxBonesInfluence = 4;
    }

    if ( m->rigid )
    {
        if ( m->getBonesCount() != 1 )
        {
            throw std::runtime_error( "must be one bone in this mesh" );
        }

        m->rigidBoneId = m->getBoneId( 0 );
    }

    if ( m->maxBonesInfluence == 0 ) // unrigged mesh
    {
        m->rigid = true; // mesh is rigid when all its vertices are
        // rigged to one bone with weight = 1.0,
        // or when no one vertex is rigged at all
        m->rigidBoneId = -1; // no bone
    }

    // -- Remove unneded for rigid mesh --
    if ( m->rigid )
    {
        m->weightBuffer = 0;
        m->matrixIndexBuffer = 0;
        m->bonesIndices.clear();
    }
    else
    // -- Check zero weight bones --
    {
        MatrixIndexBuffer::iterator mi = m->matrixIndexBuffer->begin();
        WeightBuffer::iterator      w  = m->weightBuffer->begin();
        bool hasUnriggedVertices = false;

        while ( mi < m->matrixIndexBuffer->end() )
        {
            if ( (*w)[0] <= 0.0 ) // no influences at all
            {
                (*w)[0] = 1.0;
                (*mi)[0] = m->bonesIndices.size();
                hasUnriggedVertices = true;
                // last+1 bone in shader is always identity matrix.
                // we need this hack for meshes where some vertexes
                // are rigged and some are not (so we create
                // non-movable bone for them) (see #68)
            }
            
            ++mi;
            ++w;
        }

        if ( hasUnriggedVertices )
        {
            m->bonesIndices.push_back( unriggedBoneIndex );
        }
    }
}

static
void
checkForEmptyTexCoord( osgCal::MeshData* m )
{
    for ( TexCoordBuffer::const_iterator
              tc = m->texCoordBuffer->begin(),
              tcEnd = m->texCoordBuffer->end();
          tc != tcEnd; ++tc )
    {
        if ( tc->x() != 0.0f || tc->y() != 1.0f )
        {
            // y compared with 1.0 since we invert texture coordinates
            return;
        }
    }

    // -- Remove unused texture coordinates and tangents --
//    std::cout << "empty tex coord: " << m->name << std::endl;
    m->texCoordBuffer = 0;
    m->tangentAndHandednessBuffer = 0;
}

static
void
generateTangentAndHandednessBuffer( osgCal::MeshData* m,
                                    const CalIndex*   indexBuffer )
{
    if ( !m->texCoordBuffer.valid() )
    {
        return;
    }

    int vertexCount = m->vertexBuffer->size();
    int faceCount   = m->getIndicesCount() / 3;    

    m->tangentAndHandednessBuffer = new TangentAndHandednessBuffer( vertexCount );

    CalVector* tan1 = new CalVector[vertexCount];
    CalVector* tan2 = new CalVector[vertexCount];

    const GLfloat* texCoordBufferData = (GLfloat*) m->texCoordBuffer->getDataPointer();

    const GLfloat* vb = (GLfloat*) m->vertexBuffer->getDataPointer();
#ifdef OSG_CAL_BYTE_BUFFERS
    GLfloat* thb = new GLfloat[ vertexCount*4 ];
    const GLfloat* nb = floatNormalBuffer;
#else
    GLfloat* thb = (GLfloat*) m->tangentAndHandednessBuffer->getDataPointer();
//    GLshort* thb = (GLshort*) m->tangentAndHandednessBuffer->getDataPointer();
    const GLfloat* nb = (GLfloat*) m->normalBuffer->getDataPointer();
#endif

    for ( int face = 0; face < faceCount; face++ )
    {
        for ( int j = 0; j < 3; j++ )
        {
            // there seems to be no visual difference in calculating
            // tangent per vertex (as is tan1[i1] += spos(j=0,1,2))
            // or per face (tan1[i1,i2,i3] += spos)
            CalIndex i1 = indexBuffer[face*3+(j+0)%3];
            CalIndex i2 = indexBuffer[face*3+(j+1)%3];
            CalIndex i3 = indexBuffer[face*3+(j+2)%3];
        
            const float* v1 = &vb[i1*3];
            const float* v2 = &vb[i2*3];
            const float* v3 = &vb[i3*3];

            const float* w1 = &texCoordBufferData[i1*2];
            const float* w2 = &texCoordBufferData[i2*2];
            const float* w3 = &texCoordBufferData[i3*2];

#define x(_a) (_a[0])
#define y(_a) (_a[1])
#define z(_a) (_a[2])
        
            float x1 = x(v2) - x(v1);
            float x2 = x(v3) - x(v1);
            float y1 = y(v2) - y(v1);
            float y2 = y(v3) - y(v1);
            float z1 = z(v2) - z(v1);
            float z2 = z(v3) - z(v1);
        
            float s1 = x(w2) - x(w1);
            float s2 = x(w3) - x(w1);
            float t1 = y(w2) - y(w1);
            float t2 = y(w3) - y(w1);

#undef x
#undef y
#undef z
        
            //float r = 1.0F / (s1 * t2 - s2 * t1);
            float r = (s1 * t2 - s2 * t1) < 0 ? -1.0 : 1.0;
            CalVector sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                           (t2 * z1 - t1 * z2) * r);
            CalVector tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                           (s1 * z2 - s2 * z1) * r);

            // sdir & tdir can be 0 (when UV unwrap doesn't exists
            // or has errors like coincide points)
            // we ignore them
            if ( sdir.length() > 0 )
            {
                sdir.normalize(); 

                tan1[i1] += sdir;
                //tan1[i2] += sdir;
                //tan1[i3] += sdir;
            }

            if ( tdir.length() > 0 )
            {
                tdir.normalize();

                tan2[i1] += tdir;
                //tan2[i2] += tdir;
                //tan2[i3] += tdir;
            }
        }
    }
    
    for (long a = 0; a < vertexCount; a++)
    {
        CalVector tangent;
        CalVector binormal;
        CalVector t = tan1[a];
        CalVector b = tan2[a];
        CalVector n = CalVector( nb[a*3+0],
                                 nb[a*3+1],
                                 nb[a*3+2] );

        // tangent & bitangent can be zero when UV unwrap doesn't exists
        // or has errors like coincide points
        if ( t.length() > 0 )
        {
            t.normalize();
        
            // Gram-Schmidt orthogonalize
            tangent = t - n * (n*t);
            tangent.normalize();

            // Calculate handedness
            binormal = CalVector(n % tangent) *
                ((((n % t) * b) < 0.0F) ? -1.0f : 1.0f);
            binormal.normalize();
        }
        else if ( b.length() > 0 )
        {
            b.normalize();
        
            // Gram-Schmidt orthogonalize
            binormal = b - n * (n*b);
            binormal.normalize();

            // Calculate handedness
            tangent = CalVector(n % binormal) *
                ((((n % b) * t) < 0.0F) ? -1.0f : 1.0f);
            tangent.normalize();
        }

//         std::cout << "t = " << tangent.x  << '\t' << tangent.y  << '\t' << tangent.z  << '\n';
//         std::cout << "b = " << binormal.x << '\t' << binormal.y << '\t' << binormal.z << '\n';
//            std::cout << "n = " << n.x        << '\t' << n.y        << '\t' << n.z        << '\n';

//         thb[a*4+0] = floatToHalf( tangent.x ); //tangent.x * 0x7FFF;
//         thb[a*4+1] = floatToHalf( tangent.y ); //tangent.y * 0x7FFF;
//         thb[a*4+2] = floatToHalf( tangent.z ); //tangent.z * 0x7FFF;
//         thb[a*4+3] = floatToHalf((((n % tangent) * binormal) > 0.0F) ? -1.0f : 1.0f); // handedness
        thb[a*4+0] = tangent.x; 
        thb[a*4+1] = tangent.y;
        thb[a*4+2] = tangent.z;
        thb[a*4+3] = ((((n % tangent) * binormal) > 0.0F) ? -1.0f : 1.0f); // handedness
    }
    
    delete[] tan1;
    delete[] tan2;

#ifdef OSG_CAL_BYTE_BUFFERS
    GLbyte* tangents = (GLbyte*) tangentBuffer->getDataPointer();
    GLbyte* binormals = (GLbyte*) binormalBuffer->getDataPointer();

    for ( int i = 0; i < vertexCount*3; i++ )
    {
        tangents[i]  = static_cast< GLbyte >( tangentBuffer[i]*127.0 );
        binormals[i] = static_cast< GLbyte >( binormalBuffer[i]*127.0 );
        //std::cout << (int)tangents[i] << '\n';
    }

    delete[] tangentBuffer;
    delete[] binormalBuffer;
#endif
}

void
loadMeshes( CalCoreModel* calCoreModel,
            MeshesVector& meshes )
{
    const int maxVertices = Constants::MAX_VERTEX_PER_MODEL;
    const int maxFaces    = Constants::MAX_VERTEX_PER_MODEL * 3;

    std::auto_ptr< CalHardwareModel > calHardwareModel( new CalHardwareModel( calCoreModel ) );
    
    osg::ref_ptr< VertexBuffer >      vertexBuffer( new VertexBuffer( maxVertices ) );
    osg::ref_ptr< WeightBuffer >      weightBuffer( new WeightBuffer( maxVertices ) );
    osg::ref_ptr< MatrixIndexBuffer > matrixIndexBuffer( new MatrixIndexBuffer( maxVertices ) );
    osg::ref_ptr< NormalBuffer >      normalBuffer( new NormalBuffer( maxVertices ) );
    osg::ref_ptr< NormalBuffer >      tangentBuffer( new NormalBuffer( maxVertices ) );
    osg::ref_ptr< NormalBuffer >      binormalBuffer( new NormalBuffer( maxVertices ) );
    osg::ref_ptr< TexCoordBuffer >    texCoordBuffer( new TexCoordBuffer( maxVertices ) );
    std::vector< CalIndex >           indexBuffer( maxFaces*3 );

    std::vector< float > floatMatrixIndexBuffer( maxVertices*4 );

    calHardwareModel->setVertexBuffer((char*)vertexBuffer->getDataPointer(),
                                      3*sizeof(float));
#ifdef OSG_CAL_BYTE_BUFFERS
    std::vector< float > floatNormalBuffer( getVertexCount()*3 );
    calHardwareModel->setNormalBuffer((char*)&floatNormalBuffer.begin(),
                                      3*sizeof(float));
#else
    calHardwareModel->setNormalBuffer((char*)normalBuffer->getDataPointer(),
                                      3*sizeof(float));
#endif
    calHardwareModel->setWeightBuffer((char*)weightBuffer->getDataPointer(),
                                      4*sizeof(float));
    calHardwareModel->setMatrixIndexBuffer((char*)&floatMatrixIndexBuffer.front(),
                                           4*sizeof(float));
    calHardwareModel->setTextureCoordNum( 1 );
    calHardwareModel->setTextureCoordBuffer(0, // texture stage #
                                            (char*)texCoordBuffer->getDataPointer(),
                                            2*sizeof(float));
    calHardwareModel->setIndexBuffer( &indexBuffer.front() );
    // calHardwareModel->setCoreMeshIds(_activeMeshes);
    // if ids not set all meshes will be used at load() time

    //std::cout << "calHardwareModel->load" << std::endl;
    calHardwareModel->load( 0, 0, Constants::MAX_BONES_PER_MESH );
    //std::cout << "calHardwareModel->load ok" << std::endl;

    int vertexCount = calHardwareModel->getTotalVertexCount();
//    int faceCount   = calHardwareModel->getTotalFaceCount();

//    std::cout << "vertexCount = " << vertexCount << "; faceCount = " << faceCount << std::endl;
    
    GLubyte* matrixIndexBufferData = (GLubyte*) matrixIndexBuffer->getDataPointer();

    for ( int i = 0; i < vertexCount*4; i++ )
    {
        matrixIndexBufferData[i] = static_cast< GLubyte >( floatMatrixIndexBuffer[i] );
    }

#ifdef OSG_CAL_BYTE_BUFFERS
    GLbyte* normals = (GLbyte*) normalBuffer->getDataPointer();

    for ( int i = 0; i < vertexCount*3; i++ )
    {
        normals[i]  = static_cast< GLbyte >( floatNormalBuffer[i]*127.0 );
    }
#endif

    // invert UVs for OpenGL (textures are inverted otherwise - for example, see abdulla/klinok)
    GLfloat* texCoordBufferData = (GLfloat*) texCoordBuffer->getDataPointer();

    for ( float* tcy = texCoordBufferData + 1;
          tcy < texCoordBufferData + 2*vertexCount;
          tcy += 2 )
    {
        *tcy = 1.0f - *tcy;
    }

    // -- And now create meshes data --
    int unriggedBoneIndex = calCoreModel->getCoreSkeleton()->getVectorCoreBone().size();
    // we add empty bone in ModelData to handle unrigged vertices;
    
    for( int hardwareMeshId = 0; hardwareMeshId < calHardwareModel->getHardwareMeshCount(); hardwareMeshId++ )
    {
        calHardwareModel->selectHardwareMesh(hardwareMeshId);
        int faceCount = calHardwareModel->getFaceCount();

        if ( faceCount == 0 )
        {
            continue; // we ignore empty meshes
        }
        
        CalHardwareModel::CalHardwareMesh* hardwareMesh =
            &calHardwareModel->getVectorHardwareMesh()[ hardwareMeshId ];

        osg::ref_ptr< MeshData > m( new MeshData );
        
        m->name = calCoreModel->getCoreMesh( hardwareMesh->meshId )->getName();
        m->coreMaterial = hardwareMesh->pCoreMaterial;
        if ( m->coreMaterial == NULL )
        {
            CalCoreMesh*    coreMesh    = calCoreModel->getCoreMesh( hardwareMesh->meshId );
            CalCoreSubmesh* coreSubmesh = coreMesh->getCoreSubmesh( hardwareMesh->submeshId );
            // hardwareMesh->pCoreMaterial =
            //   coreModel->getCoreMaterial( coreSubmesh->getCoreMaterialThreadId() );
            char buf[ 1024 ];
            snprintf( buf, 1024,
                      "pCoreMaterial == NULL for mesh '%s' (mesh material id = %d), verify your mesh file data",
                      m->name.c_str(),
                      coreSubmesh->getCoreMaterialThreadId() );
            throw std::runtime_error( buf );
        }

        // -- Create index buffer --
        int indexesCount = faceCount * 3;
        int startIndex = calHardwareModel->getStartIndex();

        if ( indexesCount <= 0x100 )
        {
            m->indexBuffer = new osg::DrawElementsUByte( osg::PrimitiveSet::TRIANGLES, indexesCount );

            GLubyte* data = (GLubyte*)m->indexBuffer->getDataPointer();
            const CalIndex* i    = &indexBuffer[ startIndex ];
            const CalIndex* iEnd = &indexBuffer[ startIndex + indexesCount ];
            while ( i < iEnd )
            {
                *data++ = (GLubyte)*i++;
            }
        }
        else if ( indexesCount <= 0x10000 )
        {
            m->indexBuffer = new osg::DrawElementsUShort( osg::PrimitiveSet::TRIANGLES, indexesCount );

            GLushort* data = (GLushort*)m->indexBuffer->getDataPointer();
            const CalIndex* i    = &indexBuffer[ startIndex ];
            const CalIndex* iEnd = &indexBuffer[ startIndex + indexesCount ];
            while ( i < iEnd )
            {
                *data++ = (GLushort)*i++;
            }
        }
        else
        {
            m->indexBuffer = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES, indexesCount );

            GLuint* data = (GLuint*)m->indexBuffer->getDataPointer();
            const CalIndex* i    = &indexBuffer[ startIndex ];
            const CalIndex* iEnd = &indexBuffer[ startIndex + indexesCount ];
            while ( i < iEnd )
            {
                *data++ = (GLuint)*i++;
            }
        }

        // -- Create other buffers --
        int vertexCount = calHardwareModel->getVertexCount();
        int baseVertexIndex = calHardwareModel->getBaseVertexIndex();

#define SUB_BUFFER( _type, _name )                                  \
        new _type( _name->begin() + baseVertexIndex,                \
                   _name->begin() + baseVertexIndex + vertexCount )
        
        m->vertexBuffer = SUB_BUFFER( VertexBuffer, vertexBuffer );
        m->weightBuffer = SUB_BUFFER( WeightBuffer, weightBuffer );
        m->matrixIndexBuffer = SUB_BUFFER( MatrixIndexBuffer, matrixIndexBuffer );
        m->normalBuffer = SUB_BUFFER( NormalBuffer, normalBuffer );
        m->texCoordBuffer = SUB_BUFFER( TexCoordBuffer, texCoordBuffer );

        // -- Parameters and buffers setup --
        m->boundingBox = calculateBoundingBox( m->vertexBuffer.get() );

        m->bonesIndices = hardwareMesh->m_vectorBonesIndices;

        checkRigidness( m.get(), unriggedBoneIndex );
        checkForEmptyTexCoord( m.get() );
        generateTangentAndHandednessBuffer( m.get(), &indexBuffer[ startIndex ] );

        meshes.push_back( m.get() );
    }
}

// -- Meshes I/O --

std::string
meshesCacheFileName( const std::string& cfgFileName )
{
    return cfgFileName + ".meshes.cache";
}

#if defined(_MSC_VER)
    typedef int int32_t;
#endif
    
#define READ_( _name, _buf, _size )                                                  \
    if ( fread( _buf, _size, 1, f ) != 1 )                                           \
    {                                                                                \
        throw std::runtime_error( "Can't read "#_name + std::string(" from ") + fn );\
    }

#define READ( _buf )                                                    \
    if ( fread( (void*)_buf->getDataPointer(), _buf->getTotalDataSize(), 1, f ) != 1 ) \
    {                                                                   \
        throw std::runtime_error( "Can't read "#_buf + std::string(" from ") + fn ); \
    }

#define READ_I32( _i )   { int32_t _i32_tmp = 0; READ_( _i, &_i32_tmp, 4 ); _i = _i32_tmp; }
#define READ_STRUCT( _s ) READ_( _s, &_s, sizeof ( _s ) )

#define WRITE_( _name, _buf, _size )                                                 \
    if ( fwrite( _buf, _size, 1, f ) != 1 )                                          \
    {                                                                                \
        throw std::runtime_error( "Can't write "#_name + std::string(" to ") + fn ); \
    }

#define WRITE( _buf )                                                   \
    if ( fwrite( _buf->getDataPointer(), _buf->getTotalDataSize(), 1, f ) != 1 ) \
    {                                                                   \
        throw std::runtime_error( "Can't write "#_buf + std::string(" to ") + fn ); \
    }

#define WRITE_I32( _i ) { int32_t _i32_tmp = _i; WRITE_( _i, &_i32_tmp, 4 ); }
#define WRITE_STRUCT( _s ) WRITE_( _s, &_s, sizeof ( _s ) )

/**
 * Simpe FILE wrapper, needed to call fclose() on exception.
 */
struct FileCloser
{       
        FILE*  f;

        FileCloser( FILE* f )
            : f( f )
        {}
        ~FileCloser()
        {
            fclose( f );
        }
};

/**
 * Set file I/O buffer to the specified size and free buffer on exit
 * from scope.
 * No file reads must be performed after FileBuffer exit its scope.
 */
struct FileBuffer
{
        char*  buf;
        
        FileBuffer( FILE* f,
                    int   size )
            : buf( (char*)malloc( size ) )
        {
            setvbuf( f, buf, _IOFBF, size );            
        }
        
        ~FileBuffer()
        {
            free( buf );
        }
};

/**
 * Type of buffer in meshes.cache file.
 *
 * We keep buffers separately from mesh descriptions for two reasons:
 *  - meshes can have variable number of buffers and I don't want to
 *    bother with detection of buffers end and next mesh start
 *  - we free some buffers after display list is created for all
 *    contexts and it's much better to keep these buffer in contiguous
 *    memory block to reduce memory fragmentation, so we place 
 *    index/vertex/weight/matrixIndex buffers first (they are needed
 *    for picking and vertex position calculation) and then
 *    normal/texCoord/tangent buffers for all meshes (these ones will
 *    be freed)
 */
enum BufferType
{
    BT_MASK                     = 0xFF0000,
    BT_INDEX                    = 0x010000,
    BT_VERTEX                   = 0x020000,
    BT_WEIGHT                   = 0x030000,
    BT_MATRIX_INDEX             = 0x040000,
    BT_NORMAL                   = 0x050000,
    BT_TEX_COORD                = 0x060000,
    BT_TANGENT_AND_HANDEDNESS   = 0x070000,
};
    

enum BufferElementType
{
    ET_MASK   = 0xFF00,
    ET_UBYTE  = 0x0100,
    ET_USHORT = 0x0200,
    ET_UINT   = 0x0300,
    ET_BYTE   = 0x0400,
    ET_SHORT  = 0x0500,
    ET_INT    = 0x0600,
    ET_FLOAT  = 0x0700,
};

enum BufferElementsCount
{
    EC_MASK = 0xFF,
    EC_1    = 0x01,
    EC_2    = 0x02,
    EC_3    = 0x03,
    EC_4    = 0x04,
};

static
void
readBuffer( MeshesVector& meshes,
            FILE*     f,
            const std::string& fn )
{
    int32_t meshIndex;
//    READ_I32( meshIndex );
    if ( fread( &meshIndex, 4, 1, f ) != 1 )
    {
        if ( feof( f ) )
        {
            return; // no error, file ended
        }
        else
        {
            throw std::runtime_error( "Can't read meshIndex" + std::string(" from ") + fn );
        }
    }

    MeshData* m = meshes[ meshIndex ].get();

    int bufferType;
    int bufferSize;

    READ_I32( bufferType );
    READ_I32( bufferSize );

#define CASE( _type, _name, _data_type )                \
        case BT_##_type:                                \
            m->_name = new _data_type( bufferSize );    \
            READ( m->_name );                           \
            break

//     printf( "reading %d mesh, buffer type = %d (0x%08X), buffer size = %d\n",
//             meshIndex, bufferType, bufferType, bufferSize );
//     fflush( stdout );
    
    switch ( bufferType & BT_MASK )
    {
        case BT_INDEX:
            switch ( bufferType & ET_MASK )
            {
                case ET_UBYTE:
                    m->indexBuffer = new osg::DrawElementsUByte( osg::PrimitiveSet::TRIANGLES, bufferSize );
                    break;

                case ET_USHORT:
                    m->indexBuffer = new osg::DrawElementsUShort( osg::PrimitiveSet::TRIANGLES, bufferSize );
                    break;

                case ET_UINT:
                    m->indexBuffer = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES, bufferSize );
                    break;

                default:
                {
                    char err[ 1024 ];
                    sprintf( err, "Unknown index buffer element type %d (0x%08X)",
                             bufferType & ET_MASK, bufferType & ET_MASK );
                    throw std::runtime_error( err );
                }

            }
            READ( m->indexBuffer );
            break;

        CASE( VERTEX, vertexBuffer, VertexBuffer );
        CASE( WEIGHT, weightBuffer, WeightBuffer );
        CASE( MATRIX_INDEX, matrixIndexBuffer, MatrixIndexBuffer );
        CASE( NORMAL, normalBuffer, NormalBuffer );
        CASE( TEX_COORD, texCoordBuffer, TexCoordBuffer );
        CASE( TANGENT_AND_HANDEDNESS, tangentAndHandednessBuffer, TangentAndHandednessBuffer );

        default:
        {
            char err[ 1024 ];
            sprintf( err, "Unknown buffer type %d (0x%08X)", bufferType, bufferType );
            throw std::runtime_error( err );
        }
    }

#undef CASE
}

static const int HW_MODEL_FILE_VERSION = 0xCA3D0003;

void
loadMeshes( const std::string&  fn,
            const CalCoreModel* calCoreModel,
            MeshesVector& meshes )
{
    FILE* f = fopen( fn.c_str(), "rb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't open " + fn );
    }

    FileCloser closeOnExit( f );
//    FileBuffer setReadBufferOfSize( f, 1*1024*1024 ); <- not so much difference

    // -- Check version --
    int version;

    READ_I32( version );
    if ( version != HW_MODEL_FILE_VERSION )
    {
        fclose( f );
        throw std::runtime_error( "Incorrect file version " + fn + ". Try rerun osgCalPreparer." );
    }

    // -- Read mesh descriptions --
    int meshesCount = 0;

    READ_I32( meshesCount );
    meshes.resize( meshesCount );

    for ( int i = 0; i < meshesCount; i++ )
    {
        MeshData* m = new MeshData;
        meshes[i] = m;

        // -- Read name --
        int nameBufSize;
        READ_I32( nameBufSize );
        if ( nameBufSize > 1024 )
        {
            throw std::runtime_error( "Too long mesh name (incorrect meshes.cache file?)." );
        }
        char name[ 1024 ];
        READ_( m->name, name, nameBufSize );
        m->name = std::string( &name[0], &name[ nameBufSize ] );

        // -- Read material --
        int coreMaterialThreadId;
        READ_I32( coreMaterialThreadId );

        m->coreMaterial = const_cast< CalCoreModel* >( calCoreModel )->
            getCoreMaterial( coreMaterialThreadId );

        // -- Read bone parameters --
        READ_I32( m->rigid );
        READ_I32( m->rigidBoneId );
        READ_I32( m->maxBonesInfluence );

        // -- Read bonesIndices --
        int biSize = 0;
        READ_I32( biSize );
        m->bonesIndices.resize( biSize );
        for ( int bi = 0; bi < biSize; bi++ )
        {
            READ_I32( m->bonesIndices[ bi ] );
        }

        // -- Read boundingBox --
        assert( sizeof ( m->boundingBox ) == 6 * 4 ); // must be 6 floats
        READ_STRUCT( m->boundingBox );
    }

    // -- Read meshes buffers --
    while ( !feof( f ) )
    {
        readBuffer( meshes, f, fn );
    }
}


int
getCoreMaterialThreadId( CalCoreModel* model,
                         const CalCoreMaterial* material )
{
    for ( int i = 0; i < model->getCoreMaterialCount(); i++ )
    {
        if ( model->getCoreMaterial( i ) == material )
        {
            return i;
        }
    }
    return (-1);
}


void saveMeshes( const CalCoreModel* calCoreModel,
                 const MeshesVector& meshes,
                 const std::string&  fn )
{
    FILE* f = fopen( fn.c_str(), "wb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't create " + fn );
    }

    FileCloser closeOnExit( f );
//    FileBuffer setReadBufferOfSize( f, 1*1024*1024 );

    WRITE_I32( HW_MODEL_FILE_VERSION );

    // -- Write meshes --
    WRITE_I32( meshes.size() );

    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        MeshData* m = meshes[i].get();

        // -- Write name --
        const std::string& name = m->name;
        WRITE_I32( name.size() );
        WRITE_( m->name, name.data(), name.size() );

        // -- Write material --
        int coreMaterialThreadId = getCoreMaterialThreadId(
            const_cast< CalCoreModel* >( calCoreModel ), m->coreMaterial );
        if ( coreMaterialThreadId < 0 )
        {
            fclose( f );
            throw std::runtime_error( "Can't get coreMaterialThreadId (mesh.pCoreMaterial not found in coreModel?" );            
        }
        WRITE_I32( coreMaterialThreadId );

        // -- Read bone parameters --
        WRITE_I32( m->rigid );
        WRITE_I32( m->rigidBoneId );
        WRITE_I32( m->maxBonesInfluence );

        // -- Read bonesIndices --
        WRITE_I32( m->bonesIndices.size() );
        for ( size_t bi = 0; bi < m->bonesIndices.size(); bi++ )
        {
            WRITE_I32( m->bonesIndices[ bi ] );
        }

        // -- Write boundingBox --
        assert( sizeof ( m->boundingBox ) == 6 * 4 ); // must be 6 floats
        WRITE_STRUCT( m->boundingBox );
    }

#define WRITE_BUFFER( _bufferType, _buffer )    \
    if ( m->_buffer.valid() )                   \
    {                                           \
        WRITE_I32( i );                         \
        WRITE_I32( _bufferType );               \
        WRITE_I32( m->_buffer->size() );        \
        WRITE( m->_buffer )                     \
    }

    // -- Write resident mesh buffers --
    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        MeshData* m = meshes[i].get();

        WRITE_I32( i );
        switch ( m->indexBuffer->getType() )
        {
            case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
                WRITE_I32( BT_INDEX + ET_UBYTE );
                break;

            case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
                WRITE_I32( BT_INDEX + ET_USHORT );
                break;

            case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
                WRITE_I32( BT_INDEX + ET_UINT );
                break;

            default:
                throw std::runtime_error( "unsupported indexBuffer type?" );

        }
        WRITE_I32( m->getIndicesCount() );
        WRITE( m->indexBuffer );
        
        WRITE_BUFFER( BT_VERTEX, vertexBuffer );
        WRITE_BUFFER( BT_WEIGHT, weightBuffer );
        WRITE_BUFFER( BT_MATRIX_INDEX, matrixIndexBuffer );
    }

    // -- Write mesh buffers that will be freed after display list created --
    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        MeshData* m = meshes[i].get();

        WRITE_BUFFER ( BT_NORMAL, normalBuffer );
        WRITE_BUFFER ( BT_TEX_COORD, texCoordBuffer );
        WRITE_BUFFER ( BT_TANGENT_AND_HANDEDNESS, tangentAndHandednessBuffer );
    }

#undef WRITE_BUFFER
}

}
