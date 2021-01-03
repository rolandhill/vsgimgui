#include "VSGImGui.h"
#include "VSGImGuiEventHandler.h"
#include "imgui/imgui.h"

#include <vsg/all.h>

struct Params
{
    bool showGui = true;         // you can toggle this with your own EventHandler and key
    bool showDemoWindow = false;
    bool showSecondWindow = false;
    float clearColor[3] { 0.2f, 0.2f, 0.4f }; // Unfortunately, this doesn't change dynamically in vsg
    uint32_t counter = 0;
    float dist = 0.f;
};

class MyRenderCallback
{
    public:
        MyRenderCallback( Params &params ):
            _params(params)
        {}

        // Example here taken from the Dear imgui comments (mostly)
        void operator()()
        {
            // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
            if( _params.showGui )
            {
                ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

                ImGui::Text("Some useful message here.");               // Display some text (you can use a format strings too)
                ImGui::Checkbox("Demo Window", &_params.showDemoWindow);      // Edit bools storing our window open/close state
                ImGui::Checkbox("Another Window", &_params.showSecondWindow);
                ImGui::SliderFloat("float", &_params.dist, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit3("clear color", (float*)&_params.clearColor); // Edit 3 floats representing a color

                if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    _params.counter++;

                ImGui::SameLine();
                ImGui::Text("counter = %d", _params.counter);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
            }

            // 3. Show another simple window.
            if( _params.showSecondWindow )
            {
                ImGui::Begin("Another Window", &_params.showSecondWindow);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    _params.showSecondWindow = false;
                ImGui::End();
            }
        }


    private:
        Params &_params;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgviewer";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    arguments.read(options);

    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});

    // enable required device features.
    windowTraits->deviceFeatures.samplerAnisotropy = VK_TRUE;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    try 
    {
        auto vsg_scene = vsg::Group::create();

        auto nodeTx = vsg::MatrixTransform::create();
        if (auto node = vsg::read_cast<vsg::Node>("teapot.vsgt"); node) 
        {
            nodeTx->addChild( node );
            vsg_scene->addChild(nodeTx);
        }


        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout<<"Could not create windows."<<std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
        double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

        // These are set statically because the geometry in the class is expanded in the shader
        double nearFarRatio = 0.01;

        // set up the camera
        //auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, 0.0, radius*3.5), centre, vsg::dvec3(0.0, 1.0, 0.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 400.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // The commandGraph will contain a 3 stage renderGraph 1) 3D scene 2) clear depth buffers 3) ImGui
        auto commandGraph = vsg::CommandGraph::create(window);
        auto renderGraph = vsg::RenderGraph::create(window);
        commandGraph->addChild(renderGraph);

        // create the normal 3D view of the scene
        auto view1 = vsg::View::create(camera, vsg_scene);
        renderGraph->addChild(view1);

        // clear the depth buffer before view2 gets rendered
        VkClearAttachment attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, VkClearValue{1.0f, 0.0f}};
        VkClearRect rect{VkRect2D{VkOffset2D{0, 0}, VkExtent2D{window->extent2D().width, window->extent2D().height}}, 0, 1};
        auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect});
        renderGraph->addChild(clearAttachments);


        // ********** Create the ImGui node and add it to the renderGraph  ************
        auto gui = vsgImGui::create(window);
        renderGraph->addChild(gui);

        Params params;
        gui->setRenderCallback( MyRenderCallback( params ) );
        // ***************************************

        // ********** Add the ImGui event handler first to handle events early  **************
        viewer->addEventHandler(VSGImGuiEventHandler::create(gui));
        // ***************************************

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));


        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});


        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() )
        {
            viewer->handleEvents();

            {
                gui->setShowDemoWindow( params.showDemoWindow );

                vsg::dmat4 M;
                camera->getViewMatrix()->get(M);
                vsg::dmat4 Mi = vsg::inverse(M);
                nodeTx->setMatrix( Mi * vsg::translate( 0.0, 0.0, params.dist * -100.0 )  * M );
            }

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }
    }
    catch( vsg::Exception &ve )
    {
        std::cerr << "[Exception] - " << ve.message << std::endl;
    }

    return 0;
}
