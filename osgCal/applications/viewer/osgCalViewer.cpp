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
#include <osg/ArgumentParser>
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
#include <osgUtil/GLObjectsVisitor>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

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
        model->blendCycle( animNum, 1.0f, 0 );
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

/**
 * Add window in multi-window setup.
 */
void
addWindow( osgViewer::Viewer& viewer,
           int x,
           int y,
           int width,
           int height,
           float xTranslate,
           float yTranslate )
{
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = x;
    traits->y = y;
    traits->width = width;
    traits->height = height;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());

    osg::ref_ptr<osg::Camera> camera = new osg::Camera;
    camera->setGraphicsContext(gc.get());
    camera->setViewport(new osg::Viewport(0,0, traits->width, traits->height));
    GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
    camera->setDrawBuffer(buffer);
    camera->setReadBuffer(buffer);

    // add this slave camra to the viewer, with a shift left of the projection matrix
    viewer.addSlave(camera.get(), osg::Matrixd::translate( xTranslate, yTranslate, 0.0),
                    osg::Matrixd());
}

class AnimationToggleHandler : public osgGA::GUIEventHandler 
{
    public: 

        AnimationToggleHandler( osgCal::Model* m )
            : model( m )
            , animationNames( m->getCoreModel()->getAnimationNames() )
            , currentAnimation( -1 )
        {
        }
        
        bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
        {
            osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
            if (!viewer) return false;
    
            switch(ea.getEventType())
            {
                case(osgGA::GUIEventAdapter::KEYDOWN):
                {
                    if (ea.getKey() >= '1' && ea.getKey() < '1' + (int)animationNames.size())
                    {
                        if ( currentAnimation != -1 )
                        {
                            model->clearCycle( currentAnimation, 1.0 ); // clear in 1sec
                        }
                        
                        currentAnimation = ea.getKey() - '1';

                        model->blendCycle( currentAnimation, 1.0f, 1.0 );                        
                    }
                    else if ( ea.getKey() == '0' )
                    {
                        model->clearCycle( currentAnimation, 0.0 ); // clear now
                        // TODO: actually it's better to blend to idle animation
                        currentAnimation = -1;
                    }
                }
                default: break;
            }
        
            return false;
        }
    
        /** Get the keyboard and mouse usage of this manipulator.*/
        virtual void getUsage(osg::ApplicationUsage& usage) const
        {
            usage.addKeyboardMouseBinding( "0", "Stop animation" );
            
            for ( size_t i = 0; i < animationNames.size(); i++ )
            {
                char k[] = { i + '1', '\0' };
                usage.addKeyboardMouseBinding( k, animationNames[i] );
            }
        }

    private:

        osgCal::Model*              model;
        std::vector< std::string >  animationNames;
        int                         currentAnimation;

};

class ToggleHandler : public osgGA::GUIEventHandler 
{
    public: 

        ToggleHandler( bool& toggleVar,
                       char  key,
                       const std::string& help )
            : toggleVar( &toggleVar )
            , key( key )
            , help( help )
        {
        }
        
        bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
        {
            osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
            if (!viewer) return false;
    
            switch(ea.getEventType())
            {
                case(osgGA::GUIEventAdapter::KEYDOWN):
                {
                    if ( ea.getKey() == key )
                    {
                        *toggleVar = !(*toggleVar);
                    }
                }
                default: break;
            }
        
            return false;
        }
    
        /** Get the keyboard and mouse usage of this manipulator.*/
        virtual void getUsage(osg::ApplicationUsage& usage) const
        {
            usage.addKeyboardMouseBinding( std::string( 1, key ), help );
        }

    private:

        bool*       toggleVar;
        char        key;
        std::string help;
};


class CompileStateSets : public osg::Operation
{
    public:
        CompileStateSets( osg::Node* node )
            : osg::Operation( "CompileStateSets", false )
            , node( node )
        {}
        
        virtual void operator () ( osg::Object* object )
        {
            osg::GraphicsContext* context = dynamic_cast< osg::GraphicsContext* >( object );
            
            if ( context )
            {
                osg::ref_ptr< osgUtil::GLObjectsVisitor > glov = new osgUtil::GLObjectsVisitor;
                glov->setState( context->getState() );
                node->accept( *(glov.get()) );
            }
        }

    private:
        
        osg::Node* node;
};

int
main( int argc,
      char** argv )
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);

    arguments.getApplicationUsage()->setApplicationName("osgCalViewer");
    arguments.getApplicationUsage()->setDescription("osgCalViewer");
    arguments.getApplicationUsage()->setCommandLineUsage("osgCalViewer [options] cal3d.cfg ...");
    arguments.getApplicationUsage()->addCommandLineOption("--sw", "Use software skinning and fixed-function drawing");
    arguments.getApplicationUsage()->addCommandLineOption("--hw", "Use hardware (GLSL) skinning and drawing");
    arguments.getApplicationUsage()->addCommandLineOption("--no-debug", "Don't display debug information");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help","Display command line parameters");
    arguments.getApplicationUsage()->addCommandLineOption("--help-env","Display environmental variables available");
    arguments.getApplicationUsage()->addCommandLineOption("--help-all","Display all command line, env vars and keyboard & mouse bindings.");
    arguments.getApplicationUsage()->addCommandLineOption("--SingleThreaded","Select SingleThreaded threading model for viewer.");
    arguments.getApplicationUsage()->addCommandLineOption("--CullDrawThreadPerContext","Select CullDrawThreadPerContext threading model for viewer.");
    arguments.getApplicationUsage()->addCommandLineOption("--DrawThreadPerContext","Select DrawThreadPerContext threading model for viewer.");
    arguments.getApplicationUsage()->addCommandLineOption("--CullThreadPerCameraDrawThreadPerContext","Select CullThreadPerCameraDrawThreadPerContext threading model for viewer.");

    // if user request help write it out to cout.
    bool helpAll = arguments.read("--help-all");
    unsigned int helpType =
        ((helpAll || arguments.read("-h") || arguments.read("--help"))
         ? osg::ApplicationUsage::COMMAND_LINE_OPTION : 0 ) |
        ((helpAll || arguments.read("--help-env"))
         ? osg::ApplicationUsage::ENVIRONMENTAL_VARIABLE : 0 );
    
    if ( helpType )
    {
        arguments.getApplicationUsage()->write( std::cout, helpType );
        return 1;
    }

    // report any errors if they have occurred when parsing the program arguments.
    if ( arguments.errors() )
    {
        arguments.writeErrorMessages( std::cout );
        return 1;
    }
    
    std::string fn;

    // note currently doesn't delete the loaded file entries from the command line yet...
    for(int pos=1;pos<arguments.argc();++pos)
    {
        if (!arguments.isOption(pos))
        {
            fn = arguments[pos];
        }
    }

    if ( arguments.argc() <= 1 || fn == "" )
    {
        arguments.getApplicationUsage()->write( std::cout,
                                                osg::ApplicationUsage::COMMAND_LINE_OPTION );
        return 1;
    }

    if ( arguments.read( "--no-debug" ) == false )
    {
        osg::setNotifyLevel( osg::DEBUG_FP );
    }

    osg::Group* root = new osg::Group();
    
    // -- Load model --
    { // scope for model ref_ptr
        osg::ref_ptr< osgCal::CoreModel > coreModel( new osgCal::CoreModel() );
        int         animNum = -1;
        osg::ref_ptr< osgCal::MeshFilter > meshFilter = 0;

        try
        {
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
        catch ( std::runtime_error& e )
        {
            std::cout << "runtime error during load:" << std::endl
                      << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        osg::ref_ptr< osgCal::MeshTyper > meshTyper = new osgCal::AllMeshesHardware;

        while ( arguments.read( "--sw" ) ) { meshTyper = new osgCal::AllMeshesSoftware; }
        while ( arguments.read( "--hw" ) ) { meshTyper = new osgCal::AllMeshesHardware; }
            
        root->addChild( makeModel( coreModel.get(),
                                   meshTyper.get(),
                                   meshFilter.get(),
                                   animNum ) );
    } // end of model's ref_ptr scope

    // -- Setup viewer --
    osgViewer::Viewer viewer;
    
//     addWindow( viewer,   0,   0, 640, 480,  1.0, -1.0 );
//     addWindow( viewer, 640,   0, 640, 480, -1.0, -1.0 );
//     addWindow( viewer,   0, 480, 640, 480,  1.0,  1.0 );
//     addWindow( viewer, 640, 480, 640, 480, -1.0,  1.0 );

    // add the state manipulator
    viewer.addEventHandler( new osgGA::StateSetManipulator( viewer.getCamera()->getOrCreateStateSet() ) );
    
    // add the thread model handler
    viewer.addEventHandler(new osgViewer::ThreadingHandler);

    // add the full screen toggle handler
    viewer.addEventHandler( new osgViewer::WindowSizeHandler );

    // add the stats handler
    viewer.addEventHandler( new osgViewer::StatsHandler );

    // add the help handler
    viewer.addEventHandler(new osgViewer::HelpHandler(arguments.getApplicationUsage()));

    // add the animation toggle handler
    viewer.addEventHandler( new AnimationToggleHandler( (osgCal::Model*)root->getChild(0) ) );
    
    // add the pause handler
    bool paused = false;
    viewer.addEventHandler( new ToggleHandler( paused, 'p', "Pause animation" ) );

    while (arguments.read("--SingleThreaded")) viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
    while (arguments.read("--CullDrawThreadPerContext")) viewer.setThreadingModel(osgViewer::Viewer::CullDrawThreadPerContext);
    while (arguments.read("--DrawThreadPerContext")) viewer.setThreadingModel(osgViewer::Viewer::DrawThreadPerContext);
    while (arguments.read("--CullThreadPerCameraDrawThreadPerContext")) viewer.setThreadingModel(osgViewer::Viewer::CullThreadPerCameraDrawThreadPerContext);

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

    // -- light #0 --
    osg::Light* light0 = new osg::Light();
    light0->setLightNum( 0 );
    light0->setAmbient( osg::Vec4( 0, 0, 0, 1 ) );
    light0->setDiffuse( osg::Vec4( 0.8, 0.8, 0.8, 1 ) );
    light0->setSpecular( osg::Vec4( 1, 1, 1, 0 ) );
    // as in SceneView, except direction circa as in 3DSMax
    light0->setPosition( normalize(osg::Vec4( 0.15, 0.4, 1, 0 )) ); // w=0 - Directional
    light0->setDirection( osg::Vec3( 0, 0, 0 ) );  // Direction = (0,0,0) - Omni light

    osg::LightSource* lightSource0 = new osg::LightSource();
    lightSource0->setLight( light0 );
    lightSource0->setReferenceFrame( osg::LightSource::ABSOLUTE_RF );
    lightSource0->addChild( root );

//     // -- light #1 --
//     osg::Light* light1 = new osg::Light();
//     light1->setLightNum( 1 );
//     light1->setAmbient( osg::Vec4( 0, 0, 0, 1 ) );
//     light1->setDiffuse( osg::Vec4( 0.4, 0.4, 0.4, 1 ) );
//     light1->setSpecular( osg::Vec4( 1, 1, 1, 0 ) );
//     // as in SceneView, except direction circa as in 3DSMax
//     light1->setPosition( normalize(osg::Vec4( -0.15, -0.4, -1, 0 )) ); // w=0 - Directional
//     light1->setDirection( osg::Vec3( 0, 0, 0 ) );  // Direction = (0,0,0) - Omni light

//     osg::LightSource* lightSource1 = new osg::LightSource();
//     lightSource1->setLight( light1 );
//     lightSource1->setReferenceFrame( osg::LightSource::ABSOLUTE_RF );
//     lightSource1->addChild( lightSource0 );

    viewer.setSceneData(/*root*/lightSource0);
//    root->getOrCreateStateSet()->setMode( GL_LIGHTING,osg::StateAttribute::ON );
//    root->getOrCreateStateSet()->setAttributeAndModes( light, osg::StateAttribute::ON );

//    osg::Light* light = (osg::Light*)
//        root->getOrCreateStateSet()->getAttribute( osg::StateAttribute::LIGHT );
//    std::cout << "light: " << light << std::endl;
//    viewer.getEventHandlerList().push_back( new osgGA::TrackballManipulator() );

    viewer.setCameraManipulator(new osgGA::TrackballManipulator());
    viewer.setRealizeOperation( new CompileStateSets( lightSource0 ) );
    viewer.realize();

    // -- Main loop --
    osg::Timer_t startTick = osg::Timer::instance()->tick();

    enum PauseState { Unpaused, Paused };
    PauseState   pauseState = Unpaused;
    osg::Timer_t pauseStartTick = 0;
    double       totalPauseTime = 0; 

    while ( !viewer.done() )
    {
        osg::Timer_t tick = osg::Timer::instance()->tick();

        if ( pauseState == Unpaused && paused )
        {
            pauseState = Paused;
            pauseStartTick = tick;
        }
        if ( pauseState == Paused && !paused )
        {
            pauseState = Unpaused;
            totalPauseTime += osg::Timer::instance()->delta_s( pauseStartTick, tick );
        }

        double currentTime = osg::Timer::instance()->delta_s( 
            startTick,
            pauseState == Unpaused ? tick : pauseStartTick );

        viewer.frame( currentTime - totalPauseTime );
    }

    viewer.setSceneData( new osg::Group() ); // destroy scene data before viewer

    return 0;
}
