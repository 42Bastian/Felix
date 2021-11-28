#pragma once

#include "ImageCart.hpp"
class ImageProperties;

class ImageLnx : public ImageCart
{
public:

  struct Header
  {
    std::array<uint8_t, 4>   magic;
    uint16_t                 pageSizeBank0;
    uint16_t                 pageSizeBank1;
    uint16_t                 version;
    std::array<uint8_t, 32>  cartname;
    std::array<uint8_t, 16>  manufname;
    uint8_t                  rotation;
    uint8_t                  audBits;
    uint8_t                  eepromBits;
    std::array<uint8_t, 3>   spare;
  } header{};

  static std::shared_ptr<ImageLnx const> create( std::vector<uint8_t> & data );
  
  ImageLnx( std::vector<uint8_t> data );

  void populate( ImageProperties & imageProperties ) const;

private:
  Header const*const mHeader;
};
