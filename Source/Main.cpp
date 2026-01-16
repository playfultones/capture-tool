#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

//==============================================================================
// Menu command IDs
namespace CommandIDs
{
    enum
    {
        newProject  = 0x100001,
        openProject = 0x100002
    };
}

//==============================================================================
class ReferenceCaptureApplication : public juce::JUCEApplication,
                                    public juce::MenuBarModel
{
public:
    const juce::String getApplicationName() override
    {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override
    {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        
        // Set up the native menu bar on macOS
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(this);
#endif
    }

    void shutdown() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    //==============================================================================
    // MenuBarModel implementation
    
    juce::StringArray getMenuBarNames() override
    {
        return { "File" };
    }
    
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& /*menuName*/) override
    {
        juce::PopupMenu menu;
        
        if (menuIndex == 0) // File menu
        {
            menu.addItem(CommandIDs::newProject, "New Project", true, false);
            menu.addItem(CommandIDs::openProject, "Open Project...", true, false);
        }
        
        return menu;
    }
    
    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (mainWindow == nullptr)
            return;
        
        auto* mainComponent = mainWindow->getMainComponent();
        if (mainComponent == nullptr)
            return;
        
        switch (menuItemID)
        {
            case CommandIDs::newProject:
                mainComponent->performMenuAction("newProject");
                break;
                
            case CommandIDs::openProject:
                mainComponent->performMenuAction("openProject");
                break;
                
            default:
                break;
        }
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            mainComponent = new MainComponent();
            setContentOwned(mainComponent, true);
            setResizable(true, true);
            
            // Set constraints to maintain aspect ratio and minimum size
            constrainer.setFixedAspectRatio(defaultWidth / static_cast<double>(defaultHeight));
            constrainer.setMinimumSize(minWidth, minHeight);
            setConstrainer(&constrainer);
            
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        
        MainComponent* getMainComponent() const { return mainComponent; }

    private:
        // Default window dimensions (must match MainComponent::setSize)
        static constexpr int defaultWidth = 1024;
        static constexpr int defaultHeight = 768;
        
        // Minimum window size (half of default, maintains aspect ratio)
        static constexpr int minWidth = 512;
        static constexpr int minHeight = 384;
        
        MainComponent* mainComponent = nullptr;
        juce::ComponentBoundsConstrainer constrainer;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(ReferenceCaptureApplication)
