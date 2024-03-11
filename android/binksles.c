// Copyright Epic Games, Inc. All Rights Reserved.
// based on the simple api source
#include "bink.h"
#include "popmal.h"
#include "string.h"
#include "rrCore.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "radstdout", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "radstdout", __VA_ARGS__))

static SLObjectItf engobj;
static SLObjectItf outmix;
static SLPlayItf pinter;
static SLAndroidSimpleBufferQueueItf qinter;
static SLObjectItf player;

static U8 * buffer = 0;
static S32 buf_size;
static S32 submitted_bufs;
static S32 returned_bufs;
static S32 cur_buf;
static S32 playing;

#define MAX_BUFFERS 4
#define START_BUFFERS 2
#define MS_PER_BUF 16

static void callback( SLAndroidSimpleBufferQueueItf bq, void *context )
{
  ++returned_bufs;
}

static int bink_sles_open( int freq, int chans, int * fragsize )
{
  SLEngineItf eng;
  SLDataLocator_AndroidSimpleBufferQueue q;
  SLDataFormat_PCM f;
  SLDataSource source;
  SLDataSink sink;
  SLDataLocator_OutputMix dataloc;
  U32 sz;
  SLInterfaceID needed[] = {SL_IID_BUFFERQUEUE};
  SLboolean needtobe[] = {SL_BOOLEAN_TRUE};

  slCreateEngine( &engobj, 0, 0, 0, 0, 0);
  if ( engobj == 0 )
  {
    LOGW("Bink: Couldn't create SL engine\n");
    return 0;
  }
  (*engobj)->Realize( engobj, SL_BOOLEAN_FALSE);

  (*engobj)->GetInterface( engobj, SL_IID_ENGINE, &eng );

  // Create the output system, if needed.
  (*eng)->CreateOutputMix( eng, &outmix, 0, 0, 0);
  if ( outmix == 0 )
  {
    LOGW("Bink: Couldn't create SL output mix\n");
    err:
    (*engobj)->Destroy( engobj );
    engobj = 0;
    return 0;
  }
  (*outmix)->Realize( outmix, SL_BOOLEAN_FALSE);
  
  // Queue
  q.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
  q.numBuffers = MAX_BUFFERS+1;

  // Format
  f.formatType = SL_DATAFORMAT_PCM;
  f.numChannels = chans;
  f.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  f.containerSize = 16;
  f.channelMask = 0; // 0 = stereo format
  if ( chans == 1 )
    f.channelMask = SL_SPEAKER_FRONT_LEFT;
  f.endianness = SL_BYTEORDER_LITTLEENDIAN;

  // Sampling rates are locked...
  switch (freq)
  {
    case 8000:  f.samplesPerSec = SL_SAMPLINGRATE_8; break;
    case 11025: f.samplesPerSec = SL_SAMPLINGRATE_11_025; break;
    case 16000: f.samplesPerSec = SL_SAMPLINGRATE_16; break;
    case 22050: f.samplesPerSec = SL_SAMPLINGRATE_22_05; break;
    case 24000: f.samplesPerSec = SL_SAMPLINGRATE_24; break;
    case 32000: f.samplesPerSec = SL_SAMPLINGRATE_32; break;
    case 44100: f.samplesPerSec = SL_SAMPLINGRATE_44_1; break;
    case 48000: f.samplesPerSec = SL_SAMPLINGRATE_48; break;
    default: 
      LOGW("Bink: Bad SL sound frequency\n");
      goto err;
  }

  // Source specification
  source.pFormat = &f;
  source.pLocator = &q;
  
  // Configure "Sink"
  dataloc.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  dataloc.outputMix = outmix;
  sink.pFormat = 0;
  sink.pLocator = &dataloc;

  // Create Player.
  (*eng)->CreateAudioPlayer( eng, &player, &source, &sink, 1, needed, needtobe );
  if ( player == 0 )
  {
    LOGW("Bink: Couldn't create SL audio player\n");
    err2:
    (*outmix)->Destroy( outmix );
    outmix = 0;
    goto err;
  } 
  (*player)->Realize( player, SL_BOOLEAN_FALSE);

  (*player)->GetInterface( player, SL_IID_PLAY, &pinter);
  (*player)->GetInterface( player, SL_IID_BUFFERQUEUE, &qinter);

  // Register for notification of queue completion.
  (*qinter)->RegisterCallback( qinter, callback, 0 );
 
  // Figure out how big our heap buffer needs to be, based on the channel count,
  // and ms size for the buffers.
  buf_size = ( freq * MS_PER_BUF ) / 1000;
  buf_size = (buf_size + 63) & ~63;
  // Ensure we are divisible by the bytes per channel
  buf_size *= chans * sizeof(S16);

  buffer = popmalloc( 0, buf_size * MAX_BUFFERS );
  if ( buffer == 0 )
  {
    LOGW("Bink: Couldn't sound memory\n");
    goto err2;
  }
  
  submitted_bufs = 0;
  returned_bufs = 0;
  cur_buf = 0;
  playing = 0;
  *fragsize = buf_size;

  return 1;
}

static void bink_sles_close(void) 
{
  if ( pinter )
  {
    (*pinter)->SetPlayState(pinter, SL_PLAYSTATE_STOPPED );
    pinter = 0;
  }

  if ( player )
  {
    (*player)->Destroy( player );
    player = 0;
  }
  
  if ( outmix )
  {
    (*outmix)->Destroy( outmix );
    outmix = 0;
  }

  if ( engobj )
  {
    (*engobj)->Destroy( engobj );
    engobj = 0;
  }

  if ( buffer )
  {
    popfree( buffer );
    buffer = 0;
  }
}

static int bink_sles_ready()
{
  S32 bufs = submitted_bufs - returned_bufs;
  if ( bufs < MAX_BUFFERS )
  {
    rrassert( bufs >= 0 );
    if ( bufs == 0 )
      playing = 0;
//    LOGI("ready bufs: %d %d\n",( MAX_BUFFERS - bufs ), buf_size );
    return ( MAX_BUFFERS - bufs ) * buf_size;
  }
  return 0;
}

static int bink_sles_write( const void * data, int length ) 
{
  S32 bufs = submitted_bufs - returned_bufs;

  if ( bufs < MAX_BUFFERS )
  {
    S32 ofs = submitted_bufs % MAX_BUFFERS;
    ofs *= buf_size;

    memcpy( buffer + ofs, data, length );
    (*qinter)->Enqueue( qinter, buffer + ofs, length );

    ++submitted_bufs;

    if ( ( !playing ) && ( (bufs+1) >= START_BUFFERS ) )
    {
      (*pinter)->SetPlayState(pinter, SL_PLAYSTATE_PLAYING );
      playing = 1;
    }

    return length;
  }
  return 0;
}

#define FREQS 8
static int ok_freqs[ FREQS ]={ 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000 };

static int bink_sles_check( int * freq, int * chans, int * latency_ms, int * poll_ms )
{
  int i;
  int bestfreq = ok_freqs[ FREQS - 1 ];

  // find closest freq
  for( i = 0 ; i < (FREQS-1) ; i++ )
  {
    int nd = ok_freqs[i] - *freq;
    int od = bestfreq - *freq;
    if ( nd < 0 ) nd = -nd;
    if ( od < 0 ) od = -od;
    if ( nd < od )
      bestfreq = ok_freqs[i];
  }

  *freq = bestfreq;
  *latency_ms = START_BUFFERS * MS_PER_BUF;
  *poll_ms = MS_PER_BUF;

  return 1;
}

#include "binkssmix.h"

static audio_funcs funcs = { bink_sles_open, bink_sles_close, bink_sles_ready, bink_sles_write, bink_sles_check };

RADEXPFUNC BINKSNDOPEN RADEXPLINK BinkOpenSLES( UINTa param1, UINTa param2 )
{
  return BinkOpenSSMix( &funcs, (U32)param1, (U32)param2 );
}

