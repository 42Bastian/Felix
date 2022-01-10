#include "pch.hpp"
#include "UI.hpp"
#include "imgui.h"
#include "IInputSource.hpp"
#include "Manager.hpp"
#include "KeyNames.hpp"
#include "UserInput.hpp"
#include "ConfigProvider.hpp"
#include <imfilebrowser.h>
#include "WinAudioOut.hpp"
#include "Core.hpp"
#include "CPU.hpp"
#include "SysConfig.hpp"

UI::UI( Manager& manager ) :
  mManager{ manager },
  mOpenMenu{},
  mFileBrowser{ std::make_unique<ImGui::FileBrowser>() }
{
}

UI::~UI()
{
}

void UI::drawGui( int left, int top, int right, int bottom )
{
  ImGuiIO& io = ImGui::GetIO();

  bool hovered = io.MousePos.x > left && io.MousePos.y > top && io.MousePos.x < right&& io.MousePos.y < bottom;

  if ( hovered || mOpenMenu )
  {
    mOpenMenu = mainMenu( io );
  }

  drawDebugWindows( io );
}

bool UI::mainMenu( ImGuiIO& io )
{
  enum class FileBrowserAction
  {
    NONE,
    OPEN_CARTRIDGE,
    OPEN_BOOTROM
  };

  enum class ModalWindow
  {
    NONE,
    PROPERTIES
  };

  static ModalWindow modalWindow = ModalWindow::NONE;
  static FileBrowserAction fileBrowserAction = FileBrowserAction::NONE;
  static std::optional<KeyInput::Key> keyToConfigure;

  auto configureKeyItem = [&]( char const* name, KeyInput::Key k )
  {
    ImGui::Text( name );
    ImGui::SameLine( 60 );

    if ( ImGui::Button( keyName( mManager.mIntputSource->getVirtualCode( k ) ), ImVec2( 100, 0 ) ) )
    {
      keyToConfigure = k;
      ImGui::OpenPopup( "Configure Key" );
    }
  };

  auto sysConfig = gConfigProvider.sysConfig();

  bool openMenu = false;
  bool pauseRunIssued = false;
  bool stepInIssued = false;
  bool stepOverIssued = false;
  bool stepOutIssued = false;
  bool resetIssued = false;
  bool debugMode = mManager.mDebugger.isDebugMode();

  if ( ImGui::IsKeyPressed( VK_F3 ) )
  {
    resetIssued = true;
  }

  if ( ImGui::IsKeyPressed( VK_F4 ) )
  {
    debugMode = !debugMode;
  }

  if ( ImGui::IsKeyPressed( VK_F5 ) )
  {
    pauseRunIssued = true;
  }
  else if ( ImGui::IsKeyPressed( VK_F6 ) )
  {
    stepInIssued = true;
  }
  else if ( ImGui::IsKeyPressed( VK_F7 ) )
  {
    stepOverIssued = true;
  }
  else if ( ImGui::IsKeyPressed( VK_F8 ) )
  {
    stepOutIssued = true;
  }


  ImGui::PushStyleVar( ImGuiStyleVar_Alpha, mOpenMenu ? 1.0f : std::clamp( ( 100.0f - io.MousePos.y ) / 100.f, 0.0f, 1.0f ) );
  if ( ImGui::BeginMainMenuBar() )
  {
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, 1.0f );
    if ( ImGui::BeginMenu( "File" ) )
    {
      openMenu = true;
      if ( ImGui::MenuItem( "Open" ) )
      {
        mFileBrowser->SetTitle( "Open Cartridge image file" );
        mFileBrowser->SetTypeFilters( { ".lnx", ".lyx", ".o" } );
        mFileBrowser->Open();
        fileBrowserAction = FileBrowserAction::OPEN_CARTRIDGE;
      }
      if ( ImGui::MenuItem( "Exit", "Alt+F4" ) )
      {
        mManager.quit();
      }
      ImGui::EndMenu();
    }
    ImGui::BeginDisabled( !(bool)mManager.mImageProperties );
    if ( ImGui::BeginMenu( "Cartridge" ) )
    {
      openMenu = true;
      if ( ImGui::MenuItem( "Properties", "Ctrl+P", nullptr ) )
      {
        modalWindow = ModalWindow::PROPERTIES;
      }
      ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    if ( ImGui::BeginMenu( "Audio" ) )
    {
      openMenu = true;
      bool mute = mManager.mAudioOut->mute();
      if ( ImGui::MenuItem( "Mute", "Ctrl+M", &mute ) )
      {
        mManager.mAudioOut->mute( mute );
      }
      ImGui::EndMenu();
    }
    if ( mManager.mRenderer->canRenderBoards() )
    {
      ImGui::BeginDisabled( !(bool)mManager.mInstance );
      if ( ImGui::BeginMenu( "Debug" ) )
      {
        openMenu = true;

        if ( ImGui::MenuItem( "Reset", "F3" ) )
        {
          resetIssued = true;
        }

        ImGui::MenuItem( "Debug Mode", "F4", &debugMode );

        if ( ImGui::MenuItem( mManager.mDebugger.isPaused() ? "Run" : "Break", "F5" ) )
        {
          pauseRunIssued = true;
        }
        if ( ImGui::MenuItem( "Step In", "F6" ) )
        {
          stepInIssued = true;
        }
        if ( ImGui::MenuItem( "Step Over", "F7" ) )
        {
          stepOverIssued = true;
        }
        if ( ImGui::MenuItem( "Step Out", "F8" ) )
        {
          stepOutIssued = true;
        }
        if ( ImGui::BeginMenu( "Debug Windows" ) )
        {
          bool cpuWindow = mManager.mDebugger.isCPUVisualized();
          bool disasmWindow = mManager.mDebugger.isDisasmVisualized();
          bool historyWindow = mManager.mDebugger.isHistoryVisualized();
          if ( ImGui::MenuItem( "CPU Window", "Ctrl+C", &cpuWindow ) )
          {
            mManager.mDebugger.visualizeCPU( cpuWindow );
          }
          if ( ImGui::MenuItem( "Disassembly Window", "Ctrl+D", &disasmWindow ) )
          {
            mManager.mDebugger.visualizeDisasm( disasmWindow );
          }
          if ( ImGui::MenuItem( "History Window", "Ctrl+H", &historyWindow ) )
          {
            if ( historyWindow )
            {
              mManager.mInstance->debugCPU().enableHistory( mManager.mDebugger.historyVisualizer().columns, mManager.mDebugger.historyVisualizer().rows );
              mManager.mDebugger.visualizeHistory( true );
            }
            else
            {
              mManager.mInstance->debugCPU().disableHistory();
              mManager.mDebugger.visualizeHistory( false );
            }
          }
          ImGui::BeginDisabled( !mManager.mDebugger.isDebugMode() );
          if ( ImGui::MenuItem( "New Screen View", "Ctrl+S" ) )
          {
            mManager.mDebugger.newScreenView();
          }
          ImGui::EndDisabled();
          ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu( "Options" ) )
        {
          bool breakOnBrk = mManager.mDebugger.isBreakOnBrk();
          bool debugModeOnBreak = mManager.mDebugger.debugModeOnBreak();
          bool normalModeOnRun = mManager.mDebugger.normalModeOnRun();

          if ( ImGui::MenuItem( "Break on BRK", nullptr, &breakOnBrk ) )
          {
            mManager.mDebugger.breakOnBrk( breakOnBrk );
            mManager.mInstance->debugCPU().breakOnBrk( breakOnBrk );
          }
          if ( ImGui::MenuItem( "Debug mode on break", nullptr, &debugModeOnBreak ) )
          {
            mManager.mDebugger.debugModeOnBreak( debugModeOnBreak );
          }
          if ( ImGui::MenuItem( "Normal mode on run", nullptr, &normalModeOnRun ) )
          {
            mManager.mDebugger.normalModeOnRun( normalModeOnRun );
          }
          ImGui::EndMenu();
        }
        ImGui::EndMenu();
      }
      ImGui::EndDisabled();
    }
    if ( ImGui::BeginMenu( "Options" ) )
    {
      openMenu = true;
      if ( ImGui::BeginMenu( "Input Configuration" ) )
      {
        configureKeyItem( "Left", KeyInput::LEFT );
        configureKeyItem( "Right", KeyInput::RIGHT );
        configureKeyItem( "Up", KeyInput::UP );
        configureKeyItem( "Down", KeyInput::DOWN );
        configureKeyItem( "A", KeyInput::OUTER );
        configureKeyItem( "B", KeyInput::INNER );
        configureKeyItem( "Opt1", KeyInput::OPTION1 );
        configureKeyItem( "Pause", KeyInput::PAUSE );
        configureKeyItem( "Opt2", KeyInput::OPTION2 );

        configureKeyWindow( keyToConfigure );

        ImGui::EndMenu();
      }

      if ( ImGui::BeginMenu( "Boot ROM" ) )
      {
        bool externalSelectEnabled = !sysConfig->bootROM.path.empty();
        if ( ImGui::BeginMenu( "Use external ROM", externalSelectEnabled ) )
        {
          if ( ImGui::Checkbox( "Enabled", &sysConfig->bootROM.useExternal ) )
          {
            mManager.mDoUpdate = true;
          }
          if ( ImGui::MenuItem( "Clear boot ROM path" ) )
          {
            sysConfig->bootROM.path.clear();
            if ( sysConfig->bootROM.useExternal )
            {
              sysConfig->bootROM.useExternal = false;
              mManager.mDoUpdate = true;
            }
          }
          ImGui::EndMenu();
        }
        if ( ImGui::MenuItem( "Select image" ) )
        {
          mFileBrowser->SetTitle( "Open boot ROM image file" );
          mFileBrowser->SetTypeFilters( { ".img", ".*" } );
          mFileBrowser->Open();
          fileBrowserAction = FileBrowserAction::OPEN_BOOTROM;
        }
        ImGui::EndMenu();
      }

      ImGui::Checkbox( "Single emulator instance", &sysConfig->singleInstance );
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
  }
  ImGui::PopStyleVar();

  if ( io.KeyCtrl )
  {
    if ( ImGui::IsKeyPressed( 'P' ) )
    {
      modalWindow = ModalWindow::PROPERTIES;
    }
    if ( ImGui::IsKeyPressed( 'C' ) )
    {
      mManager.mDebugger.visualizeCPU( !mManager.mDebugger.isCPUVisualized() );
    }
    if ( ImGui::IsKeyPressed( 'D' ) )
    {
      mManager.mDebugger.visualizeDisasm( !mManager.mDebugger.isDisasmVisualized() );
    }
    if ( ImGui::IsKeyPressed( 'H' ) )
    {
      bool historyWindow = !mManager.mDebugger.isHistoryVisualized();
      if ( historyWindow )
      {
        mManager.mInstance->debugCPU().enableHistory( mManager.mDebugger.historyVisualizer().columns, mManager.mDebugger.historyVisualizer().rows );
        mManager.mDebugger.visualizeHistory( true );
      }
      else
      {
        mManager.mInstance->debugCPU().disableHistory();
        mManager.mDebugger.visualizeHistory( false );
      }
    }
    if ( ImGui::IsKeyPressed( 'S' ) && mManager.mDebugger.isDebugMode() )
    {
      mManager.mDebugger.newScreenView();
    }
    if ( ImGui::IsKeyPressed( 'M' ) )
    {
      mManager.mAudioOut->mute( !mManager.mAudioOut->mute() );
    }
  }

  mManager.mDebugger.debugMode( debugMode );

  if ( resetIssued )
  {
    mManager.reset();
    mManager.mDebugger( RunMode::PAUSE );
  }
  if ( stepOutIssued )
  {
    mManager.mDebugger( RunMode::STEP_OUT );
  }
  if ( stepOverIssued )
  {
    mManager.mDebugger( RunMode::STEP_OVER );
  }
  if ( stepInIssued )
  {
    mManager.mDebugger( RunMode::STEP_IN );
  }
  else if ( pauseRunIssued )
  {
    mManager.mDebugger.togglePause();
  }

  switch ( modalWindow )
  {
  case ModalWindow::PROPERTIES:
    ImGui::OpenPopup( "Image properties" );
    break;
  default:
    break;
  }

  imagePropertiesWindow( modalWindow == ModalWindow::PROPERTIES );

  modalWindow = ModalWindow::NONE;


  if ( auto openPath = gConfigProvider.sysConfig()->lastOpenDirectory; !openPath.empty() )
  {
    mFileBrowser->SetPwd( gConfigProvider.sysConfig()->lastOpenDirectory );
  }
  mFileBrowser->Display();
  if ( mFileBrowser->HasSelected() )
  {
    using enum FileBrowserAction;
    switch ( fileBrowserAction )
    {
    case OPEN_CARTRIDGE:
      mManager.mArg = mFileBrowser->GetSelected();
      if ( auto parent = mManager.mArg.parent_path(); !parent.empty() )
      {
        gConfigProvider.sysConfig()->lastOpenDirectory = mManager.mArg.parent_path();
      }
      mManager.mDoUpdate = true;
      break;
    case OPEN_BOOTROM:
      sysConfig->bootROM.path = mFileBrowser->GetSelected();
      break;
    }
    mFileBrowser->ClearSelected();
    fileBrowserAction = NONE;
  }

  return openMenu;
}


void UI::drawDebugWindows( ImGuiIO& io )
{
  std::unique_lock<std::mutex> l{ mManager.mDebugger.mutex };

  bool cpuWindow = mManager.mDebugger.isCPUVisualized();
  bool disasmWindow = mManager.mDebugger.isDisasmVisualized();
  bool historyWindow = mManager.mDebugger.isHistoryVisualized();
  bool debugMode = mManager.mDebugger.isDebugMode();

  if ( debugMode )
  {
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 2.0f, 2.0f } );

    if ( cpuWindow )
    {
      ImGui::Begin( "CPU", &cpuWindow, ImGuiWindowFlags_AlwaysAutoResize );
      mManager.renderBoard( mManager.mDebugger.cpuVisualizer() );
      ImGui::End();
      mManager.mDebugger.visualizeCPU( cpuWindow );
    }

    if ( disasmWindow )
    {
      ImGui::Begin( "Disassembly", &disasmWindow, ImGuiWindowFlags_AlwaysAutoResize );
      mManager.renderBoard( mManager.mDebugger.disasmVisualizer() );
      ImGui::End();
      mManager.mDebugger.visualizeDisasm( disasmWindow );
    }

    if ( historyWindow )
    {
      ImGui::Begin( "History", &historyWindow, ImGuiWindowFlags_AlwaysAutoResize );
      mManager.renderBoard( mManager.mDebugger.historyVisualizer() );
      ImGui::End();
      mManager.mDebugger.visualizeHistory( historyWindow );
    }

    {
      static const float xpad = 4.0f;
      static const float ypad = 4.0f + 19.0f;
      ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2{ 160.0f + xpad, 102.0f + ypad } );

      ImGui::Begin( "Rendering", &debugMode, ImGuiWindowFlags_NoCollapse );
      auto size = ImGui::GetWindowSize();
      size.x = std::max( 0.0f, size.x - xpad );
      size.y = std::max( 0.0f, size.y - ypad );
      if ( auto tex = mManager.mRenderer->mainRenderingTexture( (int)size.x, (int)size.y ) )
      {
        ImGui::Image( tex, size );
      }
      ImGui::End();
      ImGui::PopStyleVar();
    }


    std::vector<int> removedIds;
    for ( auto& sv : mManager.mDebugger.screenViews() )
    {
      static const float xpad = 4.0f;
      static const float ypad = ( 4.0f + 19.0f ) * 2;
      ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2{ 160.0f + xpad, 102.0f + ypad } );

      char buf[64];
      std::sprintf( buf, "Screen View %d", sv.id );
      bool open = true;
      ImGui::Begin( buf, &open, 0 );
      if ( !open )
        removedIds.push_back( sv.id );
      ImGui::SetNextItemWidth( 80 );
      ImGui::Combo( "##sv", (int*)&sv.type, "dispadr\0vidbase\0collbas\0custom\0" );
      ImGui::SameLine();
      std::span<uint8_t const> data{};
      std::span<uint8_t const> palette{};

      switch ( sv.type )
      {
      case ScreenViewType::DISPADR:
        ImGui::BeginDisabled();
        if ( mManager.mInstance )
        {
          uint16_t addr = mManager.mInstance->debugDispAdr();
          std::sprintf( buf, "%04x", addr );
          data = std::span<uint8_t const>{ mManager.mInstance->debugRAM() + addr, 80 * 102 };
          if ( !sv.safePalette )
            palette = mManager.mInstance->debugPalette();
        }
        break;
      case ScreenViewType::VIDBAS:
        ImGui::BeginDisabled();
        if ( mManager.mInstance )
        {
          uint16_t addr = mManager.mInstance->debugVidBas();
          std::sprintf( buf, "%04x", addr );
          data = std::span<uint8_t const>{ mManager.mInstance->debugRAM() + addr, 80 * 102 };
          if ( !sv.safePalette )
            palette = mManager.mInstance->debugPalette();
        }
        break;
      case ScreenViewType::COLLBAS:
        ImGui::BeginDisabled();
        if ( mManager.mInstance )
        {
          uint16_t addr = mManager.mInstance->debugCollBas();
          std::sprintf( buf, "%04x", addr );
          data = std::span<uint8_t const>{ mManager.mInstance->debugRAM() + addr, 80 * 102 };
          if ( !sv.safePalette )
            palette = mManager.mInstance->debugPalette();
        }
        break;
      default:  //ScreenViewType::CUSTOM:
        ImGui::BeginDisabled( false );
        if ( mManager.mInstance )
        {
          uint16_t addr = sv.customAddress;
          std::sprintf( buf, "%04x", sv.customAddress );
          data = std::span<uint8_t const>{ mManager.mInstance->debugRAM() + sv.customAddress, 80 * 102 };
          if ( !sv.safePalette )
            palette = mManager.mInstance->debugPalette();
        }
        break;
      }
      ImGui::SetNextItemWidth( 40 );
      if ( ImGui::InputTextWithHint( "##ha", "hex addr", buf, 5, ImGuiInputTextFlags_CharsHexadecimal ) )
      {
        int hex;
        std::from_chars( &buf[0], &buf[5], hex, 16 );
        hex = std::min( hex, 0xe000 );
        sv.customAddress = (uint16_t)( hex & 0b1111111111111100 );
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::Checkbox( "safe palette", &sv.safePalette );
      auto size = ImGui::GetWindowSize();
      size.x = std::max( 0.0f, size.x - xpad );
      size.y = std::max( 0.0f, size.y - ypad );

      if ( auto tex = mManager.mRenderer->screenViewRenderingTexture( sv.id, sv.type, data, palette, (int)size.x, (int)size.y ) )
      {
        ImGui::Image( tex, size );
      }
      ImGui::End();
      ImGui::PopStyleVar();
    }

    for ( int id : removedIds )
    {
      mManager.mDebugger.delScreenView( id );
    }

    mManager.mDebugger.debugMode( debugMode );
    ImGui::PopStyleVar();


    if ( ImGui::BeginPopupContextVoid() )
    {
      if ( ImGui::Checkbox( "CPU Window", &cpuWindow ) )
      {
        mManager.mDebugger.visualizeCPU( cpuWindow );
      }
      if ( ImGui::Checkbox( "Disassembly Window", &disasmWindow ) )
      {
        mManager.mDebugger.visualizeDisasm( disasmWindow );
      }
      if ( ImGui::Checkbox( "History Window", &historyWindow ) )
      {
        mManager.mDebugger.visualizeHistory( historyWindow );
        if ( historyWindow )
        {
          mManager.mInstance->debugCPU().enableHistory( mManager.mDebugger.historyVisualizer().columns, mManager.mDebugger.historyVisualizer().rows );
        }
        else
        {
          mManager.mInstance->debugCPU().disableHistory();
        }
      }
      if ( ImGui::Selectable( "New Screen View" ) )
      {
        mManager.mDebugger.newScreenView();
      }
      ImGui::EndPopup();
    }
  }
  else
  {
  mManager.mRenderer->mainRenderingTexture( 0, 0 );
  }
}

void UI::configureKeyWindow( std::optional<KeyInput::Key>& keyToConfigure )
{
  if ( ImGui::BeginPopupModal( "Configure Key", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
  {
    if ( ImGui::BeginTable( "table", 3 ) )
    {
      ImGui::TableSetupColumn( "1", ImGuiTableColumnFlags_WidthFixed );
      ImGui::TableSetupColumn( "2", ImGuiTableColumnFlags_WidthFixed, 100.0f );
      ImGui::TableSetupColumn( "3", ImGuiTableColumnFlags_WidthFixed );

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text( "Press key" );
      ImGui::TableNextRow( ImGuiTableRowFlags_None, 30.0f );
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      static int code = 0;
      if ( code == 0 )
      {
        code = mManager.mIntputSource->getVirtualCode( *keyToConfigure );
      }
      if ( auto c = mManager.mIntputSource->firstKeyPressed() )
      {
        code = c;
      }
      ImGui::Text( keyName( code ) );
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if ( ImGui::Button( "OK", ImVec2( 60, 0 ) ) )
      {
        mManager.mIntputSource->updateMapping( *keyToConfigure, code );
        keyToConfigure = std::nullopt;
        code = 0;
        ImGui::CloseCurrentPopup();
      }
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();
      ImGui::SetItemDefaultFocus();
      if ( ImGui::Button( "Cancel", ImVec2( 60, 0 ) ) )
      {
        keyToConfigure = std::nullopt;
        code = 0;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }
}

void UI::imagePropertiesWindow( bool init )
{
  if ( ImGui::BeginPopupModal( "Image properties", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
  {
    static int rotation;
    static ImageProperties::EEPROM eeprom;
    if ( init )
    {
      rotation = (int)mManager.mImageProperties->getRotation();
      eeprom = mManager.mImageProperties->getEEPROM();
    }

    auto isChanged = [&]
    {
      if ( rotation != (int)mManager.mImageProperties->getRotation() )
        return true;
      if ( eeprom.bits != mManager.mImageProperties->getEEPROM().bits )
        return true;

      return false;
    };

    auto cartName = mManager.mImageProperties->getCartridgeName();
    auto manufName = mManager.mImageProperties->getMamufacturerName();
    auto const& bankProps = mManager.mImageProperties->getBankProps();
    ImGui::TextUnformatted( "Cart Name:" );
    ImGui::SameLine();
    ImGui::TextUnformatted( cartName.data(), cartName.data() + cartName.size() );
    ImGui::TextUnformatted( "Manufacturer:" );
    ImGui::SameLine();
    ImGui::TextUnformatted( manufName.data(), manufName.data() + manufName.size() );
    ImGui::TextUnformatted( "Size:" );
    ImGui::SameLine();
    if ( bankProps[2].numberOfPages > 0 )
    {
      ImGui::Text( "( %d + %d ) * %d B = %d B", bankProps[0].numberOfPages, bankProps[2].numberOfPages, bankProps[0].pageSize, ( bankProps[0].numberOfPages + bankProps[2].numberOfPages ) * bankProps[0].pageSize );
    }
    else
    {
      ImGui::Text( "%d * %d B = %d B", bankProps[0].numberOfPages, bankProps[0].pageSize, bankProps[0].numberOfPages * bankProps[0].pageSize );
    }
    if ( bankProps[1].pageSize * bankProps[1].numberOfPages > 0 )
    {
      ImGui::TextUnformatted( "Aux Size:" );
      ImGui::SameLine();
      if ( bankProps[3].numberOfPages > 0 )
      {
        ImGui::Text( "( %d + %d ) * %d B = %d B", bankProps[1].numberOfPages, bankProps[3].numberOfPages, bankProps[1].pageSize, ( bankProps[1].numberOfPages + bankProps[3].numberOfPages ) * bankProps[1].pageSize );
      }
      else
      {
        ImGui::Text( "%d * %d B = %d B", bankProps[1].numberOfPages, bankProps[1].pageSize, bankProps[1].numberOfPages * bankProps[1].pageSize );
      }
    }
    ImGui::TextUnformatted( "AUDIn Used?:" );
    ImGui::SameLine();
    ImGui::TextUnformatted( mManager.mImageProperties->getAUDInUsed() ? "Yes" : "No" );
    ImGui::TextUnformatted( "Rotation:" );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 80 );
    ImGui::Combo( "##r", &rotation, "Normal\0Left\0Right\0" );
    ImGui::TextUnformatted( "EEPROM:" );
    ImGui::SameLine();
    int eepromType = eeprom.type();
    ImGui::SetNextItemWidth( 80 );
    if ( ImGui::Combo( "##", &eepromType, ImageProperties::EEPROM::NAMES.data(), ImageProperties::EEPROM::TYPE_COUNT ) )
    {
      eeprom.setType( eepromType );
    }
    if ( eeprom.type() != 0 )
    {
      ImGui::TextUnformatted( "EEPROM bitness:" );
      ImGui::SameLine();
      int bitness = eeprom.is16Bit() ? 1 : 0;
      if ( ImGui::RadioButton( "8-bit", &bitness, 0 ) )
      {
        eeprom.set16bit( false );
      }
      ImGui::SameLine();
      if ( ImGui::RadioButton( "16-bit", &bitness, 1 ) )
      {
        eeprom.set16bit( true );
      }
    }
    ImGui::TextUnformatted( "SD Card support:" );
    ImGui::SameLine();
    int sd = eeprom.sd() ? 1 : 0;
    if ( ImGui::RadioButton( "No", &sd, 0 ) )
    {
      eeprom.setSD( false );
    }
    ImGui::SameLine();
    if ( ImGui::RadioButton( "Yes", &sd, 1 ) )
    {
      eeprom.setSD( true );
    }
    ImGui::BeginDisabled( !isChanged() );
    if ( ImGui::Button( "Apply", ImVec2( 60, 0 ) ) )
    {
      mManager.mImageProperties->setRotation( rotation );
      mManager.updateRotation();
      if ( eeprom.bits != mManager.mImageProperties->getEEPROM().bits )
      {
        mManager.mImageProperties->setEEPROM( eeprom.bits );
        mManager.mDoUpdate = true;
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetItemDefaultFocus();
    if ( ImGui::Button( "Cancel", ImVec2( 60, 0 ) ) )
    {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
