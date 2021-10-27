#include "pch.hpp"
#include "Cartridge.hpp"
#include "GameDrive.hpp"
#include "EEPROM.hpp"
#include "TraceHelper.hpp"

Cartridge::Cartridge( std::shared_ptr<ImageCart const> cart, std::shared_ptr<TraceHelper> traceHelper ) : mCart{ std::move( cart ) }, mGameDrive{ GameDrive::create( *mCart ) }, mEEPROM{ EEPROM::create( *mCart ) },
  mTraceHelper{ std::move( traceHelper ) },
  mShiftRegister{}, mCounter{}, mAudIn{}, mCurrentStrobe{}, mAddressData{},
  mBank0{ mCart->getBank0() }, mBank1{ mCart->getBank1() }
{
}

Cartridge::~Cartridge()
{
}

bool Cartridge::getAudIn( uint64_t tick ) const
{
  if ( mGameDrive && mGameDrive->hasOutput( tick ) )
  {
    return true;
  }

  if ( mEEPROM )
  {
    if ( auto opt = mEEPROM->output( tick ) )
    {
      return *opt;
    }
  }

  return mAudIn;
}

void Cartridge::setAudIn( bool value )
{
  //TODO: here add support for GD classic storage selection
  if ( mAudIn == value )
    return;

  mAudIn = value;

  if ( mAudIn )
  {
    auto bank0a = mCart->getBank0A();
    auto bank1a = mCart->getBank1A();

    if ( !bank0a.empty() )
      mBank0 = bank0a;

    if ( !bank1a.empty() )
      mBank1 = bank1a;
  }
  else
  {
    mBank0 = mCart->getBank0();
    mBank1 = mCart->getBank1();
  }
}

void Cartridge::setCartAddressData( bool value )
{
  mAddressData = value;
}

void Cartridge::setCartAddressStrobe( bool value )
{
  if ( value ) mCounter = 0;

  if ( value && !mCurrentStrobe )
  {
    uint32_t oldShift = mShiftRegister;
    uint32_t shiftBit = mAddressData ? 1 : 0;

    mShiftRegister = ( ( oldShift << 1 ) | shiftBit ) & 0xff;

    sprintf( mCommentBuffer.data(), "shift reg $%02x <- %d = $%02x", oldShift, shiftBit, mShiftRegister );
    mTraceHelper->setTraceComment( mCommentBuffer.data() );
  }

  mCurrentStrobe = value;
}

void Cartridge::setPower( bool value )
{
}

uint8_t Cartridge::peekRCART0( uint64_t tick )
{
  if ( mGameDrive )
  {
    if ( mGameDrive->hasOutput( tick ) )
    {
      auto result = mGameDrive->get( tick );
      incrementCounter( tick );
      return result;
    }
    else if ( auto bank = mGameDrive->getBank( tick ) )
    {
      auto result = peek( *bank );
      incrementCounter( tick );
      return result;
    }
  }

  auto result = peek( mBank0 );
  incrementCounter( tick );
  return result;
}

uint8_t Cartridge::peekRCART1( uint64_t tick )
{
  auto result = peek( mBank1 );
  incrementCounter( tick );
  return result;
}

void Cartridge::pokeRCART0( uint64_t tick, uint8_t value )
{
}

void Cartridge::pokeRCART1( uint64_t tick, uint8_t value )
{
  if ( mGameDrive )
  {
    return mGameDrive->put( tick, value );
  }
}

uint8_t Cartridge::peek( CartBank const & bank )
{
  sprintf( mCommentBuffer.data(), "Cart read from %02x:%03x", mShiftRegister, mCounter );
  mTraceHelper->setTraceComment( mCommentBuffer.data() );
  return bank( mShiftRegister, mCounter );
}

void Cartridge::incrementCounter( uint64_t tick )
{
  if ( !mCurrentStrobe )
  {
    //it's 11 bits, but it's masked anyway
    mCounter++;
    if ( mEEPROM && ( mCounter & 0b10 ) != 0 )
    {
      mEEPROM->tick( tick, ( mCounter & 0x80 ) != 0, mAudIn );
    }
  }
}
