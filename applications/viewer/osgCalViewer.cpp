/*
 * Copyright (C) 2006 Vladimir Shabanov <vshabanoff@gmail.com>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */
#include <osgViewer/Viewer>
#include <osg/MatrixTransform>
#include <osg/CullFace>
#include <osg/Light>
#include <osg/Notify>
#include <osg/ShadeModel>
#include <osg/LightModel>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include <osgGA/TrackballManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osg/Texture2D>

#include <osgViewer/Viewer>
#include <osgViewer/StatsHandler>
#include <osgViewer/HelpHandler>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>

#include <osgCal/CoreModel>
#include <osgCal/Model>

osg::Node*
makeModel( osgCal::CoreModel* cm,
           osgCal::MeshTyper* mtf,
           osgCal::MeshFilter* mf,                
           int animNum = -1 )
{
    osgCal::Model* model = new osgCal::Model();

    model->load( cm, mtf, mf );

    if ( animNum != -1 )
    {
        model->getCalModel()->getMixer()->blendCycle(animNum, 1.0f, 0);
    }

    return model;
}

template < typename T >
T normalize( const T& v )
{
    T r( v );
    r.normalize();
    return r;
}

int
main( int argc,
      const char** argv )
{
    osg::setNotifyLevel( osg::DEBUG_FP );

    osg::Group* root = new osg::Group();
    
    // -- Load model --
    { // scope for model ref_ptr
        osg::ref_ptr< osgCal::CoreModel > coreModel( new osgCal::CoreModel() );
        int         animNum = -1;
        osg::ref_ptr< osgCal::MeshFilter > meshFilter = 0;

        try
        {
            if ( argc > 1 )
            {
                std::string fn = argv[1];
                std::string ext = osgDB::getLowerCaseFileExtension( fn );
                std::string dir = osgDB::getFilePath( fn );
                std::string name = osgDB::getStrippedName( fn );

                if ( dir == "" )
                {
                    dir = ".";
                }

                if ( ext == "caf" )
                {
                    coreModel->load( dir + "/cal3d.cfg" );

                    for ( size_t i = 0; i < coreModel->getAnimationNames().size(); i++ )
                    {
                        if ( coreModel->getAnimationNames()[i] == name )
                        {
                            animNum = i;
                            break;
                        }
                    }

                    if ( animNum == -1 ) 
                    {
                        // animation is absent in cal3d.cfg, so load it manually
                        CalCoreModel* cm = coreModel->getCalCoreModel();

                        std::cout << coreModel->getScale() << std::endl;
                        if ( coreModel->getScale() != 1 )
                        {
                            // to eliminate scaling of the model by non-scaled anumation
                            // we scale model back, load animation, and rescale one more time
                            cm->scale( 1.0 / coreModel->getScale() );
                        }

                        animNum = cm->loadCoreAnimation( fn );

                        if ( coreModel->getScale() != 1 )
                        {
                            cm->scale( coreModel->getScale() );
                        }
                    }
                }
                else if ( ext == "cmf" )
                {
                    coreModel->load( dir + "/cal3d.cfg" );
                    meshFilter = new osgCal::OneMesh( osgDB::getStrippedName( fn ) );
                }
                else
                {
                    coreModel->load( fn );
                }
            }
            else
            {
                std::cout << "Usage:\n"
                          << "  osgCalViewer <cal3d>.cfg\n"
                          << "  osgCalViewer <mesh-name>.cmf\n"
                          << "  osgCalViewer <animation-name>.caf" << std::endl;
                return 0;
            }
        }
        catch ( std::runtime_error& e )
        {
            std::cout << "runtime error during load:" << std::endl
                      << e.what() << std::endl;
            return EXIT_FAILURE;
        }

                                    
//        osg::ref_ptr< osgCal::AllMeshesSoftware > amhw = new osgCal::AllMeshesSoftware;
        osg::ref_ptr< osgCal::AllMeshesHardware > amhw = new osgCal::AllMeshesHardware;
            
        root->addChild( makeModel( coreModel.get(),
                                   amhw.get(),
                                   meshFilter.get(),
                                   animNum ) );
    } // end of model's ref_ptr scope

    // -- Setup viewer --
    osgViewer::Viewer viewer;

    // set up the camera manipulators.
    {
        osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator = new osgGA::KeySwitchMatrixManipulator;

        keyswitchManipulator->addMatrixManipulator( '1', "Trackball", new osgGA::TrackballManipulator() );
        keyswitchManipulator->addMatrixManipulator( '2', "Flight", new osgGA::FlightManipulator() );
        keyswitchManipulator->addMatrixManipulator( '3', "Drive", new osgGA::DriveManipulator() );
        keyswitchManipulator->addMatrixManipulator( '4', "Terrain", new osgGA::TerrainManipulator() );

//         std::string pathfile;
//         char keyForAnimationPath = '5';
//         while (arguments.read("-p",pathfile))
//         {
//             osgGA::AnimationPathManipulator* apm = new osgGA::AnimationPathManipulator(pathfile);
//             if (apm || !apm->valid()) 
//             {
//                 unsigned int num = keyswitchManipulator->getNumMatrixManipulators();
//                 keyswitchManipulator->addMatrixManipulator( keyForAnimationPath, "Path", apm );
//                 keyswitchManipulator->selectMatrixManipulator(num);
//                 ++keyForAnimationPath;
//             }
//         }

        viewer.setCameraManipulator( keyswitchManipulator.get() );
    }

    // add the state manipulator
    viewer.addEventHandler( new osgGA::StateSetManipulator( viewer.getCamera()->getOrCreateStateSet() ) );
    
    // add the thread model handler
//    viewer.addEventHandler(new ThreadingHandler);

    // add the stats handler
    viewer.addEventHandler( new osgViewer::StatsHandler );

    // add the help handler
//    viewer.addEventHandler(new osgViewer::HelpHandler(arguments.getApplicationUsage()));

    viewer.realize();
    viewer.frame();

//    viewer.getCullSettings().setDefaults();
//    viewer.getCullSettings().setComputeNearFarMode( osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR );
//    viewer.getCullSettings().setCullingMode( osg::CullSettings::DEFAULT_CULLING & ~osg::CullSettings::NEAR_PLANE_CULLING);

//    root->getOrCreateStateSet()->setAttributeAndModes( new osg::CullFace, osg::StateAttribute::ON );
    // turn on back face culling

// light is already smooth by default
//    osg::ShadeModel* shadeModel = new osg::ShadeModel;
//    shadeModel->setMode( osg::ShadeModel::SMOOTH );
//    root->getOrCreateStateSet()->setAttributeAndModes( shadeModel, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE );

     osg::LightModel* lightModel = new osg::LightModel();
//    lightModel->setColorControl( osg::LightModel::SINGLE_COLOR );
//    lightModel->setColorControl( osg::LightModel::SEPARATE_SPECULAR_COLOR );
    lightModel->setAmbientIntensity( osg::Vec4(0.1,0.1,0.1,1) );
    root->getOrCreateStateSet()->setAttributeAndModes( lightModel, osg::StateAttribute::ON );

    osg::Light* light = new osg::Light();
    light->setLightNum( 0 );
    light->setAmbient( osg::Vec4( 0, 0, 0, 1 ) );
    light->setDiffuse( osg::Vec4( 0.8, 0.8, 0.8, 1 ) );
    light->setSpecular( osg::Vec4( 1, 1, 1, 0 ) );
    // as in SceneView, except direction circa as in 3DSMax
    light->setPosition( normalize(osg::Vec4( 0.15, 0.4, 1, 0 )) ); // w=0 - Directional
    light->setDirection( osg::Vec3( 0, 0, 0 ) );  // Direction = (0,0,0) - Omni light

    osg::LightSource* lightSource = new osg::LightSource();
    lightSource->setLight( light );
    lightSource->setReferenceFrame( osg::LightSource::ABSOLUTE_RF );
    lightSource->addChild( root );

    viewer.setSceneData(/*root*/lightSource);
//    root->getOrCreateStateSet()->setMode( GL_LIGHTING,osg::StateAttribute::ON );
//    root->getOrCreateStateSet()->setAttributeAndModes( light, osg::StateAttribute::ON );

//    osg::Light* light = (osg::Light*)
//        root->getOrCreateStateSet()->getAttribute( osg::StateAttribute::LIGHT );
//    std::cout << "light: " << light << std::endl;
//    viewer.getEventHandlerList().push_back( new osgGA::TrackballManipulator() );  

    return viewer.run();
}
