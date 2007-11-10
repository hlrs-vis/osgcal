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
#include <sys/stat.h>
#include <stdexcept>

#include <osg/io_utils>

#include <osgDB/FileNameUtils>

#include <osgCal/IOUtils>


namespace osgCal
{

std::string
getPrefix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_first_of(":");

    if(pos == std::string::npos) return std::string();

    return str.substr(0, pos + 1);
}

std::string
getSuffix( const std::string& str )
{
    std::string::size_type pos;
    pos = str.find_last_of(":");

    if(pos == std::string::npos) return str;

    return str.substr(pos + 1);
}

bool
endsWith( const std::string& str,
          const std::string& suffix )
{
    std::string::size_type suffl = suffix.length();;
    std::string::size_type strl = str.length();;

    return ( strl >= suffl && str.substr( strl - suffl ) == suffix );
}

std::string
getAfter( const std::string& prefix,
          const std::string& str )
{
    return str.substr(prefix.size());
}

bool
prefixEquals( const std::string& str,
              const std::string& prefix )
{
    if ( str.length() < prefix.length() )
    {
        return false;
    }
    else
    {
        return str.substr( 0, prefix.length() ) == prefix;
    }
}

bool
isFileExists( const std::string& f )
{
    struct stat st;

    return ( stat( f.c_str(), &st ) == 0 );
}

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
checkRigidness( osgCal::MeshData* m )
{
    // -- Calculate maxBonesInfluence & rigidness --
    m->maxBonesInfluence = 0;
    m->rigid = true;

    MatrixIndexBuffer::const_iterator mi   = m->matrixIndexBuffer->begin();
    WeightBuffer::const_iterator      w    = m->weightBuffer->begin();
    WeightBuffer::const_iterator      wEnd = m->weightBuffer->end();
        
    for ( ; w != wEnd; ++w, ++mi )
    {
        if ( !( mi->x() == 0
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

        while ( mi < m->matrixIndexBuffer->end() )
        {
            if ( (*w)[0] <= 0.0 ) // no influences at all
            {
                (*w)[0] = 1.0;
                (*mi)[0] = 30;
                // last+1 bone in shader is always identity matrix.
                // we need this hack for meshes where some vertexes
                // are rigged and some are not (so we create
                // non-movable bone for them) (see #68)
            }
            
            ++mi;
            ++w;
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
generateTangentAndHandednessBuffer( osgCal::MeshData* m )
{
    if ( !m->texCoordBuffer.valid() )
    {
        return;
    }

    int vertexCount = m->vertexBuffer->size();
    int faceCount   = m->indexBuffer->size() / 3;    

    m->tangentAndHandednessBuffer = new TangentAndHandednessBuffer( vertexCount );

    CalVector* tan1 = new CalVector[vertexCount];
    CalVector* tan2 = new CalVector[vertexCount];

    const GLfloat* texCoordBufferData = (GLfloat*) m->texCoordBuffer->getDataPointer();

    const GLuint*  ib = (GLuint*) m->indexBuffer->getDataPointer();
    const GLfloat* vb = (GLfloat*) m->vertexBuffer->getDataPointer();
#ifdef OSG_CAL_BYTE_BUFFERS
    GLfloat* thb = new GLfloat[ vertexCount*4 ];
    const GLfloat* nb = floatNormalBuffer;
#else
    GLfloat* thb = (GLfloat*) m->tangentAndHandednessBuffer->getDataPointer();
    const GLfloat* nb = (GLfloat*) m->normalBuffer->getDataPointer();
#endif

    for ( int face = 0; face < faceCount; face++ )
    {
        for ( int j = 0; j < 3; j++ )
        {
            // there seems to be no visual difference in calculating
            // tangent per vertex (as is tan1[i1] += spos(j=0,1,2))
            // or per face (tan1[i1,i2,i3] += spos)
            GLuint i1 = ib[face*3+(j+0)%3];
            GLuint i2 = ib[face*3+(j+1)%3];
            GLuint i3 = ib[face*3+(j+2)%3];
        
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
    throw (std::runtime_error)
{
    const int maxVertices = OSGCAL_MAX_VERTEX_PER_MODEL;
    const int maxFaces    = OSGCAL_MAX_VERTEX_PER_MODEL * 3;

    std::auto_ptr< CalHardwareModel > calHardwareModel( new CalHardwareModel( calCoreModel ) );
    
    osg::ref_ptr< VertexBuffer >      vertexBuffer( new VertexBuffer( maxVertices ) );
    osg::ref_ptr< WeightBuffer >      weightBuffer( new WeightBuffer( maxVertices ) );
    osg::ref_ptr< MatrixIndexBuffer > matrixIndexBuffer( new MatrixIndexBuffer( maxVertices ) );
    osg::ref_ptr< NormalBuffer >      normalBuffer( new NormalBuffer( maxVertices ) );
    osg::ref_ptr< TangentBuffer >     tangentBuffer( new TangentBuffer( maxVertices ) );
    osg::ref_ptr< BinormalBuffer >    binormalBuffer( new BinormalBuffer( maxVertices ) );
    osg::ref_ptr< TexCoordBuffer >    texCoordBuffer( new TexCoordBuffer( maxVertices ) );
    osg::ref_ptr< osg::UIntArray >    indexBuffer( new osg::UIntArray( maxFaces*3 ) );

    float* floatMatrixIndexBuffer = new float[maxVertices*4];

    calHardwareModel->setVertexBuffer((char*)vertexBuffer->getDataPointer(),
                                      3*sizeof(float));
#ifdef OSG_CAL_BYTE_BUFFERS
    float* floatNormalBuffer = new float[getVertexCount()*3];
    calHardwareModel->setNormalBuffer((char*)floatNormalBuffer,
                                      3*sizeof(float));
#else
    calHardwareModel->setNormalBuffer((char*)normalBuffer->getDataPointer(),
                                      3*sizeof(float));
#endif
    calHardwareModel->setWeightBuffer((char*)weightBuffer->getDataPointer(),
                                      4*sizeof(float));
    calHardwareModel->setMatrixIndexBuffer((char*)floatMatrixIndexBuffer,
                                           4*sizeof(float));
    calHardwareModel->setTextureCoordNum(1);
    calHardwareModel->setTextureCoordBuffer(0, // texture stage #
                                            (char*)texCoordBuffer->getDataPointer(),
                                            2*sizeof(float));
    calHardwareModel->setIndexBuffer((CalIndex*)(GLuint*)indexBuffer->getDataPointer());
    // calHardwareModel->setCoreMeshIds(_activeMeshes);
    // if ids not set all meshes will be used at load() time

    //std::cout << "calHardwareModel->load" << std::endl;
    calHardwareModel->load( 0, 0, OSGCAL_MAX_BONES_PER_MESH );
    //std::cout << "calHardwareModel->load ok" << std::endl;

    int vertexCount = calHardwareModel->getTotalVertexCount();
//    int faceCount   = calHardwareModel->getTotalFaceCount();

//    std::cout << "vertexCount = " << vertexCount << "; faceCount = " << faceCount << std::endl;
    
#ifdef OSG_CAL_BYTE_BUFFERS
    typedef GLubyte MatrixIndex;
#else
    typedef GLshort MatrixIndex;
#endif
    MatrixIndex* matrixIndexBufferData = (MatrixIndex*) matrixIndexBuffer->getDataPointer();

    for ( int i = 0; i < vertexCount*4; i++ )
    {
        matrixIndexBufferData[i] = static_cast< MatrixIndex >( floatMatrixIndexBuffer[i] );
    }

    delete[] floatMatrixIndexBuffer;

#ifdef OSG_CAL_BYTE_BUFFERS
    GLbyte* normals = (GLbyte*) normalBuffer->getDataPointer();

    for ( int i = 0; i < vertexCount*3; i++ )
    {
        normals[i]  = static_cast< GLbyte >( floatNormalBuffer[i]*127.0 );
    }

    delete[] floatNormalBuffer;
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

        MeshData* m = new MeshData;
        
        m->name = calCoreModel->getCoreMesh( hardwareMesh->meshId )->getName();
        m->coreMaterial = hardwareMesh->pCoreMaterial;

        int indexesCount = faceCount * 3;
        int startIndex = calHardwareModel->getStartIndex();

        m->indexBuffer = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES, indexesCount );

        memcpy( &m->indexBuffer->front(),
                (GLuint*)&indexBuffer->front() + startIndex,
                indexesCount * sizeof ( GLuint ) );

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

        m->boundingBox = calculateBoundingBox( m->vertexBuffer.get() );

        m->bonesIndices = hardwareMesh->m_vectorBonesIndices;

        checkRigidness( m );
        checkForEmptyTexCoord( m );
        generateTangentAndHandednessBuffer( m );

        meshes.push_back( m );
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
    BT_INDEX,
    BT_VERTEX,
    BT_WEIGHT,
    BT_MATRIX_INDEX,
    BT_NORMAL,
    BT_TEX_COORD,
    BT_TANGENT_AND_HANDEDNESS
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

//     printf( "reading %d mesh, buffer type = %d, buffer size = %d\n",
//             meshIndex, bufferType, bufferSize );
//     fflush( stdout );
    
    switch ( bufferType )
    {
        case BT_INDEX:
            m->indexBuffer = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES, bufferSize );
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

static const int HW_MODEL_FILE_VERSION = 0xCA3D0002;

void
loadMeshes( const std::string&  fn,
            const CalCoreModel* calCoreModel,
            MeshesVector& meshes )
    throw (std::runtime_error)
{
    FILE* f = fopen( fn.c_str(), "rb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't open " + fn );
    }

    FileCloser closeOnExit( f );
    FileBuffer setReadBufferOfSize( f, 1*1024*1024 );

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
    throw (std::runtime_error)
{
    FILE* f = fopen( fn.c_str(), "wb" );

    if ( f == NULL )
    {
        throw std::runtime_error( "Can't create " + fn );
    }

    FileCloser closeOnExit( f );
    FileBuffer setReadBufferOfSize( f, 1*1024*1024 );

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
        WRITE_I32( BT_##_bufferType );          \
        WRITE_I32( m->_buffer->size() );        \
        WRITE( m->_buffer )                     \
    }

    // -- Write resident mesh buffers --
    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        MeshData* m = meshes[i].get();

        WRITE_BUFFER( INDEX, indexBuffer );
        WRITE_BUFFER( VERTEX, vertexBuffer );
        WRITE_BUFFER( WEIGHT, weightBuffer );
        WRITE_BUFFER( MATRIX_INDEX, matrixIndexBuffer );
    }

    // -- Write mesh buffers that will be freed after display list created --
    for ( size_t i = 0; i < meshes.size(); i++ )
    {
        MeshData* m = meshes[i].get();

        WRITE_BUFFER ( NORMAL, normalBuffer );
        WRITE_BUFFER ( TEX_COORD, texCoordBuffer );
        WRITE_BUFFER ( TANGENT_AND_HANDEDNESS, tangentAndHandednessBuffer );
    }

#undef WRITE_BUFFER
}


// -- CoreModel loading --

CalCoreModel*
loadCoreModel( const std::string& cfgFileName,
               float& scale,
               bool ignoreMeshes )
    throw (std::runtime_error)
{
    // -- Initial loading of model --
    scale = 1.0f;
    bool bScale = false;

    FILE* f = fopen( cfgFileName.c_str(), "r" );
    if( !f )
    {
        throw std::runtime_error( "Can't open " + cfgFileName );
    }

    std::auto_ptr< CalCoreModel > calCoreModel( new CalCoreModel( "dummy" ) );

    // Extract path from fileName
    std::string dir = osgDB::getFilePath( cfgFileName );

    static const int LINE_BUFFER_SIZE = 4096;
    char buffer[LINE_BUFFER_SIZE];

    while ( fgets( buffer, LINE_BUFFER_SIZE,f ) )
    {
        // Ignore comments or empty lines
        if ( *buffer == '#' || *buffer == 0 )
            continue;

        char* equal = strchr( buffer, '=' );
        if ( equal )
        {
            // Terminates first token
            *equal++ = 0;
            // Removes ending newline ( CR & LF )
            {
                int last = strlen( equal ) - 1;
                if ( equal[last] == '\n' ) equal[last] = 0;
                if ( last > 0 && equal[last-1] == '\r' ) equal[last-1] = 0;
            }

            // extract file name. all animations, meshes and materials names
            // are taken from file name without extension
            std::string nameToLoad;            
            char* point = strrchr( equal, '.' );
            if ( point )
            {
                nameToLoad = std::string( equal, point );
            }
            else
            {
                nameToLoad = equal;
            }
                
            std::string fullpath = dir + "/" + std::string( equal );

            // process .cfg parameters
            if ( !strcmp( buffer, "scale" ) ) 
            { 
                bScale	= true;
                scale	= atof( equal );
                continue;
            }

            if ( !strcmp( buffer, "skeleton" ) ) 
            {
                if( !calCoreModel->loadCoreSkeleton( fullpath ) )
                {
                    throw std::runtime_error( "Can't load skeleton: "
                                             + CalError::getLastErrorDescription() );
                }                
            } 
            else if ( !strcmp( buffer, "animation" ) )
            {
                int animationId = calCoreModel->loadCoreAnimation( fullpath );
                if( animationId < 0 )
                {
                    throw std::runtime_error( "Can't load animation " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreAnimation(animationId)->setName( nameToLoad );
            }
            else if ( !strcmp( buffer, "mesh" ) )
            {
                if ( ignoreMeshes )
                {
                    continue; // we don't need meshes since VBO data is already loaded from cache
                }

                int meshId = calCoreModel->loadCoreMesh( fullpath );
                if( meshId < 0 )
                {
                    throw std::runtime_error( "Can't load mesh " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                }
                calCoreModel->getCoreMesh( meshId )->setName( nameToLoad );

                // -- Remove zero influence vertices --
                // warning: this is a temporary workaround and subject to remove!
                // (this actually must be fixed in blender exporter)
                CalCoreMesh* cm = calCoreModel->getCoreMesh( meshId );

                for ( int i = 0; i < cm->getCoreSubmeshCount(); i++ )
                {
                    CalCoreSubmesh* sm = cm->getCoreSubmesh( i );

                    std::vector< CalCoreSubmesh::Vertex >& v = sm->getVectorVertex();

                    for ( size_t j = 0; j < v.size(); j++ )
                    {
                        std::vector< CalCoreSubmesh::Influence >& infl = v[j].vectorInfluence;

                        for ( size_t ii = 0; ii < infl.size(); ii++ )
                        {
                            if ( infl[ii].weight == 0 )
                            {
                                infl.erase( infl.begin() + ii );
                                --ii;
                            }
                        }
                    }
                }
            }
            else if ( !strcmp( buffer, "material" ) )  
            {
                int materialId = calCoreModel->loadCoreMaterial( fullpath );

                if( materialId < 0 ) 
                {
                    throw std::runtime_error( "Can't load material " + nameToLoad + ": "
                                             + CalError::getLastErrorDescription() );
                } 
                else 
                {
                    calCoreModel->createCoreMaterialThread( materialId ); 
                    calCoreModel->setCoreMaterialId( materialId, 0, materialId );

                    CalCoreMaterial* material = calCoreModel->getCoreMaterial( materialId );
                    material->setName( nameToLoad );
                }
            }
        }
    }

    // scaling must be done after everything has been created
    if( bScale )
    {
        calCoreModel->scale( scale );
    }
    fclose( f );

    return calCoreModel.release();
}

}
