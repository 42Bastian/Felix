#pragma once

#include "IVideoSink.hpp"
#include "ActionQueue.hpp"
#include "DisplayGenerator.hpp"
#include "Suzy.hpp"
#include "Utility.hpp"

class Mikey;
class CPU;
class Cartridge;
class ComLynx;
class InputFile;
class ImageBS93;
class ImageBIOS;
class ImageCart;
class IEscape;
class ComLynxWire;

class Felix
{
public:
  Felix( std::shared_ptr<ComLynxWire> comLynxWire, std::shared_ptr<IVideoSink> videoSink, std::function<KeyInput()> const& inputProvider );
  ~Felix();

  void requestDisplayDMA( uint64_t tick, uint16_t address );
  void runSuzy();
  void assertInterrupt( int mask, std::optional<uint64_t> tick = std::nullopt );
  void desertInterrupt( int mask, std::optional<uint64_t> tick = std::nullopt );

  void setAudioOut( int sps, std::span<AudioSample> outputBuffer );
  int advance();

  void injectFile( InputFile const& file );
  void setLog( std::filesystem::path const & path );

  void enterMonitor();

  Cartridge & getCartridge();
  ComLynx & getComLynx();

private:

  enum class PageType
  {
    RAM = 0 * 5,
    SUZY = 1 * 5,
    MIKEY = 2 * 5,
    KERNEL = 3 * 5
  };

  struct MAPCTL
  {
    bool sequentialDisable;
    bool vectorSpaceDisable;
    bool kernelDisable;
    bool mikeyDisable;
    bool suzyDisable;
  };

  enum class SequencedActionResult
  {
    CARRY_ON,
    SWITCH_INSTANCE,
    BAIL_OUT
  } executeSequencedAction( SequencedAction );
  bool executeSuzyAction();
  void executeCPUAction();
  void handlePatch( uint16_t address );

  uint8_t readKernel( uint16_t address );
  void writeKernel( uint16_t address, uint8_t value );

  void pulseReset( std::optional<uint16_t> resetAddress = std::nullopt );
  void writeMAPCTL( uint8_t value );
  void enqueueSampling();

private:
  std::array<uint8_t,65536> mRAM;
  std::array<uint8_t, 512> mROM;
  std::array<PageType, 256> mPageTypes;
  std::array<std::shared_ptr<IEscape>, 16> mEscapes;
  uint64_t mCurrentTick;
  int mSamplesRemainder;
  int mSPS;
  std::span<AudioSample> mOutputSamples;
  uint32_t mSamplesEmitted;
  ActionQueue mActionQueue;
  std::shared_ptr<CPU> mCpu;
  std::shared_ptr<Cartridge> mCartridge;
  std::shared_ptr<ComLynx> mComLynx;
  std::shared_ptr<ComLynxWire> mComLynxWire;
  std::shared_ptr<Mikey> mMikey;
  std::shared_ptr<Suzy> mSuzy;
  MAPCTL mMapCtl;
  uint64_t mFastCycleTick;
  uint64_t mPatchMagickCodeAccumulator;
  uint32_t mSequencedAccessAddress;
  uint16_t mDMAAddress;
  std::shared_ptr<ISuzyProcess> mSuzyProcess;
  ISuzyProcess::Request const* mSuzyProcessRequest;
  bool mResetRequestDuringSpriteRendering;
  bool mSuzyRunning;
};
