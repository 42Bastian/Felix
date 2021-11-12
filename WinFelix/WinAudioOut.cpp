#include "pch.hpp"
#include "WinAudioOut.hpp"
#include "Core.hpp"
#include "Log.hpp"
#include "IEncoder.hpp"

WinAudioOut::WinAudioOut() : mWav{}
{
  CoInitializeEx( NULL, COINIT_MULTITHREADED );

  HRESULT hr;

  ComPtr<IMMDeviceEnumerator> deviceEnumerator;
  hr = CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( deviceEnumerator.ReleaseAndGetAddressOf() ) );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = deviceEnumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, mDevice.ReleaseAndGetAddressOf() );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mDevice->Activate( __uuidof( IAudioClient ), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void **>( mAudioClient.ReleaseAndGetAddressOf() ) );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mAudioClient->GetMixFormat( &mMixFormat );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, mMixFormat, nullptr );
  if ( FAILED( hr ) )
    throw std::exception{};

  mEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
  if ( mEvent == NULL )
    throw std::exception{};

  mAudioClient->SetEventHandle( mEvent );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mAudioClient->GetBufferSize( &mBufferSize );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mAudioClient->GetService( __uuidof( IAudioRenderClient ), reinterpret_cast<void **>( mRenderClient.ReleaseAndGetAddressOf() ) );
  if ( FAILED( hr ) )
    throw std::exception{};

  hr = mAudioClient->GetService( __uuidof( IAudioClock ), reinterpret_cast<void**>( mAudioClock.ReleaseAndGetAddressOf() ) );
  if ( FAILED( hr ) )
    throw std::exception{};

  //bytes per second
  uint64_t frequency;
  hr = mAudioClock->GetFrequency( &frequency );
  if ( FAILED( hr ) )
    throw std::exception{};

  LARGE_INTEGER l;
  QueryPerformanceFrequency( &l );

  mTimeToSamples = (double)frequency / (double)(l.QuadPart * mMixFormat->nBlockAlign);

  mSamplesDelta = mSamplesDeltaDelta = 0;

  std::fill( mWindow.begin(), mWindow.end(), 0 );

  mAudioClient->Start();
}

WinAudioOut::~WinAudioOut()
{
  mAudioClient->Stop();

  if ( mEvent )
  {
    CloseHandle( mEvent );
    mEvent = NULL;
  }

  CoUninitialize();

  if ( mMixFormat )
  {
    CoTaskMemFree( mMixFormat );
    mMixFormat = nullptr;
  }

  if ( mWav )
  {
    wav_close( mWav );
    mWav = nullptr;
  }
}

void WinAudioOut::setEncoder( std::shared_ptr<IEncoder> pEncoder )
{
  mEncoder = std::move( pEncoder );
}

void WinAudioOut::setWavOut( std::filesystem::path path )
{
  mWav = wav_open( path.string().c_str(), WAV_OPEN_WRITE );
  if ( wav_err()->code != WAV_OK )
  {
    L_ERROR << "Error opening wav file " << path.string() << ": " << wav_err()->message;
    if ( mWav )
    {
      wav_close( mWav );
      mWav = nullptr;
    }
    return;
  }

  wav_set_format( mWav, WAV_FORMAT_IEEE_FLOAT );
  wav_set_num_channels( mWav, mMixFormat->nChannels );
  wav_set_sample_rate( mWav, mMixFormat->nSamplesPerSec );
  wav_set_sample_size( mWav, sizeof(float) );
}

int32_t WinAudioOut::correctedSPS( int64_t samplesEmittedPerFrame, int64_t renderingTimeQPC )
{
  int baseResult = mMixFormat->nSamplesPerSec;

  if ( samplesEmittedPerFrame == 0 )
    return baseResult;

  if ( renderingTimeQPC == 0 )
    return baseResult;

  auto samplesPerRenderingTimeR = renderingTimeQPC * mTimeToSamples;
  auto samplesPerRenderingTimeI = (int32_t)std::round( samplesPerRenderingTimeR );

  auto newDelta = samplesPerRenderingTimeI - (int32_t)samplesEmittedPerFrame;
  mSamplesDeltaDelta = newDelta - mSamplesDelta;
  mSamplesDelta = newDelta;

  int sum = 0;
  for ( size_t i = 0; i < mWindow.size() - 1; ++i )
  {
    sum += mWindow[i];
    mWindow[i] = mWindow[i + 1];
  }
  sum += mWindow.back();

  int avg = sum / (int)mWindow.size();

  mWindow.back() = samplesPerRenderingTimeI;

  double ratio = (double)avg / samplesEmittedPerFrame;

  //L_DEBUG << "SPF: " << samplesEmittedPerFrame << "\t\tSPR: " << samplesPerRenderingTimeI << "\t\tavg: " << avg << "\t" << ratio;

  return baseResult;
}

void WinAudioOut::fillBuffer( std::shared_ptr<Core> instance, int64_t renderingTimeQPC )
{
  DWORD retval = WaitForSingleObject( mEvent, 100 );
  if ( retval != WAIT_OBJECT_0 )
    return;

  HRESULT hr;
  uint32_t padding{};
  hr = mAudioClient->GetCurrentPadding( &padding );
  uint32_t framesAvailable = mBufferSize - padding;

  if ( framesAvailable > 0 )
  {
    if ( mSamplesBuffer.size() < framesAvailable )
    {
      mSamplesBuffer.resize( framesAvailable );
    }

    auto sps = correctedSPS( instance->globalSamplesEmittedPerFrame(), renderingTimeQPC );

    instance->setAudioOut( sps, std::span<AudioSample>{ mSamplesBuffer.data(), framesAvailable } );

    instance->advanceAudio();

    BYTE *pData;
    hr = mRenderClient->GetBuffer( framesAvailable, &pData );
    if ( FAILED( hr ) )
      return;
    float* pfData = reinterpret_cast<float*>( pData );
    for ( uint32_t i = 0; i < framesAvailable; ++i )
    {
      pfData[i * mMixFormat->nChannels + 0] = mSamplesBuffer[i].left / 32768.0f;
      pfData[i * mMixFormat->nChannels + 1] = mSamplesBuffer[i].right / 32768.0f;
    }

    if ( mEncoder )
      mEncoder->pushAudioBuffer( std::span<float const>( pfData, framesAvailable * mMixFormat->nChannels ) );

    if ( mWav )
      wav_write( mWav, pfData, framesAvailable );

    hr = mRenderClient->ReleaseBuffer( framesAvailable, 0 );
  }
}




