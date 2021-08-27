#pragma once

#include "IVideoSink.hpp"
#include "DisplayGenerator.hpp"

struct RenderFrame;
class WinImgui;
class Manager;
class IEncoder;

class WinRenderer
{
public:

  WinRenderer();
  ~WinRenderer();

  void setInstances( int instances );
  void setEncoder( std::shared_ptr<IEncoder> encoder );
  void initialize( HWND hWnd, std::filesystem::path const& iniPath );
  int win32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

  std::shared_ptr<IVideoSink> getVideoSink( int instance ) const;

  void render( Manager & config );

private:
  struct CBPosSize
  {
    int32_t posx;
    int32_t posy;
    int32_t scale;
    uint32_t vscale;
  };

  struct Pixel
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t x;
  };

  struct DPixel
  {
    Pixel left;
    Pixel right;
  };


  void updateSourceTexture( int instance, std::shared_ptr<RenderFrame> frame );
  void updateVscale( uint32_t vScale );

  int sizing( RECT & rect );

  class SizeManager
  {
  public:
    SizeManager();
    SizeManager( int instances, bool horizontal, int windowWidth, int windowHeight );

    int windowWidth() const;
    int windowHeight() const;
    int minWindowWidth( std::optional<bool> horizontal = std::nullopt ) const;
    int minWindowHeight( std::optional<bool> horizontal = std::nullopt ) const;
    int instanceXOff( int instance ) const;
    int instanceYOff( int instance ) const;
    int scale() const;
    bool horizontal() const;
    explicit operator bool() const;

  private:
    int mWinWidth;
    int mWinHeight;
    int mScale;
    int mInstances;
    bool mHorizontal;
  };

  struct Instance : public IVideoSink
  {
    Instance();

    std::array<DPixel, 256> mPalette;
    std::shared_ptr<RenderFrame> mActiveFrame;
    std::queue<std::shared_ptr<RenderFrame>> mFinishedFrames;
    mutable std::mutex mQueueMutex;
    uint64_t mBeginTick;
    uint64_t mLastTick;
    uint64_t mFrameTicks;

    void updatePalette( uint16_t reg, uint8_t value );
    void newFrame( uint64_t tick, uint8_t hbackup ) override;
    void newRow( uint64_t tick, int row ) override;
    void emitScreenData( std::span<uint8_t const> data ) override;
    void updateColorReg( uint8_t reg, uint8_t value ) override;
    std::shared_ptr<RenderFrame> pullNextFrame();
  };

private:
  std::array<std::shared_ptr<Instance>,2> mInstances;
  std::atomic<int> mInstancesCount;
  HWND mHWnd;
  std::unique_ptr<WinImgui>         mImgui;
  ComPtr<ID3D11Device>              mD3DDevice;
  ComPtr<ID3D11DeviceContext>       mImmediateContext;
  ComPtr<IDXGISwapChain>            mSwapChain;
  ComPtr<ID3D11ComputeShader>       mRendererCS;
  ComPtr<ID3D11Buffer>              mPosSizeCB;
  ComPtr<ID3D11UnorderedAccessView> mBackBufferUAV;
  ComPtr<ID3D11RenderTargetView>    mBackBufferRTV;
  std::array<ComPtr<ID3D11Texture2D>,2> mSources;
  std::array<ComPtr<ID3D11ShaderResourceView>,2> mSourceSRVs;

  ComPtr<ID3D11Texture2D>           mPreStagingY;
  ComPtr<ID3D11Texture2D>           mPreStagingU;
  ComPtr<ID3D11Texture2D>           mPreStagingV;
  ComPtr<ID3D11Texture2D>           mStagingY;
  ComPtr<ID3D11Texture2D>           mStagingU;
  ComPtr<ID3D11Texture2D>           mStagingV;
  ComPtr<ID3D11UnorderedAccessView> mPreStagingYUAV;
  ComPtr<ID3D11UnorderedAccessView> mPreStagingUUAV;
  ComPtr<ID3D11UnorderedAccessView> mPreStagingVUAV;

  SizeManager mSizeManager;
  boost::rational<int32_t> mRefreshRate;
  std::shared_ptr<IEncoder> mEncoder;
  uint32_t mVScale;
};
