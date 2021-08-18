
Texture2D<unorm float4> src : register( t0 );
RWTexture2D<unorm float4> dst : register( u0 );
RWTexture2D<unorm float> dstY : register( u1 );
RWTexture2D<unorm float> dstU : register( u2 );
RWTexture2D<unorm float> dstV : register( u3 );

float3 rgb2yuv( float3 rgb )
{
  float4 rgba = float4( rgb, 1 );
  float y = dot( rgba, float4( 0.257, 0.504, 0.098, 0.0625 ) );
  float u = dot( rgba, float4( -0.148, -0.291, 0.439, 0.5 ) );
  float v = dot( rgba, float4( 0.439, -0.368, -0.071, 0.5 ) );
  return float3( y, u, v );
}

cbuffer cb : register( b0 )
{
  int2 off;
  int size;
  uint vsize;
};


[numthreads( 32, 2, 1 )]  
void main( uint3 DT : SV_DispatchThreadID )
{
  for ( int y = 0; y < size; ++y )
  {
    for ( int x = 0; x < size; ++x )
    {
      dst[off + DT.xy * size + int2( x, y )]  = src[DT.xy];
    }
  }

  float3 yuv = rgb2yuv( src[DT.xy].rgb );

  {
    for ( uint y = 0; y < vsize; ++y )
    {
      for ( uint x = 0; x < vsize; ++x )
      {
        dstY[DT.xy * vsize + uint2( x, y )] = yuv.r;
      }
    }
  }

  {
    for ( uint y = 0; y < vsize / 2; ++y )
    {
      for ( uint x = 0; x < vsize / 2; ++x )
      {
        dstU[DT.xy * vsize / 2 + uint2( x, y )] = yuv.g;
        dstV[DT.xy * vsize / 2 + uint2( x, y )] = yuv.b;
      }
    }
  }
}
