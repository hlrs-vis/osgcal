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
#include <cal3d/model.h>

#include <osg/Notify>
#include <osg/BlendFunc>
#include <osg/Image>
#include <osg/Material>
#include <osg/NodeCallback>
#include <osg/ShapeDrawable>
#include <osg/TexEnv>
#include <osg/TexGen>
#include <osg/TexEnv>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/Timer>
#include <osg/MatrixTransform>
#include <osg/io_utils>
#include <osgUtil/GLObjectsVisitor>

#include <osgCal/Model>
#include <osgCal/SubMeshHardware>
#include <osgCal/SubMeshSoftware>

using namespace osgCal;

class CalUpdateCallback: public osg::NodeCallback 
{
    public:

        CalUpdateCallback()
            : previous(0)
            , prevTime(0)
        {}

        virtual void operator()( osg::Node*        node,
                                 osg::NodeVisitor* nv )
        {
            Model* model = dynamic_cast< Model* >( node );

            if ( previous == 0 )
            {
                previous = timer.tick();
            }
            
            double deltaTime = 0;

            if (!nv->getFrameStamp())
            {
                osg::Timer_t current = timer.tick();
                deltaTime = timer.delta_s(previous, current);
                previous = current;
            }
            else
            {
                double time = nv->getFrameStamp()->getSimulationTime();
                deltaTime = time - prevTime;
                prevTime = time;
            }

            //std::cout << "CalUpdateCallback: " << deltaTime << std::endl;
            if ( deltaTime > 0.0 )
            {
                model->update( deltaTime );
            }
            //std::cout << "CalUpdateCallback: ok" << std::endl;

            traverse(node, nv); // may be needed for user nodes
//            node->dirtyBound(); <- is it necessary?
        }

    private:

        osg::Timer timer;
        osg::Timer_t previous;
        double prevTime;

};

Model::Model()
{
//    setThreadSafeRefUnref( true ); 
//    ^ not needed, since the model is not shared data
}

Model::Model( const Model&, const osg::CopyOp& )
    : Group() // to eliminate warning
{
    throw std::runtime_error( "Model copying is not supported" );
}

Model::~Model()
{
    setUpdateCallback( 0 );
}

// static
// osg::Drawable*
// linesDrawable( osg::Vec3Array* lines,
//                const osg::Vec3& color )
// {
//     osg::Geometry* g = new osg::Geometry;
//     osg::Vec3Array* vcolor = new osg::Vec3Array;
//     vcolor->push_back( color );
//     g->setVertexArray( lines );
//     g->setColorArray( vcolor );
//     g->setColorBinding( osg::Geometry::BIND_OVERALL );
//     g->addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::LINES, 0, lines->size() ) );
//     g->getOrCreateStateSet()->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);;
//     return g;
// }

void
Model::load( CoreModel* _coreModel,
             MeshTyper* meshTyper,
             MeshFilter* meshFilter )
{
    if ( modelData.valid() )
    {
        throw std::runtime_error( "Model already load" );
    }

    coreModel = _coreModel;

    CalModel* calModel = new CalModel( coreModel->getCalCoreModel() );
    calModel->update( 0 );

    modelData = new ModelData( calModel );
    //modelData->setThreadSafeRefUnref( true );

    setUpdateCallback( new CalUpdateCallback() );
//    setUserData( cm ); // <- maybe this helps to not FLATTEN_STATIC_TRANSFORMS?

    if ( meshTyper == 0 )
    {
        meshTyper = new AllMeshesHardware();
    }
    else
    {
        meshTyper->ref();
    }

    // -- Process meshes --
    
    for ( size_t i = 0; i < coreModel->getMeshes().size(); i++ )
    {
        const CoreModel::Mesh* mesh = coreModel->getMeshes()[i].get();

        if ( meshFilter != NULL && meshFilter->filter( *mesh ) == false )
        {
            continue;
        }

        addMesh( mesh, meshTyper->type( *mesh ),
                 coreModel->getFlags() & CoreModel::USE_DEPTH_FIRST_MESHES );
    }

    meshTyper->unref();

//     // -- TBN debug --
//     if ( coreModel->getFlags() & CoreModel::SHOW_TBN )
//     {
//         osg::Vec3Array* t = new osg::Vec3Array;
//         osg::Vec3Array* b = new osg::Vec3Array;
//         osg::Vec3Array* n = new osg::Vec3Array;
//         const NormalBuffer& vb = *(coreModel->getVertexBuffer());
//         const NormalBuffer& tb = *(coreModel->getTangentBuffer());
//         const NormalBuffer& bb = *(coreModel->getBinormalBuffer());
//         const NormalBuffer& nb = *(coreModel->getNormalBuffer());
//         const float scale = 1.0f;

//         for ( size_t i = 0; i < vb.size(); i++ )
//         {
//             if ( tb[i].length2() > 0 )
//             {
//                 t->push_back( vb[i] );
//                 b->push_back( vb[i] );
//                 n->push_back( vb[i] );
//                 t->push_back( vb[i] + tb[i]*scale );
//                 b->push_back( vb[i] + bb[i]*scale );
//                 n->push_back( vb[i] + nb[i]*scale );
//             }
//         }

//         geode->addDrawable( linesDrawable( t, osg::Vec3( 1.0, 0.0, 0.0 ) ) );
//         geode->addDrawable( linesDrawable( b, osg::Vec3( 0.0, 1.0, 0.0 ) ) );
//         geode->addDrawable( linesDrawable( n, osg::Vec3( 0.0, 0.0, 1.0 ) ) );
//     }

    updatableMeshes.swap( updatableMeshes ); // trim vector
}


void
Model::addMesh( const CoreModel::Mesh* mesh,
                MeshType meshType,
                bool useDepthFirstMesh )
{
    osg::Drawable* g = 0;
    osg::Drawable* depthSubMesh = 0;

    // -- Create mesh drawable --
    switch ( meshType )
    {
        case MT_HARDWARE:
        {
            SubMeshHardware* smhw = new SubMeshHardware( coreModel.get(), modelData.get(), mesh,
                                                         useDepthFirstMesh );

            g = smhw;
            depthSubMesh = smhw->getDepthSubMesh();

            // -- Add shader state sets for compilation --
            usedStateSets[ mesh->stateSets->staticHardware.get() ] = true;

            if ( !mesh->data->rigid )
            {
                usedStateSets[ mesh->stateSets->hardware.get() ] = true;
            }

            if ( depthSubMesh )
            {
                usedStateSets[ mesh->stateSets->staticDepthOnly.get() ] = true;
                if ( !mesh->data->rigid )
                {
                    usedStateSets[ mesh->stateSets->depthOnly.get() ] = true;
                }
            }
            break;
        }

        case MT_SOFTWARE:
            g = new SubMeshSoftware( coreModel.get(), modelData.get(), mesh );
            usedStateSets[ mesh->stateSets->software.get() ] = true;
            break;

        default:
            throw std::runtime_error( "Model::addMesh - unknown mesh type" );
    }

    // -- Remember drawable --
    g->setName( mesh->data->name ); // for debug only, TODO: subject to remove
    meshes[ mesh->data->name ].push_back( g );

    // -- Setup data variance --
    if ( mesh->data->rigid )
    {
        g->setDataVariance( STATIC );
    }
    else
    {            
        g->setDataVariance( DYNAMIC );
        // ^ No drawing during updates. Otherwise there will be a
        // crash in multithreaded osgCalViewer modes
        // (first reported by Jan Ciger)
    }

    if ( depthSubMesh )
    {
        depthSubMesh->setDataVariance( g->getDataVariance() );
    }

    // -- Remember Updatable (and update) --
    if ( mesh->data->rigid == false )
    {
        Updatable* u = dynamic_cast< Updatable* >( g );
        u->update();
        updatableMeshes.push_back( u );
    }

    // -- Add to geode or bone matrix transform --
    if ( mesh->data->rigid == false // deformable
         || (mesh->data->rigid && mesh->data->rigidBoneId == -1) // unrigged
        )
    {
        if ( !geode.valid() )
        {
            geode = new osg::Geode;
            addChild( geode.get() );
        }
        
        geode->addDrawable( g );
        if ( depthSubMesh )
        {
            geode->addDrawable( depthSubMesh );
        }
    }
    else
    {
        TransformAndGeode& tg = getOrCreateTransformAndGeode( mesh->data->rigidBoneId );

        if ( tg.second == NULL ) // no geode created yet
        {
            tg.second = new osg::Geode;
            tg.first->addChild( tg.second );
        }

        tg.second->addDrawable( g );
        if ( depthSubMesh )
        {
            tg.second->addDrawable( depthSubMesh );
        }
    }
}

Model::TransformAndGeode&
Model::getOrCreateTransformAndGeode( int boneId )
{
    // for rigged rigid meshes we use bone
    // transform w/o per-vertex transformations
    RigidTransformsMap::iterator
        boneMT = rigidTransforms.find( boneId );

    if ( boneMT != rigidTransforms.end() )
    {
        // use ready bone matrix transform
        return boneMT->second;
    }
    else
    {
        // create new matrix transform for bone
        osg::MatrixTransform* mt = new osg::MatrixTransform;

        mt->setDataVariance( DYNAMIC );            
        mt->setMatrix( modelData->getBoneMatrix( boneId ) );

        addChild( mt );

        return rigidTransforms.insert(
            RigidTransformsMap::value_type( boneId,
                                            std::make_pair( mt, (osg::Geode*)NULL ) )
            ).first->second;
        // we only create MatrixTransform here, no geode is added, hence NULL
    }
}

void
Model::addNode( int boneId,
                osg::Node* node )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );
    tg.first->addChild( node );
}

void
Model::addDrawable( int boneId,
                    osg::Drawable* drawable )
{
    TransformAndGeode& tg = getOrCreateTransformAndGeode( boneId );

    if ( tg.second == NULL ) // no geode created yet
    {
        tg.second = new osg::Geode;
        tg.first->addChild( tg.second );
    }

    tg.second->addDrawable( drawable );
}

void
Model::accept( osg::NodeVisitor& nv )
{
    osgUtil::GLObjectsVisitor* glv = dynamic_cast< osgUtil::GLObjectsVisitor* >( &nv );

    if ( glv )
    {
        osg::notify( osg::INFO )
            << "compiling shaders and display lists" << std::endl;

        // -- Compile shaders --
        for ( std::map< osg::StateSet*, bool >::iterator s = usedStateSets.begin();
              s != usedStateSets.end(); ++s )
        {
            glv->apply( *(s->first) );
        }

        usedStateSets.clear();

        // -- Compile display lists and (maybe) user assigned sub-nodes --
        osg::Group::accept( nv );
    }
    else
    {
        osg::Group::accept( nv );        
    }
}

void
Model::update( double deltaTime ) 
{
    if ( modelData->update( deltaTime ) == false )
    {
        return;
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif // _OPENMP
    for ( std::vector< Updatable* >::iterator
              u    = updatableMeshes.begin(),
              uEnd = updatableMeshes.end();
          u < uEnd; ++u )
    {
        (*u)->update();
    }

    for ( RigidTransformsMap::iterator
              t    = rigidTransforms.begin(),
              tEnd = rigidTransforms.end();
          t != tEnd; ++t )
    {
        if ( modelData->getBoneParams( t->first ).changed )
        {
            t->second.first->setMatrix( modelData->getBoneMatrix( t->first ) );
        }
    }
}

void
Model::blendCycle( int id,
                   float weight,
                   float delay )
{
    modelData->getCalMixer()->blendCycle( id, weight, delay );
}

void
Model::clearCycle( int id,
                   float delay )
{
    modelData->getCalMixer()->clearCycle( id, delay );
}

const Model::MeshesList&
Model::getMeshes( const std::string& name ) const throw (std::runtime_error)
{
    MeshMap::const_iterator i = meshes.find( name );

    if ( i != meshes.end() )
    {
        return i->second;
    }
    else
    {
        throw std::runtime_error( "Model::getMeshes - can't find mesh \"" + name + "\"" );
    }
}


// -- ModelData --

ModelData::ModelData( CalModel* cm )
    : calModel( cm )
    , calMixer( (CalMixer*)cm->getAbstractMixer() )
{
    const std::vector< CalBone* >& vectorBone = calModel->getSkeleton()->getVectorBone();

    bones.resize( vectorBone.size() );

    std::vector< CalBone* >::const_iterator b = vectorBone.begin();        
    for ( BoneParamsVector::iterator
              bp = bones.begin(),
              bpEnd = bones.end();
          bp < bpEnd; ++bp, ++b )
    {
        bp->bone = *b;
    }
}

ModelData::~ModelData()
{
    delete calModel;
}

inline
float
square( float x )
{
    return x*x;
}

bool
ModelData::update( float deltaTime )
{
    // -- Update calMixer & skeleton --
    if ( //calMixer->getAnimationVector().size() == <total animations count>
         calMixer->getAnimationActionList().size() == 0 &&
         calMixer->getAnimationCycle().size() == 0 )
    {
        return false; // no animations, nothing to update
    }

    calMixer->updateAnimation( deltaTime ); 
    calMixer->updateSkeleton();

    // -- Update bone parameters --
    bool anythingChanged = false;
    for ( BoneParamsVector::iterator
              b    = bones.begin(),
              bEnd = bones.end();
          b < bEnd; ++b )
    {
        const CalQuaternion& rotation = b->bone->getRotationBoneSpace();
        const CalVector&     translation = b->bone->getTranslationBoneSpace();
        const CalMatrix&     rm = b->bone->getTransformMatrix();

        const osg::Matrix3   r( rm.dxdx, rm.dydx, rm.dzdx,
                                rm.dxdy, rm.dydy, rm.dzdy,
                                rm.dxdz, rm.dydz, rm.dzdz );
        const osg::Vec3f     t( translation.x, translation.y, translation.z );

        // -- Check for deformed --
        b->deformed =
            // cal3d reports nonzero translations for non-animated models
            // and non zero quaternions (seems like some FP round-off error). 
            // So we must check for deformations using some epsilon value.
            // Problem:
            //   * It is cal3d that must return correct values, no epsilons
            // But nevertheless we use this to reduce CPU load.

            t.length() > /*boundingBox.radius() **/ 1e-5 // usually 1e-6 .. 1e-7
            ||
            osg::Vec3d( rotation.x,
                        rotation.y,
                        rotation.z ).length() > 1e-6 // usually 1e-7 .. 1e-8
            ;

        // -- Check for changes --
        float s = 0;
        for ( int j = 0; j < 9; j++ )
        {
            s += square( r[j] - b->rotation[j] );
        }
        s += ( t - b->translation ).length2();

        if ( s < 1e-7 ) // usually 1e-11..1e-12
        {
            b->changed = false;
        }
        else
        {
            b->changed = true;
            anythingChanged = true;
            b->rotation = r;
            b->translation = t;
        }
        
//         std::cout << "bone: " << b->bone->getCoreBone()->getName() << std::endl
//                   << "quaternion: "
//                   << rotation.x << ' '
//                   << rotation.y << ' '
//                   << rotation.z << ' '
//                   << rotation.w << std::endl
//                   << "translation: "
//                   << t.x() << ' ' << t.y() << ' ' << t.z()
//                   << "; len = " << t.length()
//                   << "; s = " << s // << std::endl
//                   << "; changed = " << b->changed // << std::endl
//                   << "; deformed = " << b->deformed // << std::endl
// //                  << "len / bbox.radius = " << translation.length() / boundingBox.radius()
//                   << std::endl;
    }

    return anythingChanged;
}
