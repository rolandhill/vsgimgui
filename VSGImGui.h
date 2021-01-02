#ifndef VSG_IMGUI_DEF
#define VSG_IMGUI_DEF
#include <vsg/viewer/Window.h>
#include <functional>

class VSG_DECLSPEC vsgImGui: public vsg::Inherit<vsg::Command, vsgImGui>
{
    public:
        vsgImGui( const vsg::ref_ptr<vsg::Window> &window );
        virtual ~vsgImGui();

        using RenderCallback = std::function<void()>;
        void setRenderCallback(const RenderCallback &callback);

        void render() const;

        void record(vsg::CommandBuffer& commandBuffer) const override;

        void setShowDemoWindow(bool);
        bool getShowDemoWindow() const;

    private:
        VkInstance _instance;
        VkDevice _device;
        uint32_t _queueFamily;
        VkQueue _queue;
        VkDescriptorPool _descriptorPool;
        VkCommandPool _commandPool;
        RenderCallback _renderCallback;
        bool           _showDemoWindow = false;


        void _init( const vsg::ref_ptr<vsg::Window> &window );
        void _uploadFonts( const vsg::ref_ptr<vsg::Window> &window );
};

namespace vsg
{
    VSG_type_name(vsgImGui);
}

#endif
