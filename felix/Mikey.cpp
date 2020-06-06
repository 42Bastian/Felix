#include "Mikey.hpp"
#include <cassert>
#include "TimerCore.hpp"
#include "AudioChannel.hpp"
#include "Felix.hpp"
#include "Cartridge.hpp"

Mikey::Mikey( Felix & busMaster, std::function<void( DisplayGenerator::Pixel const* )> const& fun ) : mFelix{ busMaster }, mAccessTick{}, mTimers{}, mAudioChannels{}, mPalette{}, mAttenuation{ 0xff, 0xff, 0xff, 0xff }, mDisplayGenerator{ std::make_unique<DisplayGenerator>( fun ) },
mParallelPort{ mFelix, *mDisplayGenerator }, mDisplayRegs{}, mSerCtl{}, mSuzyDone{}, mPan{ 0xff }, mStereo{}, mSerDat{}, mIRQ{}
{
  mTimers[0x0] = std::make_unique<TimerCore>( 0x0, [this]( uint64_t tick, bool interrupt )
  {
    uint8_t cnt = mTimers[0x02]->getCount( tick );
    if ( cnt == 101 )
    {
      mDisplayGenerator->updateDispAddr( mDisplayRegs.dispAdr );
    }
    if ( auto dma = mDisplayGenerator->hblank( tick, cnt ) )
    {
      mFelix.requestDisplayDMA( dma.tick, dma.address );
    }
    mTimers[0x2]->borrowIn( tick );
    if ( interrupt )
    {
      setIRQ( 0x01 );
    }
  } );  //timer 0 -> timer 2
  mTimers[0x1] = std::make_unique<TimerCore>( 0x1, [this]( uint64_t tick, bool interrupt )
  {
    mTimers[0x3]->borrowIn( tick );
    if ( interrupt )
    {
      setIRQ( 0x02 );
    }
  } );  //timer 1 -> timer 3
  mTimers[0x2] = std::make_unique<TimerCore>( 0x2, [this]( uint64_t tick, bool interrupt )
  {
    mTimers[0x4]->borrowIn( tick );
    mDisplayGenerator->vblank( tick );
    if ( interrupt )
    {
      setIRQ( 0x04 );
    }
  } );  //timer 2 -> timer 4
  mTimers[0x3] = std::make_unique<TimerCore>( 0x3, [this]( uint64_t tick, bool interrupt )
  {
    mTimers[0x5]->borrowIn( tick );
    if ( interrupt )
    {
      setIRQ( 0x08 );
    }
  } );  //timer 3 -> timer 5
  mTimers[0x4] = std::make_unique<TimerCore>( 0x4, [this]( uint64_t tick, bool interrupt )
  {
  } );  //timer 4
  mTimers[0x5] = std::make_unique<TimerCore>( 0x5, [this]( uint64_t tick, bool interrupt )
  {
    mTimers[0x7]->borrowIn( tick );
    if ( interrupt )
    {
      setIRQ( 0x20 );
    }
  } );  //timer 5 -> timer 7
  mTimers[0x6] = std::make_unique<TimerCore>( 0x6, [this]( uint64_t tick, bool interrupt )
  {
    if ( interrupt )
    {
      setIRQ( 0x40 );
    }
  } );  //timer 6
  mTimers[0x7] = std::make_unique<TimerCore>( 0x7, [this]( uint64_t tick, bool interrupt )
  {
    mTimers[0x8]->borrowIn( tick );
    if ( interrupt )
    {
      setIRQ( 0x80 );
    }
  } );  //timer 7 -> audio 0
  mTimers[0x8] = std::make_unique<TimerCore>( 0x8, [this]( uint64_t tick, bool unused )
  {
    mAudioChannels[0x0]->trigger();
    mTimers[0x9]->borrowIn( tick );
  } );  //audio 0 -> audio 1
  mTimers[0x9] = std::make_unique<TimerCore>( 0x9, [this]( uint64_t tick, bool unused )
  {
    mAudioChannels[0x1]->trigger();
    mTimers[0xa]->borrowIn( tick );
  } );  //audio 1 -> audio 2
  mTimers[0xa] = std::make_unique<TimerCore>( 0xa, [this]( uint64_t tick, bool unused )
  {
    mAudioChannels[0x2]->trigger();
    mTimers[0xb]->borrowIn( tick );
  } );  //audio 2 -> audio 3
  mTimers[0xb] = std::make_unique<TimerCore>( 0xb, [this]( uint64_t tick, bool unused )
  {
    mAudioChannels[0x3]->trigger();
    mTimers[0x0]->borrowIn( tick );
  } );  //audio 3 -> timer 1

  mAudioChannels[0x0] = std::make_unique<AudioChannel>( *mTimers[0x8] );
  mAudioChannels[0x1] = std::make_unique<AudioChannel>( *mTimers[0x9] );
  mAudioChannels[0x2] = std::make_unique<AudioChannel>( *mTimers[0xa] );
  mAudioChannels[0x3] = std::make_unique<AudioChannel>( *mTimers[0xb] );
}

Mikey::~Mikey()
{
}

uint64_t Mikey::requestAccess( uint64_t tick, uint16_t address )
{
  mAccessTick = tick + 5;

  address &= 0xff;
  int timer = -1;

  if ( address < 0x20 )
  {
    timer = ( address & 0x1f ) >> 2;
  }
  else if ( address < 0x40 )
  {
    timer = 8 + ( ( ( address - 0x20 ) & 0x1f ) >> 3 );
  }

  if ( timer >= 0 )
  {
    auto nextTick = mAccessTick & ~0xfull + ( timer * 16 / 12 );
    mAccessTick = nextTick < mAccessTick ? nextTick + 16 : nextTick;
  }
  
  return mAccessTick;
}

uint8_t Mikey::read( uint16_t address )
{
  address &= 0xff;

  if ( address < 0x20 )
  {
    switch ( address & 0x3 )
    {
    case TIMER::BACKUP:
      return mTimers[( address >> 2 ) & 7]->getBackup( mAccessTick );
    case TIMER::CONTROLA:
      return mTimers[( address >> 2 ) & 7]->getControlA( mAccessTick );
    case TIMER::COUNT:
      return mTimers[( address >> 2 ) & 7]->getCount( mAccessTick );
    case TIMER::CONTROLB:
      return mTimers[( address >> 2 ) & 7]->getControlB( mAccessTick );
    }
  }
  else if ( address < 0x40 )
  {
    switch ( address & 0x7 )
    {
    case AUDIO::VOLCNTRL:
      return mAudioChannels[( address >> 3 ) & 3]->getVolume();
    case AUDIO::FEEDBACK:
      return mAudioChannels[( address >> 3 ) & 3]->getFeedback();
    case AUDIO::OUTPUT:
      return mAudioChannels[( address >> 3 ) & 3]->getOutput();
    case AUDIO::SHIFT:
      return mAudioChannels[( address >> 3 ) & 3]->getShift();
    case AUDIO::BACKUP:
      return mAudioChannels[( address >> 3 ) & 3]->getBackup( mAccessTick );
    case AUDIO::CONTROL:
      return mAudioChannels[( address >> 3 ) & 3]->getControl( mAccessTick );
    case AUDIO::COUNTER:
      return mAudioChannels[( address >> 3 ) & 3]->getCounter( mAccessTick );
    case AUDIO::OTHER:
      return mAudioChannels[( address >> 3 ) & 3]->getOther( mAccessTick );
    }
  }
  else switch ( address )
  {
  case ATTENREG0:
    return mAttenuation[0];
  case ATTENREG1:
    return mAttenuation[1];
  case ATTENREG2:
    return mAttenuation[2];
  case ATTENREG3:
    return mAttenuation[3];
  case MPAN:
    return mPan;
  case MSTEREO:
    return mStereo;
  case INTRST:
  case INTSET:
    return mIRQ;
  case IODIR:
    return mParallelPort.getDirection();
  case IODAT:
    return mParallelPort.getData();
  case MIKEYHREV:
    return 0x01;
  case SERCTL:
    return
      ( mSerCtl.txrdy ? 0x80 : 0x00 ) |
      ( mSerCtl.rxrdy ? 0x40 : 0x00 ) |
      ( mSerCtl.txempty ? 0x20 : 0x00 ) |
      ( mSerCtl.parerr ? 0x10 : 0x00 ) |
      ( mSerCtl.overrun ? 0x08 : 0x00 ) |
      ( mSerCtl.framerr ? 0x04 : 0x00 ) |
      ( mSerCtl.rxbrk ? 0x02 : 0x00 ) |
      ( mSerCtl.parbit ? 0x01 : 0x00 );
  case GREEN + 0x00:
  case GREEN + 0x01:
  case GREEN + 0x02:
  case GREEN + 0x03:
  case GREEN + 0x04:
  case GREEN + 0x05:
  case GREEN + 0x06:
  case GREEN + 0x07:
  case GREEN + 0x08:
  case GREEN + 0x09:
  case GREEN + 0x0a:
  case GREEN + 0x0b:
  case GREEN + 0x0c:
  case GREEN + 0x0d:
  case GREEN + 0x0e:
  case GREEN + 0x0f:
  case BLUERED + 0x00:
  case BLUERED + 0x01:
  case BLUERED + 0x02:
  case BLUERED + 0x03:
  case BLUERED + 0x04:
  case BLUERED + 0x05:
  case BLUERED + 0x06:
  case BLUERED + 0x07:
  case BLUERED + 0x08:
  case BLUERED + 0x09:
  case BLUERED + 0x0a:
  case BLUERED + 0x0b:
  case BLUERED + 0x0c:
  case BLUERED + 0x0d:
  case BLUERED + 0x0e:
  case BLUERED + 0x0f:
    return mPalette[address - GREEN];
  default:
    return (uint8_t)0xff;
    break;
  }

  return uint8_t();
}

Mikey::WriteAction Mikey::write( uint16_t address, uint8_t value )
{
  SequencedAction result;
  address &= 0xff;

  if ( address < 0x20 )
  {
    switch ( address & 0x3 )
    {
    case TIMER::BACKUP:
      result = mTimers[(address >> 2) & 7]->setBackup( mAccessTick, value );
      break;
    case TIMER::CONTROLA:
      result = mTimers[( address >> 2 ) & 7]->setControlA( mAccessTick, value );
      break;
    case TIMER::COUNT:
      result = mTimers[( address >> 2 ) & 7]->setCount( mAccessTick, value );
      break;
    case TIMER::CONTROLB:
      result = mTimers[( address >> 2 ) & 7]->setControlB( mAccessTick, value );
      break;
    }
  }
  else if ( address < 0x40 )
  {
    switch ( address & 0x7 )
    {
    case AUDIO::VOLCNTRL:
      result = mAudioChannels[( address >> 3 ) & 3]->setVolume( (int8_t)value );
      break;
    case AUDIO::FEEDBACK:
      result = mAudioChannels[( address >> 3 ) & 3]->setFeedback( value );
      break;
    case AUDIO::OUTPUT:
      result = mAudioChannels[( address >> 3 ) & 3]->setOutput( value );
      break;
    case AUDIO::SHIFT:
      result = mAudioChannels[( address >> 3 ) & 3]->setShift( value );
      break;
    case AUDIO::BACKUP:
      result = mAudioChannels[( address >> 3 ) & 3]->setBackup( mAccessTick, value );
      break;
    case AUDIO::CONTROL:
      result = mAudioChannels[( address >> 3 ) & 3]->setControl( mAccessTick, value );
      break;
    case AUDIO::COUNTER:
      result = mAudioChannels[( address >> 3 ) & 3]->setCounter( mAccessTick, value );
      break;
    case AUDIO::OTHER:
      result = mAudioChannels[( address >> 3 ) & 3]->setOther( mAccessTick, value );
      break;
    }
  }
  else switch ( address )
  {
  case ATTENREG0:
    mAttenuation[0] = value;
    break;
  case ATTENREG1:
    mAttenuation[1] = value;
    break;
  case ATTENREG2:
    mAttenuation[2] = value;
    break;
  case ATTENREG3:
    mAttenuation[3] = value;
    break;
  case MPAN:
    mPan = value;
    break;
  case MSTEREO:
    mStereo = value;
    break;
  case INTRST:
    resetIRQ( value );
    break;
  case INTSET:
    setIRQ( value );
    break;
  case SYSCTL1:
    if ( ( value & SYSCTL1::POWERON ) == 0 )
    {
      mFelix.enterMonitor();
    }
    mFelix.getCartridge().setCartAddressStrobe( ( value & SYSCTL1::CART_ADDR_STROBE ) == 1 );
    break;
  case IODIR:
    mParallelPort.setDirection( value );
    break;
  case IODAT:
    mParallelPort.setData( value );
    break;
  case SERCTL:
    mSerCtl.txinten = ( value & 0x80 ) != 0;
    mSerCtl.rxinten = ( value & 0x40 ) != 0;
    mSerCtl.paren = ( value & 0x10 ) != 0;
    mSerCtl.txopen = ( value & 0x04 ) != 0;
    mSerCtl.txbrk = ( value & 0x02 ) != 0;
    mSerCtl.pareven = ( value & 0x01 ) != 0;
    if ( ( value & 0x08 ) != 0 )
      mSerCtl.parerr = mSerCtl.overrun = mSerCtl.framerr = false;
    break;
  case SDONEACK:
    mSuzyDone = false;
    break;
  case CPUSLEEP:
    //The presence of an interrupt in Mikey, regardless of the state of the CPU enable interrupt bit,
    //will prevent the CPU from going to sleep, and thus prevent Suzy from functioning.
    //So if sprites stop working, unintentional interrupt bits can be the hidden cause.
    if ( !mSuzyDone && mIRQ == 0 )
    {
      return { WriteAction::Type::START_SUZY };
    }
    break;
  case DISPCTL:
    mDisplayRegs.dispColor = ( value & DISPCTL::DISP_COLOR ) != 0;
    mDisplayRegs.dispFourBit = ( value & DISPCTL::DISP_FOURBIT ) != 0;
    mDisplayRegs.dispFlip = ( value & DISPCTL::DISP_FLIP ) != 0;
    mDisplayRegs.DMAEnable = ( value & DISPCTL::DMA_ENABLE ) != 0;
    mDisplayGenerator->dispCtl( mDisplayRegs.dispColor, mDisplayRegs.dispFlip, mDisplayRegs.DMAEnable );
    break;
  case PBKUP:
    mDisplayGenerator->setPBKUP( value );
    break;
  case DISPADR:
    mDisplayRegs.dispAdr &= 0xff00;
    mDisplayRegs.dispAdr |= value;
    mDisplayRegs.dispAdr &= 0xfffc;
    break;
  case DISPADR+1:
    mDisplayRegs.dispAdr &= 0x00ff;
    mDisplayRegs.dispAdr |= value << 8;
    break;
  case MTEST0:
    break;
  case MTEST1:
    break;
  case MTEST2:
    if ( ( value & 0x01 ) != 0 )
      mDisplayGenerator->updateDispAddr( mDisplayRegs.dispAdr );
    break;
  case GREEN + 0x00:
  case GREEN + 0x01:
  case GREEN + 0x02:
  case GREEN + 0x03:
  case GREEN + 0x04:
  case GREEN + 0x05:
  case GREEN + 0x06:
  case GREEN + 0x07:
  case GREEN + 0x08:
  case GREEN + 0x09:
  case GREEN + 0x0a:
  case GREEN + 0x0b:
  case GREEN + 0x0c:
  case GREEN + 0x0d:
  case GREEN + 0x0e:
  case GREEN + 0x0f:
  case BLUERED + 0x00:
  case BLUERED + 0x01:
  case BLUERED + 0x02:
  case BLUERED + 0x03:
  case BLUERED + 0x04:
  case BLUERED + 0x05:
  case BLUERED + 0x06:
  case BLUERED + 0x07:
  case BLUERED + 0x08:
  case BLUERED + 0x09:
  case BLUERED + 0x0a:
  case BLUERED + 0x0b:
  case BLUERED + 0x0c:
  case BLUERED + 0x0d:
  case BLUERED + 0x0e:
  case BLUERED + 0x0f:
    mPalette[address - GREEN] = value;
    mDisplayGenerator->updatePalette( mAccessTick, address - GREEN, value );
    break;
  default:
//    assert( false );
    break;
  }

  if ( result )
  {
    return { WriteAction::Type::ENQUEUE_ACTION, result };
  }
  else
  {
    return {};
  }
}

SequencedAction Mikey::fireTimer( uint64_t tick, uint32_t timer )
{
  assert( timer < 12 );
  return mTimers[timer]->fireAction( tick );
}

void Mikey::setDMAData( uint64_t tick, uint64_t data )
{
  if ( auto dma = mDisplayGenerator->pushData( tick, data ) )
  {
    mFelix.requestDisplayDMA( dma.tick, dma.address );
  }
}

void Mikey::suzyDone()
{
  mSuzyDone = true;
}

std::pair<float, float> Mikey::sampleAudio() const
{
  std::pair<float, float> result{};

  for ( size_t i = 0; i < 4; ++i )
  {
    if ( ( mStereo & ( (uint8_t)0x01 << i ) ) == 0 )
    {
      if ( ( mPan & ( (uint8_t)0x01 << i ) ) != 0 )
      {
        result.first += ( (int8_t)mAudioChannels[i]->getOutput() * ( mAttenuation[i] & 0x0f ) ) / 16;
      }
      else
      {
        result.first += (int8_t)mAudioChannels[i]->getOutput();
      }
    }

    if ( ( mStereo & ( (uint8_t)0x10 << i ) ) == 0 )
    {
      if ( ( mPan & ( (uint8_t)0x10 << i ) ) != 0 )
      {
        result.second += ( (int8_t)mAudioChannels[i]->getOutput() * ( mAttenuation[i] & 0xf0 ) ) / ( 16 * 16 );
      }
      else
      {
        result.second += (int8_t)mAudioChannels[i]->getOutput();
      }
    }
  }

  return { result.first / ( 4.0f * 128.0f ), result.second / ( 4.0f * 128.0f ) };
}

void Mikey::setIRQ( uint8_t mask )
{
  mIRQ |= mask;
  if ( mIRQ != 0 )
  {
    mFelix.assertInterrupt( CPU::I_IRQ );
  }
}

void Mikey::resetIRQ( uint8_t mask )
{
  mIRQ &= ~mask;
  if ( mIRQ == 0 )
  {
    mFelix.desertInterrupt( CPU::I_IRQ );
  }
}

DisplayGenerator::Pixel const * Mikey::getSrface() const
{
  return mDisplayGenerator->getSrface();
}