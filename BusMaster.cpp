#include "BusMaster.hpp"
#include "CPU.hpp"
#include "Cartridge.hpp"
#include "ComLynx.hpp"
#include "Mikey.hpp"
#include <fstream>
#include <filesystem>
#include <cassert>

BusMaster::BusMaster() : mRAM{}, mROM{}, mPageTypes{}, mBusReservationTick{}, mCurrentTick{}, mActionQueue{},
mCpu{ std::make_shared<CPU>() }, mCartridge{ std::make_shared<Cartridge>() }, mComLynx{ std::make_shared<ComLynx>() }, mMikey{ std::make_shared<Mikey>( *this ) }, mSuzy{ std::make_shared<Suzy>() },
  mDReq{}, mCPUReq{}, mCpuExecute{ mCpu->execute( *this ) }, mCpuTrace{ /*cpuTrace( *mCpu, mDReq )*/ },
  mMapCtl{}, mSequencedAccessAddress{ ~0u }, mDMAAddress{}, mFastCycleTick{ 4 }
{
  for ( auto it = mRAM.begin(); it != mRAM.end(); ++it )
  {
    *it = (uint8_t)rand();
  }

  {
    std::ifstream fin{ "d:/test/tests/lynxboot.img", std::ios::binary };
    if ( fin.bad() )
      throw std::exception{};

    fin.read( ( char* )mROM.data(), mROM.size() );
  }
  if ( size_t size = (size_t)std::filesystem::file_size( "d:/test/tests/7.o" ) )
  {
    std::ifstream fin{ "d:/test/tests/7.o", std::ios::binary };
    if ( fin.bad() )
      throw std::exception{};

    fin.seekg( 2 );
    uint16_t start;
    fin.read( ( char* )&start+1, 1 );
    fin.read( ( char* )&start, 1 );
    fin.read( ( char* )mRAM.data() + start - 6, size );

    mROM[0x1fc] = start&0xff;
    mROM[0x1fd] = start>>8;
  }

  for ( size_t i = 0; i < mPageTypes.size(); ++i )
  {
    switch ( i )
    {
      case 0xff:
        mPageTypes[i] = PageType::FF;
        break;
      case 0xfe:
        mPageTypes[i] = PageType::FE;
        break;
      case 0xfd:
        mPageTypes[i] = PageType::MIKEY;
        break;
      case 0xfc:
        mPageTypes[i] = PageType::SUZY;
        break;
      default:
        mPageTypes[i] = PageType::RAM;
        break;
    }
  }

  //mCpuTrace = cpuTrace( *mCpu, mDReq );
}

BusMaster::~BusMaster()
{
}

CPURequest * BusMaster::request( CPURead r )
{
  mCPUReq = CPURequest{ r };
  processCPU();
  return &mCPUReq;
}

CPURequest * BusMaster::request( CPUFetchOpcode r )
{
  mCPUReq = CPURequest{ r };
  processCPU();
  return &mCPUReq;
}

CPURequest * BusMaster::request( CPUFetchOperand r )
{
  mCPUReq = CPURequest{ r };
  processCPU();
  return &mCPUReq;
}

CPURequest * BusMaster::request( CPUWrite w )
{
  mCPUReq = CPURequest{ w };
  processCPU();
  return &mCPUReq;
}

CPURequest * BusMaster::cpuRequest()
{
  return &mCPUReq;
}

void BusMaster::requestDisplayDMA( uint64_t tick, uint16_t address )
{
  mDMAAddress = address;
  mActionQueue.push( { Action::DISPLAY_DMA, tick } );
}

DisplayGenerator::Pixel const* BusMaster::process( uint64_t ticks, KeyInput & keys )
{
  mSuzy->updateKeyInput( keys );
  mActionQueue.push( { Action::END_FRAME, mCurrentTick + ticks } );

  for ( ;; )
  {
    auto seqAction = mActionQueue.pop();
    mCurrentTick = seqAction.getTick();
    auto action = seqAction.getAction();

    switch ( action )
    {
      case Action::DISPLAY_DMA:
        mMikey->setDMAData( mCurrentTick, *(uint64_t*)( mRAM.data() + mDMAAddress ) );
        mBusReservationTick += 6 * mFastCycleTick + 2 * 5;
        break;
      case Action::FIRE_TIMER0:
      case Action::FIRE_TIMER1:
      case Action::FIRE_TIMER2:
      case Action::FIRE_TIMER3:
      case Action::FIRE_TIMER4:
      case Action::FIRE_TIMER5:
      case Action::FIRE_TIMER6:
      case Action::FIRE_TIMER7:
      case Action::FIRE_TIMER8:
      case Action::FIRE_TIMER9:
      case Action::FIRE_TIMERA:
      case Action::FIRE_TIMERB:
      case Action::FIRE_TIMERC:
      if ( auto newAction = mMikey->fireTimer( mCurrentTick, ( int )action - ( int )Action::FIRE_TIMER0 ) )
      {
        mActionQueue.push( newAction );
      }
      break;
    case Action::CPU_FETCH_OPCODE_RAM:
      mCPUReq.value = mRAM[mCPUReq.address];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCurrentTick;
      mCPUReq.tick = mCurrentTick;
      mCPUReq.interrupt = mMikey->getIRQ() != 0 ? CPU::I_IRQ : 0;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_FETCH_OPERAND_RAM:
      mCPUReq.value = mRAM[mCPUReq.address];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_READ_RAM:
      mCPUReq.value = mRAM[mCPUReq.address];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      break;
    case Action::CPU_WRITE_RAM:
      mRAM[mCPUReq.address] = mCPUReq.value;
      mSequencedAccessAddress = ~0;
      mCPUReq();
      break;
    case Action::CPU_FETCH_OPCODE_FE:
      mCPUReq.value = mROM[mCPUReq.address & 0x1ff];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCurrentTick;
      mCPUReq.tick = mCurrentTick;
      mCPUReq.interrupt = mMikey->getIRQ() != 0 ? CPU::I_IRQ : 0;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_FETCH_OPERAND_FE:
      mCPUReq.value = mROM[mCPUReq.address & 0x1ff];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_READ_FE:
      mCPUReq.value = mROM[mCPUReq.address & 0x1ff];
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      break;
    case Action::CPU_WRITE_FE:
      mSequencedAccessAddress = ~0;
      mCPUReq();
      break;
    case Action::CPU_FETCH_OPCODE_FF:
      mCPUReq.value = readFF( mCPUReq.address & 0xff );
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCurrentTick;
      mCPUReq.tick = mCurrentTick;
      mCPUReq.interrupt = mMikey->getIRQ() != 0 ? CPU::I_IRQ : 0;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_FETCH_OPERAND_FF:
      mCPUReq.value = readFF( mCPUReq.address & 0xff );
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      mDReq.resume();
      break;
    case Action::CPU_READ_FF:
      mCPUReq.value = readFF( mCPUReq.address & 0xff );
      mSequencedAccessAddress = mCPUReq.address + 1;
      mCPUReq();
      break;
    case Action::CPU_WRITE_FF:
      writeFF( mCPUReq.address & 0xff, mCPUReq.value );
      mSequencedAccessAddress = ~0;
      mCPUReq();
      break;
    case Action::CPU_READ_SUZY:
      mCPUReq.value = mSuzy->read( mCPUReq.address );
      mCPUReq();
      break;
    case Action::CPU_WRITE_SUZY:
      mSuzy->write( mCPUReq.address, mCPUReq.value );
      mCPUReq();
      break;
    case Action::CPU_READ_MIKEY:
      mCPUReq.value = mMikey->read( mCPUReq.address );
      mCPUReq();
      break;
    case Action::CPU_WRITE_MIKEY:
      switch ( auto mikeyAction = mMikey->write( mCPUReq.address, mCPUReq.value ) )
      {
      case Mikey::WriteAction::Type::START_SUZY:
        mSuzyProcess = mSuzy->suzyProcess();
        //mSuzyExecute = mSuzy->processSprites( mSuzyReq );
        processSuzy();
        break;
      case Mikey::WriteAction::Type::ENQUEUE_ACTION:
        mActionQueue.push( mikeyAction.action );
        [[fallthrough]];
      case Mikey::WriteAction::Type::NONE:
        mCPUReq();
        break;
      }
      break;
    case Action::SUZY_NONE:
      mSuzyProcess.reset();
      mCPUReq();
      break;
    case Action::SUZY_READ:
      suzyRead( ( ISuzyProcess::RequestRead const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::SUZY_READ4:
      suzyRead4( ( ISuzyProcess::RequestRead4 const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::SUZY_WRITE:
      suzyWrite( ( ISuzyProcess::RequestWrite const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::SUZY_COLRMW:
      suzyColRMW( ( ISuzyProcess::RequestColRMW const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::SUZY_VIDRMW:
      suzyVidRMW( ( ISuzyProcess::RequestVidRMW const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::SUZY_XOR:
      suzyXor( ( ISuzyProcess::RequestXOR const* )mSuzyProcessRequest );
      processSuzy();
      break;
    case Action::END_FRAME:
      return mMikey->getSrface();
    case Action::CPU_FETCH_OPCODE_SUZY:
    case Action::CPU_FETCH_OPCODE_MIKEY:
    case Action::CPU_FETCH_OPERAND_SUZY:
    case Action::CPU_FETCH_OPERAND_MIKEY:
      assert( false );
      break;
    default:
      break;
    }
  }
}

void BusMaster::enterMonitor()
{
}

Cartridge & BusMaster::getCartridge()
{
  assert( mCartridge );
  return *mCartridge;
}

ComLynx & BusMaster::getComLynx()
{
  assert( mComLynx );
  return *mComLynx;
}

void BusMaster::suzyRead( ISuzyProcess::RequestRead const * req )
{
  auto value = mRAM[req->addr];
  mSuzyProcess->respond( value );
}

void BusMaster::suzyRead4( ISuzyProcess::RequestRead4 const* req )
{
  assert( req->addr <= 0xfffc );
  auto value = *( ( uint32_t const* )( mRAM.data() + req->addr ) );
  mSuzyProcess->respond( value );
}

void BusMaster::suzyWrite( ISuzyProcess::RequestWrite const * req )
{
  mRAM[req->addr] = req->value;
}

void BusMaster::suzyColRMW( ISuzyProcess::RequestColRMW const * req )
{
  assert( req->addr <= 0xfffc );
  const uint32_t value = *( (uint32_t const*)( mRAM.data() + req->addr ) );

  //broadcast
  const uint8_t u8 = req->value;
  const uint16_t u16 = req->value | ( req->value << 8 );
  const uint32_t u32 = u16 | ( u16 << 16 );

  const uint32_t rmwvaleu = ( value & ~req->mask ) | ( u32 & req->mask );
  *( (uint32_t*)( mRAM.data() + req->addr ) ) =  rmwvaleu;

  const uint32_t resvalue = value & req->mask;
  uint8_t result{};

  //horizontal max
  for ( int i = 0; i < 8; ++i )
  {
    result = std::max( result, (uint8_t)( resvalue >> ( i * 4 ) & 0x0f ) );
  }
  mSuzyProcess->respond( result );
}

void BusMaster::suzyVidRMW( ISuzyProcess::RequestVidRMW const* req )
{
  auto value = mRAM[req->addr] & req->mask | req->value;
  mRAM[req->addr] = ( uint8_t )value;
}

void BusMaster::suzyXor( ISuzyProcess::RequestXOR const* req )
{
  auto value = mRAM[req->addr] ^ req->value;
  mRAM[req->addr] = ( uint8_t )value;
}

void BusMaster::processSuzy()
{
  static constexpr std::array<int, ( int )ISuzyProcess::Request::Type::_SIZE> requestCost ={
    0, //NONE,
    0, //READ,
    3, //READ4,
    0, //WRITE,
    7, //COLRMW,
    1, //VIDRMW,
    1  //XOR,
  };

  mSuzyProcessRequest = mSuzyProcess->advance();
  int op = ( int )mSuzyProcessRequest->type;
  mActionQueue.push( { ( Action )( ( int )Action::SUZY_NONE + op ), mBusReservationTick } );
  if ( op )
  {
    mBusReservationTick += 5ull + requestCost[op] * mFastCycleTick;
  }
}

void BusMaster::processCPU()
{
  static constexpr std::array<Action, 25> requestToAction = {
    Action::NONE,
    Action::CPU_FETCH_OPCODE_RAM,
    Action::CPU_FETCH_OPERAND_RAM,
    Action::CPU_READ_RAM,
    Action::CPU_WRITE_RAM,
    Action::NONE_FE,
    Action::CPU_FETCH_OPCODE_FE,
    Action::CPU_FETCH_OPERAND_FE,
    Action::CPU_READ_FE,
    Action::CPU_WRITE_FE,
    Action::NONE_FF,
    Action::CPU_FETCH_OPCODE_FF,
    Action::CPU_FETCH_OPERAND_FF,
    Action::CPU_READ_FF,
    Action::CPU_WRITE_FF,
    Action::NONE_MIKEY,
    Action::CPU_FETCH_OPCODE_MIKEY,
    Action::CPU_FETCH_OPERAND_MIKEY,
    Action::CPU_READ_MIKEY,
    Action::CPU_WRITE_MIKEY,
    Action::NONE_SUZY,
    Action::CPU_FETCH_OPCODE_SUZY,
    Action::CPU_FETCH_OPERAND_SUZY,
    Action::CPU_READ_SUZY,
    Action::CPU_WRITE_SUZY
  };

  auto pageType = mPageTypes[mCPUReq.address >> 8];
  mActionQueue.push( { requestToAction[( size_t )mCPUReq.mType + ( int )pageType], mBusReservationTick } );
  switch ( pageType )
  {
    case PageType::RAM:
    case PageType::FE:
    case PageType::FF:
      mBusReservationTick += ( mCPUReq.address == mSequencedAccessAddress ) ? mFastCycleTick : 5;
      break;
    case PageType::MIKEY:
      mBusReservationTick = mMikey->requestAccess( mBusReservationTick, mCPUReq.address );
      break;
    case PageType::SUZY:
      mBusReservationTick = mSuzy->requestAccess( mBusReservationTick, mCPUReq.address );
      break;
  }
}

uint8_t BusMaster::readFF( uint16_t address )
{
  if ( address >= 0xfa )
  {
    uint8_t * ptr = mMapCtl.vectorSpaceDisable ? ( mRAM.data() + 0xff00 ) : ( mROM.data() + 0x100 );
    return ptr[address];
  }
  else if ( address < 0xf8 )
  {
    uint8_t * ptr = mMapCtl.kernelDisable ? ( mRAM.data() + 0xff00 ) : ( mROM.data() + 0x100 );
    return ptr[address];
  }
  else if ( address == 0xf9 )
  {
    return 0xf0 | //high nibble of MAPCTL is set
      ( mMapCtl.vectorSpaceDisable ? 0x08 : 0x00 ) |
      ( mMapCtl.kernelDisable ? 0x04 : 0x00 ) |
      ( mMapCtl.mikeyDisable ? 0x02 : 0x00 ) |
      ( mMapCtl.suzyDisable ? 0x01 : 0x00 );
  }
  else
  {
    //there is always RAM at 0xfff8
    return mRAM[0xff00 + address];
  }
}

void BusMaster::writeFF( uint16_t address, uint8_t value )
{
  if ( address >= 0xfa && mMapCtl.vectorSpaceDisable || address < 0xf8 && mMapCtl.kernelDisable || address == 0xf8 )
  {
    mRAM[0xff00 + address] = value;
  }
  else if ( address == 0xf9 )
  {
    mMapCtl.sequentialDisable = ( value & 0x80 ) != 0;
    mMapCtl.vectorSpaceDisable = ( value & 0x08 ) != 0;
    mMapCtl.kernelDisable = ( value & 0x04 ) != 0;
    mMapCtl.mikeyDisable = ( value & 0x02 ) != 0;
    mMapCtl.suzyDisable = ( value & 0x01 ) != 0;

    mFastCycleTick = mMapCtl.sequentialDisable ? 5 : 4;
    mPageTypes[0xfe] = mMapCtl.kernelDisable ? PageType::RAM : PageType::FE;
    mPageTypes[0xfd] = mMapCtl.mikeyDisable ? PageType::RAM : PageType::MIKEY;
    mPageTypes[0xfc] = mMapCtl.suzyDisable ? PageType::RAM : PageType::SUZY;
  }
  else
  {
    //ignore write to ROM
  }
}
